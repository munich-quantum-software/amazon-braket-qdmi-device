/*
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * Licensed under the Apache License v2.0 with LLVM Exceptions (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * https://llvm.org/LICENSE.txt
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations under
 * the License.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

/**
 * @file test_device_unit.cpp
 * @brief Offline unit tests for the Amazon Braket QDMI device.
 *
 * These tests exercise local code paths only, without making any real AWS API
 * calls.
 *
 * Fixtures:
 *  - AmazonBraketQDMIOfflineTest   : session allocated but NOT initialised
 *  - AmazonBraketQDMILocalJobTest  : session initialised with fake credentials
 */

#include "amazon-braket-qdmi-device/Device.hpp"
#include "amazon-braket-qdmi-device/DeviceParser.hpp"
#include "amazon-braket-qdmi-device/Wait.hpp"
#include "amazon-braket-qdmi-device/constants.hpp"
#include "amazon_braket_qdmi/device.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>
#include <limits>
#include <optional>
#include <random>
#include <stdexcept>
#include <stdlib.h> // NOLINT(modernize-deprecated-headers): POSIX setenv/unsetenv
#include <string>
#include <system_error>

struct AMAZON_BRAKET_QDMI_Device_Job_TestAccess {
  static auto
  wait(AMAZON_BRAKET_QDMI_Device_Job job, const size_t timeout,
       const amazon::braket::qdmi::detail::JobWaitFunctions& functions)
      -> QDMI_STATUS {
    job->status_.store(QDMI_JOB_STATUS_RUNNING);
    return job->wait(timeout, functions);
  }
};

namespace {
struct WaitState {
  QDMI_STATUS checkResult = QDMI_SUCCESS;
  std::array<QDMI_Job_Status, 2> checkedStatuses{QDMI_JOB_STATUS_RUNNING,
                                                 QDMI_JOB_STATUS_RUNNING};
  amazon::braket::qdmi::detail::WaitClock::duration elapsed{};
  size_t checkCalls = 0;
  size_t nowCalls = 0;
  size_t sleepCalls = 0;
};

auto makeWaitFunctions(WaitState& state)
    -> amazon::braket::qdmi::detail::JobWaitFunctions {
  return {
      .context = &state,
      .checkStatus =
          [](void* context, QDMI_Job_Status* status) {
            auto* waitState = static_cast<WaitState*>(context);
            const auto statusIndex = waitState->checkCalls == 0U ? 0U : 1U;
            ++waitState->checkCalls;
            *status = waitState->checkedStatuses[statusIndex];
            return waitState->checkResult;
          },
      .now =
          [](void* context) {
            auto* waitState = static_cast<WaitState*>(context);
            ++waitState->nowCalls;
            return amazon::braket::qdmi::detail::WaitClock::time_point{} +
                   (waitState->nowCalls == 1U
                        ? amazon::braket::qdmi::detail::WaitClock::duration{}
                        : waitState->elapsed);
          },
      .sleepFor =
          [](void* context, std::chrono::steady_clock::duration) {
            auto* waitState = static_cast<WaitState*>(context);
            ++waitState->sleepCalls;
          }};
}

TEST(AmazonBraketQDMIWaitTimeoutTest, TimeoutUsesSecondsWithoutNarrowing) {
  using amazon::braket::qdmi::detail::WaitClock;
  using amazon::braket::qdmi::detail::waitTimedOut;

  constexpr WaitClock::time_point start{};
  EXPECT_FALSE(waitTimedOut(start, start + std::chrono::milliseconds{999}, 1U));
  EXPECT_TRUE(waitTimedOut(start, start + std::chrono::seconds{1}, 1U));
  EXPECT_FALSE(waitTimedOut(start, start + std::chrono::seconds{1}, 0U));
  EXPECT_FALSE(waitTimedOut(start, start + std::chrono::seconds{1},
                            std::numeric_limits<size_t>::max()));
}

constexpr const char* BELL_STATE_PROGRAM = "OPENQASM 3.0;\n"
                                           "qubit[2] q;\n"
                                           "bit[2] c;\n"
                                           "h q[0];\n"
                                           "cnot q[0], q[1];\n"
                                           "c[0] = measure q[0];\n"
                                           "c[1] = measure q[1];\n";

static_assert(AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN ==
              QDMI_DEVICE_SESSION_PARAMETER_BASEURL);
static_assert(AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_REGION ==
              QDMI_DEVICE_SESSION_PARAMETER_CUSTOM2);
static_assert(AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_RESERVATION_ARN ==
              QDMI_DEVICE_SESSION_PARAMETER_CUSTOM3);
static_assert(AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3BUCKET ==
              QDMI_DEVICE_JOB_PARAMETER_CUSTOM1);
static_assert(AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3PREFIX ==
              QDMI_DEVICE_JOB_PARAMETER_CUSTOM2);
static_assert(AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_RESERVATION_ARN ==
              QDMI_DEVICE_JOB_PARAMETER_CUSTOM3);

class ScopedTemporaryDirectory {
public:
  ScopedTemporaryDirectory() {
    const auto parent = std::filesystem::temp_directory_path();
    std::random_device random;
    for (size_t attempt = 0; attempt < 100; ++attempt) {
      path_ = parent / ("amazon-braket-qdmi-" + std::to_string(random()) + "-" +
                        std::to_string(attempt));
      std::error_code error;
      if (std::filesystem::create_directory(path_, error)) {
        return;
      }
      if (error && error != std::errc::file_exists) {
        throw std::filesystem::filesystem_error(
            "Failed to create temporary test directory", path_, error);
      }
    }
    throw std::runtime_error(
        "Failed to create a unique temporary test directory");
  }

  ScopedTemporaryDirectory(const ScopedTemporaryDirectory&) = delete;
  auto operator=(const ScopedTemporaryDirectory&)
      -> ScopedTemporaryDirectory& = delete;

  ~ScopedTemporaryDirectory() {
    std::error_code error;
    std::filesystem::remove_all(path_, error);
  }

  [[nodiscard]] auto path() const -> const std::filesystem::path& {
    return path_;
  }

private:
  std::filesystem::path path_;
};

// NOLINTBEGIN(misc-include-cleaner)
class ScopedEnvironment {
public:
  ScopedEnvironment(const char* name, const char* value) : name_(name) {
    if (const char* previous = std::getenv(name); previous != nullptr) {
      previous_ = previous;
    }
#ifdef _WIN32
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
  }

  ~ScopedEnvironment() {
#ifdef _WIN32
    // The Microsoft CRT defines an empty value as removal, so its observable
    // environment has no distinct "present but empty" state.
    _putenv_s(name_.c_str(), previous_.has_value() ? previous_->c_str() : "");
#else
    if (previous_.has_value()) {
      setenv(name_.c_str(), previous_->c_str(), 1);
    } else {
      unsetenv(name_.c_str());
    }
#endif
  }

private:
  std::string name_;
  std::optional<std::string> previous_;
};
// NOLINTEND(misc-include-cleaner)

#ifdef _WIN32
TEST(ScopedEnvironmentTest, TreatsEmptyValueAsAbsent) {
  constexpr auto* variable = "AMAZON_BRAKET_QDMI_TEST_EMPTY_ENVIRONMENT";
  const ScopedEnvironment emptyEnvironment(variable, "");
  ASSERT_EQ(std::getenv(variable), nullptr);
  {
    const ScopedEnvironment temporaryEnvironment(variable, "temporary");
    ASSERT_STREQ(std::getenv(variable), "temporary");
  }
  EXPECT_EQ(std::getenv(variable), nullptr);
}
#else
TEST(ScopedEnvironmentTest, RestoresExistingEmptyValue) {
  constexpr auto* variable = "AMAZON_BRAKET_QDMI_TEST_EMPTY_ENVIRONMENT";
  const ScopedEnvironment emptyEnvironment(variable, "");
  {
    const ScopedEnvironment temporaryEnvironment(variable, "temporary");
    ASSERT_STREQ(std::getenv(variable), "temporary");
  }
  const auto* restored = std::getenv(variable);
  ASSERT_NE(restored, nullptr);
  EXPECT_STREQ(restored, "");
}
#endif
} // namespace

// =============================================================================
// Fixture: allocate-only (never initialised)
// =============================================================================

/**
 * Session that has been allocated but never initialised.
 * No AWS credentials are configured and no network call is ever made.
 */
class AmazonBraketQDMIOfflineTest : public ::testing::Test {
protected:
  AMAZON_BRAKET_QDMI_Device_Session session = nullptr;

  void SetUp() override {
    ASSERT_EQ(AMAZON_BRAKET_QDMI_device_initialize(), QDMI_SUCCESS);
    ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_alloc(&session), QDMI_SUCCESS);
  }

  void TearDown() override {
    if (session != nullptr) {
      AMAZON_BRAKET_QDMI_device_session_free(session);
      session = nullptr;
    }
    AMAZON_BRAKET_QDMI_device_finalize();
  }
};

TEST_F(AmazonBraketQDMIOfflineTest, SessionInitUsesEnvironmentFallbacks) {
  const ScopedTemporaryDirectory temporaryDirectory;
  const auto credentialsFile = temporaryDirectory.path() / "credentials.ini";
  {
    std::ofstream file(credentialsFile);
    ASSERT_TRUE(file.is_open());
    file << "[default]\n"
         << "aws_access_key_id=AKIAIOSFODNN7EXAMPLE\n"
         << "aws_secret_access_key=wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY\n";
  }

  const ScopedEnvironment baseUrl(
      AMAZON_BRAKET_QDMI_DEVICE_ENV_DEVICE_ARN,
      "arn:aws:braket:::device/quantum-simulator/amazon/sv1");
  const ScopedEnvironment authFile(AMAZON_BRAKET_QDMI_DEVICE_ENV_AUTHFILE,
                                   credentialsFile.string().c_str());
  const ScopedEnvironment region(AMAZON_BRAKET_QDMI_DEVICE_ENV_REGION,
                                 "us-east-1");

  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_init(session), QDMI_SUCCESS);
}

// =============================================================================
// Fixture: initialised with fake credentials (no network calls in tests)
// =============================================================================

/**
 * Session initialised with placeholder credentials.
 * init() creates the BraketClient object but makes no outbound API call.
 * Any test that calls queryDeviceProperty() would then hit AWS, so these
 * tests must only exercise local state machine and argument-validation paths.
 */
class AmazonBraketQDMILocalJobTest : public ::testing::Test {
protected:
  AMAZON_BRAKET_QDMI_Device_Session session = nullptr;

  void SetUp() override {
    ASSERT_EQ(AMAZON_BRAKET_QDMI_device_initialize(), QDMI_SUCCESS);
    ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_alloc(&session), QDMI_SUCCESS);

    // Fake but syntactically valid credentials — init() accepts them and builds
    // a BraketClient, but no AWS API call is issued at this stage.
    const char* accessKey = "AKIAIOSFODNN7EXAMPLE";
    ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                  session, QDMI_DEVICE_SESSION_PARAMETER_USERNAME,
                  strlen(accessKey) + 1, accessKey),
              QDMI_SUCCESS);

    const char* secretKey = "wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY";
    ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                  session, QDMI_DEVICE_SESSION_PARAMETER_PASSWORD,
                  strlen(secretKey) + 1, secretKey),
              QDMI_SUCCESS);

    const char* deviceArn =
        "arn:aws:braket:::device/quantum-simulator/amazon/sv1";
    ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                  session,
                  AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN,
                  strlen(deviceArn) + 1, deviceArn),
              QDMI_SUCCESS);

    ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_init(session), QDMI_SUCCESS);
  }

  void TearDown() override {
    if (session != nullptr) {
      AMAZON_BRAKET_QDMI_device_session_free(session);
      session = nullptr;
    }
    AMAZON_BRAKET_QDMI_device_finalize();
  }
};

// =============================================================================
// AmazonBraketQDMIOfflineTest — session init() error paths
// =============================================================================

// init() without setting a device ARN must fail.
TEST_F(AmazonBraketQDMIOfflineTest, SessionInitNoDeviceArn) {
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_init(session),
            QDMI_ERROR_INVALIDARGUMENT);
}

// init() without explicit credentials constructs the AWS SDK default credential
// provider chain. Client construction is offline and must not require the chain
// to resolve credentials eagerly.
TEST_F(AmazonBraketQDMIOfflineTest,
       SessionInitUsesDefaultCredentialProviderChain) {
  const char* deviceArn =
      "arn:aws:braket:us-east-1::device/qpu/test/FakeDevice";
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                session, AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN,
                strlen(deviceArn) + 1, deviceArn),
            QDMI_SUCCESS);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_init(session), QDMI_SUCCESS);
}

TEST_F(AmazonBraketQDMIOfflineTest,
       SessionInitRejectsAccessKeyWithoutSecretKey) {
  const char* deviceArn =
      "arn:aws:braket:us-east-1::device/qpu/test/FakeDevice";
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                session, AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN,
                strlen(deviceArn) + 1, deviceArn),
            QDMI_SUCCESS);
  const char* accessKey = "AKIAIOSFODNN7EXAMPLE";
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                session, QDMI_DEVICE_SESSION_PARAMETER_USERNAME,
                strlen(accessKey) + 1, accessKey),
            QDMI_SUCCESS);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_init(session),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AmazonBraketQDMIOfflineTest,
       SessionInitRejectsSecretKeyWithoutAccessKey) {
  const char* deviceArn =
      "arn:aws:braket:us-east-1::device/qpu/test/FakeDevice";
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                session, AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN,
                strlen(deviceArn) + 1, deviceArn),
            QDMI_SUCCESS);
  const char* secretKey = "wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY";
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                session, QDMI_DEVICE_SESSION_PARAMETER_PASSWORD,
                strlen(secretKey) + 1, secretKey),
            QDMI_SUCCESS);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_init(session),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AmazonBraketQDMIOfflineTest,
       SessionInitRejectsTokenWithoutAccessAndSecretKeys) {
  const char* deviceArn =
      "arn:aws:braket:us-east-1::device/qpu/test/FakeDevice";
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                session, AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN,
                strlen(deviceArn) + 1, deviceArn),
            QDMI_SUCCESS);
  const char* sessionToken = "FakeSessionToken123";
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                session, QDMI_DEVICE_SESSION_PARAMETER_TOKEN,
                strlen(sessionToken) + 1, sessionToken),
            QDMI_SUCCESS);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_init(session),
            QDMI_ERROR_INVALIDARGUMENT);
}

// Explicit API credentials must take precedence over an AUTHFILE fallback.
TEST_F(AmazonBraketQDMIOfflineTest,
       SessionInitDirectCredentialsIgnoreEnvironmentAuthFile) {
  const ScopedEnvironment authFile(AMAZON_BRAKET_QDMI_DEVICE_ENV_AUTHFILE,
                                   "/nonexistent/environment/credentials.ini");
  const char* deviceArn =
      "arn:aws:braket:::device/quantum-simulator/amazon/sv1";
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                session, QDMI_DEVICE_SESSION_PARAMETER_BASEURL,
                strlen(deviceArn) + 1, deviceArn),
            QDMI_SUCCESS);
  const char* accessKey = "AKIAIOSFODNN7EXAMPLE";
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                session, QDMI_DEVICE_SESSION_PARAMETER_USERNAME,
                strlen(accessKey) + 1, accessKey),
            QDMI_SUCCESS);
  const char* secretKey = "wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY";
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                session, QDMI_DEVICE_SESSION_PARAMETER_PASSWORD,
                strlen(secretKey) + 1, secretKey),
            QDMI_SUCCESS);

  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_init(session), QDMI_SUCCESS);
}

// Any explicit direct-credential parameter must suppress an AUTHFILE fallback,
// including a token that is invalid without its access and secret keys.
TEST_F(AmazonBraketQDMIOfflineTest,
       SessionInitTokenOnlyIgnoresEnvironmentAuthFile) {
  const ScopedTemporaryDirectory temporaryDirectory;
  const auto credentialsFile = temporaryDirectory.path() / "credentials.ini";
  {
    std::ofstream file(credentialsFile);
    ASSERT_TRUE(file.is_open());
    file << "[default]\n"
         << "aws_access_key_id=AKIAIOSFODNN7EXAMPLE\n"
         << "aws_secret_access_key=wJalrXUtnFEMI/K7MDENG/"
            "bPxRfiCYEXAMPLEKEY\n";
  }
  const auto credentialsPath = credentialsFile.string();
  const ScopedEnvironment authFile(AMAZON_BRAKET_QDMI_DEVICE_ENV_AUTHFILE,
                                   credentialsPath.c_str());

  const char* deviceArn =
      "arn:aws:braket:::device/quantum-simulator/amazon/sv1";
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                session, QDMI_DEVICE_SESSION_PARAMETER_BASEURL,
                strlen(deviceArn) + 1, deviceArn),
            QDMI_SUCCESS);
  const char* sessionToken = "FakeSessionToken123";
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                session, QDMI_DEVICE_SESSION_PARAMETER_TOKEN,
                strlen(sessionToken) + 1, sessionToken),
            QDMI_SUCCESS);

  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_init(session),
            QDMI_ERROR_INVALIDARGUMENT);
}

// init() with a credentials file that does not exist must fail.
TEST_F(AmazonBraketQDMIOfflineTest, SessionInitNonexistentCredentialsFile) {
  const char* deviceArn =
      "arn:aws:braket:us-east-1::device/qpu/test/FakeDevice";
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                session, AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN,
                strlen(deviceArn) + 1, deviceArn),
            QDMI_SUCCESS);
  const char* badFile = "/nonexistent/path/to/credentials.ini";
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                session, QDMI_DEVICE_SESSION_PARAMETER_AUTHFILE,
                strlen(badFile) + 1, badFile),
            QDMI_SUCCESS);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_init(session),
            QDMI_ERROR_INVALIDARGUMENT);
}

// An explicitly configured credentials file remains authoritative and malformed
// files must not fall back to the default credential provider chain.
TEST_F(AmazonBraketQDMIOfflineTest, SessionInitMalformedCredentialsFile) {
  const ScopedTemporaryDirectory temporaryDirectory;
  const auto credentialsFile =
      temporaryDirectory.path() / "malformed-credentials.ini";
  {
    std::ofstream file(credentialsFile);
    file << "[default]\n"
         << "aws_access_key_id=AKIAIOSFODNN7EXAMPLE\n";
  }
  const auto credentialsPath = credentialsFile.string();

  const char* deviceArn =
      "arn:aws:braket:us-east-1::device/qpu/test/FakeDevice";
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                session, AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN,
                strlen(deviceArn) + 1, deviceArn),
            QDMI_SUCCESS);
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                session, QDMI_DEVICE_SESSION_PARAMETER_AUTHFILE,
                credentialsPath.size() + 1, credentialsPath.c_str()),
            QDMI_SUCCESS);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_init(session),
            QDMI_ERROR_INVALIDARGUMENT);
}

// =============================================================================
// AmazonBraketQDMIOfflineTest — session C-API null-pointer guards
// =============================================================================

// alloc(nullptr) must return INVALIDARGUMENT.
TEST_F(AmazonBraketQDMIOfflineTest, SessionAllocNullptr) {
  AMAZON_BRAKET_QDMI_Device_Session* nullSessionOut = nullptr;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_alloc(nullSessionOut),
            QDMI_ERROR_INVALIDARGUMENT);
}

// init(nullptr) must return INVALIDARGUMENT.
TEST_F(AmazonBraketQDMIOfflineTest, SessionInitNullptr) {
  AMAZON_BRAKET_QDMI_Device_Session nullSession = nullptr;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_init(nullSession),
            QDMI_ERROR_INVALIDARGUMENT);
}

// job API functions with a null job handle must all return INVALIDARGUMENT.
TEST_F(AmazonBraketQDMIOfflineTest, JobSetParameterNullptr) {
  AMAZON_BRAKET_QDMI_Device_Job nullJob = nullptr;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                nullJob, QDMI_DEVICE_JOB_PARAMETER_MAX, 0, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AmazonBraketQDMIOfflineTest, JobQueryPropertyNullptr) {
  AMAZON_BRAKET_QDMI_Device_Job nullJob = nullptr;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_query_property(
                nullJob, QDMI_DEVICE_JOB_PROPERTY_MAX, 0, nullptr, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AmazonBraketQDMIOfflineTest, JobSubmitNullptr) {
  AMAZON_BRAKET_QDMI_Device_Job nullJob = nullptr;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_submit(nullJob),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AmazonBraketQDMIOfflineTest, JobCancelNullptr) {
  AMAZON_BRAKET_QDMI_Device_Job nullJob = nullptr;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_cancel(nullJob),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AmazonBraketQDMIOfflineTest, JobCheckNullptr) {
  AMAZON_BRAKET_QDMI_Device_Job nullJob = nullptr;
  QDMI_Job_Status* nullStatusOut = nullptr;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_check(nullJob, nullStatusOut),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AmazonBraketQDMIOfflineTest, JobWaitNullptr) {
  AMAZON_BRAKET_QDMI_Device_Job nullJob = nullptr;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_wait(nullJob, 0),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AmazonBraketQDMIOfflineTest, JobGetResultsNullptr) {
  AMAZON_BRAKET_QDMI_Device_Job nullJob = nullptr;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_get_results(
                nullJob, QDMI_JOB_RESULT_MAX, 0, nullptr, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
}

// =============================================================================
// AmazonBraketQDMIOfflineTest — setParameter() null-termination guards
// =============================================================================

// Each string parameter requires a null-terminated value; the library checks
// via memchr(). A byte array without a null terminator triggers
// INVALIDARGUMENT.

TEST_F(AmazonBraketQDMIOfflineTest,
       SessionSetParameterDeviceArnNotNullTerminated) {
  const std::array<char, 7> notTerminated = {'a', 'r', 'n', ':', 'a', 'w', 's'};
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                session, AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN,
                notTerminated.size(), notTerminated.data()),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AmazonBraketQDMIOfflineTest,
       SessionSetParameterRegionNotNullTerminated) {
  const std::array<char, 7> notTerminated = {'u', 's', '-', 'e', 'a', 's', 't'};
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                session, AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_REGION,
                notTerminated.size(), notTerminated.data()),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AmazonBraketQDMIOfflineTest,
       SessionSetParameterAuthfileNotNullTerminated) {
  const std::array<char, 7> notTerminated = {'/', 't', 'm', 'p', '/', 'c', 'r'};
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                session, QDMI_DEVICE_SESSION_PARAMETER_AUTHFILE,
                notTerminated.size(), notTerminated.data()),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AmazonBraketQDMIOfflineTest,
       SessionSetParameterAccessKeyNotNullTerminated) {
  const std::array<char, 7> notTerminated = {'A', 'K', 'I', 'A', '1', '2', '3'};
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                session, QDMI_DEVICE_SESSION_PARAMETER_USERNAME,
                notTerminated.size(), notTerminated.data()),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AmazonBraketQDMIOfflineTest,
       SessionSetParameterSecretKeyNotNullTerminated) {
  const std::array<char, 6> notTerminated = {'s', 'e', 'c', 'r', 'e', 't'};
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                session, QDMI_DEVICE_SESSION_PARAMETER_PASSWORD,
                notTerminated.size(), notTerminated.data()),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AmazonBraketQDMIOfflineTest,
       SessionSetParameterSessionTokenNotNullTerminated) {
  const std::array<char, 5> notTerminated = {'t', 'o', 'k', 'e', 'n'};
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                session, QDMI_DEVICE_SESSION_PARAMETER_TOKEN,
                notTerminated.size(), notTerminated.data()),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AmazonBraketQDMIOfflineTest,
       SessionSetParameterReservationArnNotNullTerminated) {
  const std::array<char, 7> notTerminated = {'a', 'r', 'n', ':', 'a', 'w', 's'};
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                session,
                AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_RESERVATION_ARN,
                notTerminated.size(), notTerminated.data()),
            QDMI_ERROR_INVALIDARGUMENT);
}

// Region is optional and must be accepted without error when valid.
TEST_F(AmazonBraketQDMIOfflineTest, SessionSetParameterRegionValid) {
  const char* region = "eu-north-1";
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                session, AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_REGION,
                strlen(region) + 1, region),
            QDMI_SUCCESS);
}

TEST_F(AmazonBraketQDMIOfflineTest, SessionSetParameterReservationArnValid) {
  const char* reservationArn =
      "arn:aws:braket:us-east-1:123456789012:reservation/"
      "a1b2c3d4-5678-90ab-cdef-1234567890ab";
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                session,
                AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_RESERVATION_ARN,
                strlen(reservationArn) + 1, reservationArn),
            QDMI_SUCCESS);
}

// =============================================================================
// AmazonBraketQDMILocalJobTest — session-level state and parameter validation
// =============================================================================

// Re-calling init() on an already-initialised session must return BADSTATE.
TEST_F(AmazonBraketQDMILocalJobTest, SessionInitBadState) {
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_init(session),
            QDMI_ERROR_BADSTATE);
}

// Calling set_parameter() after init() returns BADSTATE; unsupported parameters
// return NOTSUPPORTED or INVALIDARGUMENT on an uninitialized session.
TEST_F(AmazonBraketQDMILocalJobTest, SessionSetParameter) {
  AMAZON_BRAKET_QDMI_Device_Session uninitializedSession = nullptr;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_alloc(&uninitializedSession),
            QDMI_SUCCESS);
  EXPECT_THAT(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                  uninitializedSession,
                  AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN, 20,
                  "https://example.com"),
              testing::AnyOf(QDMI_SUCCESS, QDMI_ERROR_NOTSUPPORTED,
                             QDMI_ERROR_INVALIDARGUMENT));
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                session, AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN,
                20, "https://example.com"),
            QDMI_ERROR_BADSTATE);
  const char* reservationArn =
      "arn:aws:braket:us-east-1:123456789012:reservation/"
      "a1b2c3d4-5678-90ab-cdef-1234567890ab";
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                session,
                AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_RESERVATION_ARN,
                strlen(reservationArn) + 1, reservationArn),
            QDMI_ERROR_BADSTATE);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                session, QDMI_DEVICE_SESSION_PARAMETER_MAX, 0, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
  AMAZON_BRAKET_QDMI_device_session_free(uninitializedSession);
}

// A valid AUTHFILE takes precedence over direct parameters, including an
// otherwise incomplete direct credential configuration.
TEST_F(AmazonBraketQDMIOfflineTest,
       SessionCredentialsFileTakesPrecedenceOverDirectCredentials) {
  const ScopedTemporaryDirectory temporaryDirectory;
  const auto credentialsFile =
      temporaryDirectory.path() / "precedence-credentials.ini";
  {
    std::ofstream file(credentialsFile);
    file << "[default]\n"
         << "aws_access_key_id=AKIAIOSFODNN7EXAMPLE\n"
         << "aws_secret_access_key="
            "wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY\n";
  }
  const auto credentialsPath = credentialsFile.string();

  const char* incompleteAccessKey = "INCOMPLETE_DIRECT_ACCESS_KEY";
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                session, QDMI_DEVICE_SESSION_PARAMETER_USERNAME,
                strlen(incompleteAccessKey) + 1, incompleteAccessKey),
            QDMI_SUCCESS);
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                session, QDMI_DEVICE_SESSION_PARAMETER_AUTHFILE,
                credentialsPath.size() + 1, credentialsPath.c_str()),
            QDMI_SUCCESS);

  const char* deviceArn =
      "arn:aws:braket:::device/quantum-simulator/amazon/sv1";
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                session, AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN,
                strlen(deviceArn) + 1, deviceArn),
            QDMI_SUCCESS);

  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_init(session), QDMI_SUCCESS);
}

// A credentials file that contains more than one profile section triggers a
// warning and uses the first profile's credentials.
// This covers the multi-profile warning branch in parseCredentialsFile().
TEST_F(AmazonBraketQDMIOfflineTest,
       SessionInitMultipleProfilesCredentialsFile) {
  const ScopedTemporaryDirectory temporaryDirectory;
  const auto credentialsFile =
      temporaryDirectory.path() / "multiple-profile-credentials.ini";
  {
    std::ofstream f(credentialsFile);
    f << "[default]\n"
      << "aws_access_key_id=AKIAIOSFODNN7EXAMPLE\n"
      << "aws_secret_access_key=wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY\n"
      << "[profile2]\n"
      << "aws_access_key_id=AKIAI44QH8DHBEXAMPLE\n"
      << "aws_secret_access_key=je7MtGbClwBF/2Zp9Utk/h3yCo8nvbEXAMPLEKEY\n";
  }
  const auto credentialsPath = credentialsFile.string();

  const char* deviceArn =
      "arn:aws:braket:::device/quantum-simulator/amazon/sv1";
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                session, AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN,
                strlen(deviceArn) + 1, deviceArn),
            QDMI_SUCCESS);
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                session, QDMI_DEVICE_SESSION_PARAMETER_AUTHFILE,
                credentialsPath.size() + 1, credentialsPath.c_str()),
            QDMI_SUCCESS);

  // init() parses the two-profile file (triggering the warning), then builds
  // a BraketClient with the first profile's fake credentials. Client
  // construction does not issue an AWS request.
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_init(session), QDMI_SUCCESS);
}

// Verify that a complete direct credential set constructs the client offline.
TEST_F(AmazonBraketQDMIOfflineTest, SessionInitWithDirectCredentials) {
  const char* accessKey = "AKIAIOSFODNN7EXAMPLE";
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                session, QDMI_DEVICE_SESSION_PARAMETER_USERNAME,
                strlen(accessKey) + 1, accessKey),
            QDMI_SUCCESS);

  const char* secretKey = "wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY";
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                session, QDMI_DEVICE_SESSION_PARAMETER_PASSWORD,
                strlen(secretKey) + 1, secretKey),
            QDMI_SUCCESS);

  const char* sessionToken = "FakeSessionToken123";
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                session, QDMI_DEVICE_SESSION_PARAMETER_TOKEN,
                strlen(sessionToken) + 1, sessionToken),
            QDMI_SUCCESS);

  const char* deviceArn =
      "arn:aws:braket:::device/quantum-simulator/amazon/sv1";
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                session, AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN,
                strlen(deviceArn) + 1, deviceArn),
            QDMI_SUCCESS);

  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_init(session), QDMI_SUCCESS);
}

// =============================================================================
// AmazonBraketQDMILocalJobTest — job creation argument validation
// =============================================================================

TEST_F(AmazonBraketQDMILocalJobTest, JobCreate) {
  AMAZON_BRAKET_QDMI_Device_Session uninitializedSession = nullptr;
  AMAZON_BRAKET_QDMI_Device_Job job = nullptr;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_alloc(&uninitializedSession),
            QDMI_SUCCESS);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_create_device_job(
                uninitializedSession, &job),
            QDMI_ERROR_BADSTATE);
  EXPECT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, nullptr),
      QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_create_device_job(nullptr, &job),
            QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &job),
            QDMI_SUCCESS);
  AMAZON_BRAKET_QDMI_device_job_free(job);
  AMAZON_BRAKET_QDMI_device_session_free(uninitializedSession);
}

// =============================================================================
// AmazonBraketQDMILocalJobTest — job setParameter() validation
// =============================================================================

TEST_F(AmazonBraketQDMILocalJobTest, JobSetParameterMaxReturnsInvalidArgument) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  QDMI_Program_Format value = QDMI_PROGRAM_FORMAT_QASM3;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                freshJob, QDMI_DEVICE_JOB_PARAMETER_PROGRAMFORMAT,
                sizeof(value), &value),
            QDMI_SUCCESS);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                freshJob, QDMI_DEVICE_JOB_PARAMETER_MAX, 0, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

TEST_F(AmazonBraketQDMILocalJobTest, JobSetParameterProgram) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                freshJob, QDMI_DEVICE_JOB_PARAMETER_PROGRAM,
                strlen(BELL_STATE_PROGRAM) + 1, BELL_STATE_PROGRAM),
            QDMI_SUCCESS);
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

TEST_F(AmazonBraketQDMILocalJobTest, JobSetParameterS3Bucket) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  const char* s3Bucket = "test-job-specific-results-bucket";
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                freshJob,
                AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3BUCKET,
                strlen(s3Bucket) + 1, s3Bucket),
            QDMI_SUCCESS);
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

TEST_F(AmazonBraketQDMILocalJobTest, JobSetParameterS3Prefix) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  const char* s3Prefix = "my-experiment/run-42/";
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                freshJob,
                AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3PREFIX,
                strlen(s3Prefix) + 1, s3Prefix),
            QDMI_SUCCESS);
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

TEST_F(AmazonBraketQDMILocalJobTest, JobSetParameterReservationArn) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  const char* reservationArn =
      "arn:aws:braket:us-east-1:123456789012:reservation/"
      "a1b2c3d4-5678-90ab-cdef-1234567890ab";
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                freshJob,
                AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_RESERVATION_ARN,
                strlen(reservationArn) + 1, reservationArn),
            QDMI_SUCCESS);
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

TEST_F(AmazonBraketQDMILocalJobTest,
       JobSetParameterReservationArnInvalidArgument) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                freshJob,
                AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_RESERVATION_ARN, 0,
                nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

TEST_F(AmazonBraketQDMILocalJobTest, JobSetParameterS3InvalidArgument) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                freshJob,
                AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3BUCKET, 0,
                nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

// SHOTSNUM with wrong size → INVALIDARGUMENT (must equal sizeof(size_t)).
TEST_F(AmazonBraketQDMILocalJobTest, JobSetParameterShotsWrongSize) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  size_t shots = 100;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                freshJob, QDMI_DEVICE_JOB_PARAMETER_SHOTSNUM, 1, &shots),
            QDMI_ERROR_INVALIDARGUMENT);
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

// An unsupported program format value must be rejected with NOTSUPPORTED.
TEST_F(AmazonBraketQDMILocalJobTest, JobSetParameterUnsupportedProgramFormat) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  const auto unsupported = static_cast<QDMI_Program_Format>(0xFF);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                freshJob, QDMI_DEVICE_JOB_PARAMETER_PROGRAMFORMAT,
                sizeof(unsupported), &unsupported),
            QDMI_ERROR_NOTSUPPORTED);
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

// QASM2 is a valid program format and must be accepted.
TEST_F(AmazonBraketQDMILocalJobTest, JobSetParameterQASM2Format) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  const QDMI_Program_Format fmt = QDMI_PROGRAM_FORMAT_QASM2;
  EXPECT_EQ(
      AMAZON_BRAKET_QDMI_device_job_set_parameter(
          freshJob, QDMI_DEVICE_JOB_PARAMETER_PROGRAMFORMAT, sizeof(fmt), &fmt),
      QDMI_SUCCESS);
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

// value != nullptr with size == 0 must return INVALIDARGUMENT.
TEST_F(AmazonBraketQDMILocalJobTest, JobSetParameterValueNonNullZeroSize) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  const QDMI_Program_Format fmt = QDMI_PROGRAM_FORMAT_QASM3;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                freshJob, QDMI_DEVICE_JOB_PARAMETER_PROGRAMFORMAT,
                /*size=*/0, &fmt),
            QDMI_ERROR_INVALIDARGUMENT);
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

// Custom string parameters must have a null-terminator at buf[size-1].

TEST_F(AmazonBraketQDMILocalJobTest, JobSetParameterS3BucketNotNullTerminated) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  const std::array<char, 6> notTerminated = {'b', 'u', 'c', 'k', 'e', 't'};
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                freshJob,
                AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3BUCKET,
                notTerminated.size(), notTerminated.data()),
            QDMI_ERROR_INVALIDARGUMENT);
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

TEST_F(AmazonBraketQDMILocalJobTest, JobSetParameterS3PrefixNotNullTerminated) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  const std::array<char, 6> notTerminated = {'p', 'r', 'e', 'f', 'i', 'x'};
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                freshJob,
                AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3PREFIX,
                notTerminated.size(), notTerminated.data()),
            QDMI_ERROR_INVALIDARGUMENT);
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

TEST_F(AmazonBraketQDMILocalJobTest,
       JobSetParameterReservationArnNotNullTerminated) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  const std::array<char, 7> notTerminated = {'a', 'r', 'n', ':', 'a', 'w', 's'};
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                freshJob,
                AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_RESERVATION_ARN,
                notTerminated.size(), notTerminated.data()),
            QDMI_ERROR_INVALIDARGUMENT);
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

// =============================================================================
// AmazonBraketQDMILocalJobTest — job queryProperty()
// =============================================================================

// Two-step (size then value) query of the PROGRAM property.
TEST_F(AmazonBraketQDMILocalJobTest, JobQueryPropertyProgram) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                freshJob, QDMI_DEVICE_JOB_PARAMETER_PROGRAM,
                strlen(BELL_STATE_PROGRAM) + 1, BELL_STATE_PROGRAM),
            QDMI_SUCCESS);

  size_t programSize = 0;
  EXPECT_EQ(
      AMAZON_BRAKET_QDMI_device_job_query_property(
          freshJob, QDMI_DEVICE_JOB_PROPERTY_PROGRAM, 0, nullptr, &programSize),
      QDMI_SUCCESS);
  ASSERT_GT(programSize, 0U);

  std::string programBuf(programSize - 1, '\0');
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_query_property(
                freshJob, QDMI_DEVICE_JOB_PROPERTY_PROGRAM, programSize,
                programBuf.data(), nullptr),
            QDMI_SUCCESS);
  EXPECT_EQ(programBuf, BELL_STATE_PROGRAM);

  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

// PROGRAMFORMAT defaults to QASM3.
TEST_F(AmazonBraketQDMILocalJobTest, JobQueryPropertyProgramFormat) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  QDMI_Program_Format fmt = QDMI_PROGRAM_FORMAT_QASM2; // sentinel, overwritten
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_query_property(
                freshJob, QDMI_DEVICE_JOB_PROPERTY_PROGRAMFORMAT,
                sizeof(QDMI_Program_Format), &fmt, nullptr),
            QDMI_SUCCESS);
  EXPECT_EQ(fmt, QDMI_PROGRAM_FORMAT_QASM3);
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

// SHOTSNUM defaults to 100.
TEST_F(AmazonBraketQDMILocalJobTest, JobQueryPropertyShotsNum) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  size_t shots = 0;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_query_property(
                freshJob, QDMI_DEVICE_JOB_PROPERTY_SHOTSNUM, sizeof(size_t),
                &shots, nullptr),
            QDMI_SUCCESS);
  EXPECT_EQ(shots, 100U);
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

// MAX property sentinel must return INVALIDARGUMENT.
TEST_F(AmazonBraketQDMILocalJobTest, JobQueryPropertyMax) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_query_property(
                freshJob, QDMI_DEVICE_JOB_PROPERTY_MAX, 0, nullptr, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

// =============================================================================
// AmazonBraketQDMILocalJobTest — job cancel() state machine
// =============================================================================

// cancel() before submission: transitions to CANCELED locally (no AWS call).
TEST_F(AmazonBraketQDMILocalJobTest, JobCancelFromCreatedState) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_cancel(freshJob), QDMI_SUCCESS);

  QDMI_Job_Status status = QDMI_JOB_STATUS_CREATED;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_check(freshJob, &status),
            QDMI_SUCCESS);
  EXPECT_EQ(status, QDMI_JOB_STATUS_CANCELED);

  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

// cancel() on an already-CANCELED job → INVALIDARGUMENT (not QUEUED/RUNNING).
TEST_F(AmazonBraketQDMILocalJobTest, JobCancelFromCanceledState) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_job_cancel(freshJob), QDMI_SUCCESS);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_cancel(freshJob),
            QDMI_ERROR_INVALIDARGUMENT);
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

// =============================================================================
// AmazonBraketQDMILocalJobTest — job check() state machine
// =============================================================================

// check() on a fresh job (no taskArn) returns the current local status.
TEST_F(AmazonBraketQDMILocalJobTest, JobCheckNoTaskArn) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  QDMI_Job_Status status = QDMI_JOB_STATUS_DONE; // sentinel
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_check(freshJob, &status),
            QDMI_SUCCESS);
  EXPECT_EQ(status, QDMI_JOB_STATUS_CREATED);
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

// =============================================================================
// AmazonBraketQDMILocalJobTest — job wait() state machine
// =============================================================================

// wait() on a CREATED (never submitted) job → INVALIDARGUMENT.
TEST_F(AmazonBraketQDMILocalJobTest, JobWaitOnCreatedJob) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_wait(freshJob, 1),
            QDMI_ERROR_INVALIDARGUMENT);
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

// wait() on a CANCELED (terminal) job → QDMI_SUCCESS immediately.
TEST_F(AmazonBraketQDMILocalJobTest, JobWaitOnCanceledJob) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_job_cancel(freshJob), QDMI_SUCCESS);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_wait(freshJob, 1), QDMI_SUCCESS);
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

// wait() on a FAILED (terminal) job → QDMI_SUCCESS immediately.
TEST_F(AmazonBraketQDMILocalJobTest, JobWaitOnFailedJob) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                freshJob, QDMI_DEVICE_JOB_PARAMETER_PROGRAM,
                strlen(BELL_STATE_PROGRAM) + 1, BELL_STATE_PROGRAM),
            QDMI_SUCCESS);
  // No S3 bucket → submit() transitions status to FAILED.
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_job_submit(freshJob),
            QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_wait(freshJob, 1), QDMI_SUCCESS);
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

TEST_F(AmazonBraketQDMILocalJobTest, JobWaitTimesOutDeterministically) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);

  WaitState state;
  state.elapsed = std::chrono::seconds{1};
  const auto functions = makeWaitFunctions(state);

  EXPECT_EQ(
      AMAZON_BRAKET_QDMI_Device_Job_TestAccess::wait(freshJob, 1U, functions),
      QDMI_ERROR_TIMEOUT);
  EXPECT_EQ(state.checkCalls, 1U);
  EXPECT_EQ(state.nowCalls, 2U);
  EXPECT_EQ(state.sleepCalls, 0U);
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

TEST_F(AmazonBraketQDMILocalJobTest, JobWaitCompletesAfterPolling) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);

  WaitState state;
  state.checkedStatuses[1] = QDMI_JOB_STATUS_DONE;
  const auto functions = makeWaitFunctions(state);

  EXPECT_EQ(
      AMAZON_BRAKET_QDMI_Device_Job_TestAccess::wait(freshJob, 1U, functions),
      QDMI_SUCCESS);
  EXPECT_EQ(state.checkCalls, 2U);
  EXPECT_EQ(state.nowCalls, 2U);
  EXPECT_EQ(state.sleepCalls, 1U);
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

TEST_F(AmazonBraketQDMILocalJobTest, JobWaitPropagatesCheckFailure) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);

  WaitState state;
  state.checkResult = QDMI_ERROR_NOTSUPPORTED;
  const auto functions = makeWaitFunctions(state);

  EXPECT_EQ(
      AMAZON_BRAKET_QDMI_Device_Job_TestAccess::wait(freshJob, 1U, functions),
      QDMI_ERROR_NOTSUPPORTED);
  EXPECT_EQ(state.checkCalls, 1U);
  EXPECT_EQ(state.nowCalls, 1U);
  EXPECT_EQ(state.sleepCalls, 0U);
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

// =============================================================================
// AmazonBraketQDMILocalJobTest — job submit() local error paths
// =============================================================================

// submit() without a program set → INVALIDARGUMENT.
TEST_F(AmazonBraketQDMILocalJobTest, JobSubmitNoProgram) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_submit(freshJob),
            QDMI_ERROR_INVALIDARGUMENT);
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

// submit() with a program but no S3 bucket → INVALIDARGUMENT; status becomes
// FAILED (covers the inside-lock empty-bucket check in submit()).
TEST_F(AmazonBraketQDMILocalJobTest, JobSubmitNoS3Bucket) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                freshJob, QDMI_DEVICE_JOB_PARAMETER_PROGRAM,
                strlen(BELL_STATE_PROGRAM) + 1, BELL_STATE_PROGRAM),
            QDMI_SUCCESS);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_submit(freshJob),
            QDMI_ERROR_INVALIDARGUMENT);

  QDMI_Job_Status status = QDMI_JOB_STATUS_CREATED;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_check(freshJob, &status),
            QDMI_SUCCESS);
  EXPECT_EQ(status, QDMI_JOB_STATUS_FAILED);

  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

TEST_F(AmazonBraketQDMILocalJobTest,
       JobSubmitNoS3BucketWithSessionReservationArn) {
  AMAZON_BRAKET_QDMI_Device_Session reservedSession = nullptr;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_alloc(&reservedSession),
            QDMI_SUCCESS);

  const char* accessKey = "AKIAIOSFODNN7EXAMPLE";
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                reservedSession, QDMI_DEVICE_SESSION_PARAMETER_USERNAME,
                strlen(accessKey) + 1, accessKey),
            QDMI_SUCCESS);
  const char* secretKey = "wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY";
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                reservedSession, QDMI_DEVICE_SESSION_PARAMETER_PASSWORD,
                strlen(secretKey) + 1, secretKey),
            QDMI_SUCCESS);
  const char* deviceArn =
      "arn:aws:braket:::device/quantum-simulator/amazon/sv1";
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                reservedSession,
                AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN,
                strlen(deviceArn) + 1, deviceArn),
            QDMI_SUCCESS);
  const char* reservationArn =
      "arn:aws:braket:us-east-1:123456789012:reservation/"
      "a1b2c3d4-5678-90ab-cdef-1234567890ab";
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                reservedSession,
                AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_RESERVATION_ARN,
                strlen(reservationArn) + 1, reservationArn),
            QDMI_SUCCESS);
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_init(reservedSession),
            QDMI_SUCCESS);

  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_create_device_job(reservedSession,
                                                                &freshJob),
            QDMI_SUCCESS);
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                freshJob, QDMI_DEVICE_JOB_PARAMETER_PROGRAM,
                strlen(BELL_STATE_PROGRAM) + 1, BELL_STATE_PROGRAM),
            QDMI_SUCCESS);

  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_submit(freshJob),
            QDMI_ERROR_INVALIDARGUMENT);

  QDMI_Job_Status status = QDMI_JOB_STATUS_CREATED;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_check(freshJob, &status),
            QDMI_SUCCESS);
  EXPECT_EQ(status, QDMI_JOB_STATUS_FAILED);

  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
  AMAZON_BRAKET_QDMI_device_session_free(reservedSession);
}

TEST_F(AmazonBraketQDMILocalJobTest,
       JobSubmitWithBothReservationArnsNoS3BucketFails) {
  AMAZON_BRAKET_QDMI_Device_Session reservedSession = nullptr;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_alloc(&reservedSession),
            QDMI_SUCCESS);

  const char* accessKey = "AKIAIOSFODNN7EXAMPLE";
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                reservedSession, QDMI_DEVICE_SESSION_PARAMETER_USERNAME,
                strlen(accessKey) + 1, accessKey),
            QDMI_SUCCESS);
  const char* secretKey = "wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY";
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                reservedSession, QDMI_DEVICE_SESSION_PARAMETER_PASSWORD,
                strlen(secretKey) + 1, secretKey),
            QDMI_SUCCESS);
  const char* deviceArn =
      "arn:aws:braket:::device/quantum-simulator/amazon/sv1";
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                reservedSession,
                AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN,
                strlen(deviceArn) + 1, deviceArn),
            QDMI_SUCCESS);
  const char* sessionReservationArn =
      "arn:aws:braket:us-east-1:123456789012:reservation/"
      "a1b2c3d4-5678-90ab-cdef-1234567890ab";
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                reservedSession,
                AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_RESERVATION_ARN,
                strlen(sessionReservationArn) + 1, sessionReservationArn),
            QDMI_SUCCESS);
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_init(reservedSession),
            QDMI_SUCCESS);

  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_create_device_job(reservedSession,
                                                                &freshJob),
            QDMI_SUCCESS);
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                freshJob, QDMI_DEVICE_JOB_PARAMETER_PROGRAM,
                strlen(BELL_STATE_PROGRAM) + 1, BELL_STATE_PROGRAM),
            QDMI_SUCCESS);
  const char* jobReservationArn =
      "arn:aws:braket:us-east-1:123456789012:reservation/"
      "b2c3d4e5-6789-0abc-def1-234567890abc";
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                freshJob,
                AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_RESERVATION_ARN,
                strlen(jobReservationArn) + 1, jobReservationArn),
            QDMI_SUCCESS);

  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_submit(freshJob),
            QDMI_ERROR_INVALIDARGUMENT);

  QDMI_Job_Status status = QDMI_JOB_STATUS_CREATED;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_check(freshJob, &status),
            QDMI_SUCCESS);
  EXPECT_EQ(status, QDMI_JOB_STATUS_FAILED);

  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
  AMAZON_BRAKET_QDMI_device_session_free(reservedSession);
}

// =============================================================================
// DeviceParser offline error-path tests
// =============================================================================
//
// Each test drives a concrete parser with deliberately malformed JSON to reach
// the QDMI_ERROR_FATAL returns in SimulatorPropertiesParser and
// IQMDeviceParser. No AWS credentials or network calls are needed.

TEST(DeviceParserOfflineTest, SimulatorMissingParadigm) {
  const SimulatorPropertiesParser parser;
  ParsedDeviceProperties props;
  EXPECT_EQ(parser.ParseProperties(R"({})", props), QDMI_ERROR_FATAL);
}

TEST(DeviceParserOfflineTest, SimulatorParadigmMissingQubitCount) {
  const SimulatorPropertiesParser parser;
  ParsedDeviceProperties props;
  EXPECT_EQ(parser.ParseProperties(R"({"paradigm":{}})", props),
            QDMI_ERROR_FATAL);
}

TEST(DeviceParserOfflineTest, SimulatorMissingAction) {
  const SimulatorPropertiesParser parser;
  ParsedDeviceProperties props;
  EXPECT_EQ(
      parser.ParseProperties(
          R"({"paradigm":{"qubitCount":2,"connectivity":{"fullyConnected":true}}})",
          props),
      QDMI_ERROR_FATAL);
}

TEST(DeviceParserOfflineTest, SimulatorActionMissingOpenQASMProgram) {
  const SimulatorPropertiesParser parser;
  ParsedDeviceProperties props;
  EXPECT_EQ(
      parser.ParseProperties(
          R"({"paradigm":{"qubitCount":2,"connectivity":{"fullyConnected":true}},"action":{}})",
          props),
      QDMI_ERROR_FATAL);
}

TEST(DeviceParserOfflineTest,
     SimulatorOpenQASMProgramMissingSupportedOperations) {
  const SimulatorPropertiesParser parser;
  ParsedDeviceProperties props;
  EXPECT_EQ(
      parser.ParseProperties(
          R"({"paradigm":{"qubitCount":2,"connectivity":{"fullyConnected":true}},"action":{"braket.ir.openqasm.program":{}}})",
          props),
      QDMI_ERROR_FATAL);
}

TEST(DeviceParserOfflineTest, IQMMissingConnectivity) {
  const IQMDeviceParser parser;
  ParsedDeviceProperties props;
  EXPECT_EQ(parser.ParseProperties(R"({"paradigm":{"qubitCount":5}})", props),
            QDMI_ERROR_FATAL);
}

TEST(DeviceParserOfflineTest, IQMConnectivityMissingGraph) {
  const IQMDeviceParser parser;
  ParsedDeviceProperties props;
  EXPECT_EQ(parser.ParseProperties(
                R"({"paradigm":{"qubitCount":5,"connectivity":{}}})", props),
            QDMI_ERROR_FATAL);
}

TEST(DeviceParserOfflineTest, IQMNonNumericQubitID) {
  const IQMDeviceParser parser;
  ParsedDeviceProperties props;
  EXPECT_EQ(
      parser.ParseProperties(
          R"({"paradigm":{"qubitCount":2,"connectivity":{"connectivityGraph":{"QB1":["QB2"]}}}})",
          props),
      QDMI_ERROR_FATAL);
}

// =============================================================================
// AmazonBraketQDMILocalJobTest — job getResults() local error paths
// =============================================================================

// getResults() on a non-DONE job → BADSTATE.
TEST_F(AmazonBraketQDMILocalJobTest, JobGetResultsNotDone) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_get_results(
                freshJob, QDMI_JOB_RESULT_SHOTS, 0, nullptr, nullptr),
            QDMI_ERROR_BADSTATE);
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

// result == QDMI_JOB_RESULT_MAX is checked before the status check →
// INVALIDARGUMENT.
TEST_F(AmazonBraketQDMILocalJobTest, JobGetResultsInvalidResultType) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_get_results(
                freshJob, QDMI_JOB_RESULT_MAX, 0, nullptr, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}
