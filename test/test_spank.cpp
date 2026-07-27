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

#include "amazon-braket-qdmi-device/constants.hpp"
#include "amazon_braket_qdmi/device.h"
#include "spank_test_doubles.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <gtest/gtest.h>
#include <optional>
#include <slurm/slurm_errno.h>
#include <slurm/slurm_version.h>
#include <slurm/spank.h>
// POSIX setenv/unsetenv are declared by this C compatibility header.
#include <stdlib.h> // NOLINT(modernize-deprecated-headers)
#include <string>

// These declarations mirror the required Slurm SPANK entry-point ABI.
// NOLINTBEGIN(readability-identifier-naming, cppcoreguidelines-avoid-c-arrays,
//             modernize-avoid-c-arrays)
extern "C" {
extern const char plugin_name[];
extern const char plugin_type[];
extern const unsigned int plugin_version;
extern const unsigned int spank_plugin_version;
int slurm_spank_init(spank_t, int, char*[]);
int slurm_spank_init_post_opt(spank_t, int, char*[]);
int slurm_spank_user_init(spank_t, int, char*[]);
int slurm_spank_task_init(spank_t, int, char*[]);
}
// NOLINTEND(readability-identifier-naming, cppcoreguidelines-avoid-c-arrays,
//           modernize-avoid-c-arrays)

namespace {

constexpr auto BASE_URL_OPTION = "qdmi-device-session-parameter-baseurl";
constexpr auto AUTH_FILE_OPTION = "qdmi-device-session-parameter-authfile";

class ScopedEnvironment final {
public:
  ScopedEnvironment(const char* name, const char* value) : variableName(name) {
    if (const char* original = std::getenv(name); original != nullptr) {
      originalValue = original;
    }
    if (value == nullptr) {
      (void)unsetenv(name);
    } else {
      (void)setenv(name, value, 1);
    }
  }

  ScopedEnvironment(const ScopedEnvironment&) = delete;
  auto operator=(const ScopedEnvironment&) -> ScopedEnvironment& = delete;
  ScopedEnvironment(ScopedEnvironment&&) = delete;
  auto operator=(ScopedEnvironment&&) -> ScopedEnvironment& = delete;

  ~ScopedEnvironment() {
    if (originalValue.has_value()) {
      (void)setenv(variableName, originalValue->c_str(), 1);
    } else {
      (void)unsetenv(variableName);
    }
  }

private:
  const char* variableName;
  std::optional<std::string> originalValue;
};

class SpankTest : public ::testing::Test {
protected:
  static auto initialize(spank_t spank) -> void {
    ASSERT_EQ(slurm_spank_init(spank, 0, nullptr), 0);
  }

  static auto initializeRemote() -> spank_t {
    auto* spank = spank_test::beginRemote();
    initialize(spank);
    return spank;
  }

  static auto initializeAllocator() -> spank_t {
    auto* spank = spank_test::beginAllocator();
    initialize(spank);
    return spank;
  }

  void SetUp() override { spank_test::reset(); }
};

// GoogleTest owns the fixture objects created by TEST_F.
// NOLINTBEGIN(cppcoreguidelines-owning-memory)
TEST_F(SpankTest, RegistersAllOptions) {
  auto* const spank = initializeAllocator();
  ASSERT_EQ(spank_test::state().registeredOptions.size(), 4);

  const std::array expected = {
      BASE_URL_OPTION,
      "qdmi-device-session-parameter-region",
      "qdmi-device-session-parameter-reservation-arn",
      AUTH_FILE_OPTION,
  };
  for (const auto* name : expected) {
    ASSERT_NE(spank_test::registeredOption(name), nullptr);
  }
  (void)spank;
}

TEST_F(SpankTest, ExportsSlurmPluginMetadata) {
  EXPECT_STREQ(&plugin_name[0], "amazon_braket_qdmi");
  EXPECT_STREQ(&plugin_type[0], "spank");
  EXPECT_EQ(plugin_version, SLURM_VERSION_NUMBER);
  EXPECT_EQ(spank_plugin_version, 1U);
}

TEST_F(SpankTest, RejectsInvalidOptionValues) {
  initializeAllocator();
  auto* option = spank_test::registeredOption(BASE_URL_OPTION);
  ASSERT_NE(option, nullptr);

  EXPECT_NE(option->cb(0, "value", 0), 0);
  EXPECT_NE(option->cb(5, "value", 0), 0);
  EXPECT_NE(option->cb(option->val, nullptr, 0), 0);
  EXPECT_NE(option->cb(option->val, "", 0), 0);
  EXPECT_NE(option->cb(option->val, "value\n", 0), 0);
  EXPECT_NE(option->cb(option->val, "value\r", 0), 0);
  EXPECT_EQ(option->cb(option->val, "valid", 0), 0);
}

TEST_F(SpankTest, AllocatorWithoutOptInIsAccepted) {
  auto* const spank = initializeAllocator();
  EXPECT_EQ(slurm_spank_init_post_opt(spank, 0, nullptr), 0);
  EXPECT_EQ(spank_test::state().initializeCalls, 0);
}

TEST_F(SpankTest, AllocatorRequiresBothOptInOptions) {
  auto* spank = initializeAllocator();
  auto* baseUrl = spank_test::registeredOption(BASE_URL_OPTION);
  auto* authFile = spank_test::registeredOption(AUTH_FILE_OPTION);
  ASSERT_NE(baseUrl, nullptr);
  ASSERT_NE(authFile, nullptr);

  ASSERT_EQ(baseUrl->cb(baseUrl->val, "device", 0), 0);
  EXPECT_NE(slurm_spank_init_post_opt(spank, 0, nullptr), 0);

  spank = initializeAllocator();
  authFile = spank_test::registeredOption(AUTH_FILE_OPTION);
  ASSERT_EQ(authFile->cb(authFile->val, "/tmp/credentials", 0), 0);
  EXPECT_NE(slurm_spank_init_post_opt(spank, 0, nullptr), 0);
}

TEST_F(SpankTest, AllocatorAcceptsCompleteOptInAndOptionalParameters) {
  auto* const spank = initializeAllocator();
  spank_test::configureOptIn();
  ASSERT_EQ(spank_test::registeredOption("qdmi-device-session-parameter-region")
                ->cb(2, "us-east-1", 0),
            0);
  ASSERT_EQ(spank_test::registeredOption(
                "qdmi-device-session-parameter-reservation-arn")
                ->cb(3, "reservation", 0),
            0);
  EXPECT_EQ(slurm_spank_init_post_opt(spank, 0, nullptr), 0);
}

TEST_F(SpankTest, RemoteWithoutOptInSkipsValidation) {
  auto* const spank = initializeRemote();
  ASSERT_EQ(slurm_spank_init_post_opt(spank, 0, nullptr), 0);
  ASSERT_EQ(slurm_spank_user_init(spank, 0, nullptr), 0);
  EXPECT_EQ(slurm_spank_task_init(spank, 0, nullptr), 0);
  EXPECT_EQ(spank_test::state().initializeCalls, 0);
}

TEST_F(SpankTest, RemoteRequiredJobEnvironmentDoesNotOptIn) {
  auto* const spank = initializeRemote();
  spank_test::state().jobEnvironment[AMAZON_BRAKET_QDMI_DEVICE_ENV_BASEURL] =
      "job-device";
  spank_test::state().jobEnvironment[AMAZON_BRAKET_QDMI_DEVICE_ENV_AUTHFILE] =
      "/job/auth";

  ASSERT_EQ(slurm_spank_init_post_opt(spank, 0, nullptr), 0);
  ASSERT_EQ(slurm_spank_user_init(spank, 0, nullptr), 0);
  EXPECT_EQ(slurm_spank_task_init(spank, 0, nullptr), 0);
  EXPECT_EQ(spank_test::state().initializeCalls, 0);
  EXPECT_TRUE(spank_test::state().environment.empty());
}

TEST_F(SpankTest, RemoteCollectionIgnoresRequiredJobEnvironment) {
  auto* const spank = initializeRemote();
  spank_test::state().jobEnvironment[AMAZON_BRAKET_QDMI_DEVICE_ENV_BASEURL] =
      "job-device";
  spank_test::state().jobEnvironment[AMAZON_BRAKET_QDMI_DEVICE_ENV_AUTHFILE] =
      "/job/auth";

  // Isolate user_init's collection path before init_post_opt marks an
  // option-less plugin inactive.
  ASSERT_EQ(slurm_spank_user_init(spank, 0, nullptr), 0);
  EXPECT_LT(slurm_spank_task_init(spank, 0, nullptr), 0);
  EXPECT_EQ(spank_test::state().initializeCalls, 0);
  EXPECT_TRUE(spank_test::state().environment.empty());
}

TEST_F(SpankTest, RemoteIncompleteOptInRejectsJob) {
  auto* const spank = initializeRemote();
  auto* baseUrl = spank_test::registeredOption(BASE_URL_OPTION);
  ASSERT_EQ(baseUrl->cb(baseUrl->val, "device", 0), 0);
  ASSERT_EQ(slurm_spank_init_post_opt(spank, 0, nullptr), 0);
  ASSERT_EQ(slurm_spank_user_init(spank, 0, nullptr), 0);
  EXPECT_LT(slurm_spank_task_init(spank, 0, nullptr), 0);
  EXPECT_EQ(spank_test::state().initializeCalls, 0);
}

TEST_F(SpankTest, RemoteValidationUsesForwardedOptionsAndInjectsEnvironment) {
  auto* const spank = initializeRemote();
  spank_test::configureOptIn();
  ASSERT_EQ(spank_test::registeredOption("qdmi-device-session-parameter-region")
                ->cb(2, "us-east-1", 0),
            0);
  ASSERT_EQ(spank_test::registeredOption(
                "qdmi-device-session-parameter-reservation-arn")
                ->cb(3, "reservation", 0),
            0);
  spank_test::state().forwardedOptions[BASE_URL_OPTION] = "forwarded-device";
  spank_test::state().forwardedOptions[AUTH_FILE_OPTION] = "/forwarded/auth";
  spank_test::state().jobEnvironment[AMAZON_BRAKET_QDMI_DEVICE_ENV_REGION] =
      "job-region";
  spank_test::state()
      .jobEnvironment[AMAZON_BRAKET_QDMI_DEVICE_ENV_RESERVATION_ARN] =
      "job-reservation";
  ASSERT_EQ(slurm_spank_init_post_opt(spank, 0, nullptr), 0);
  ASSERT_EQ(slurm_spank_user_init(spank, 0, nullptr), 0);
  EXPECT_EQ(slurm_spank_user_init(spank, 0, nullptr), 0);
  EXPECT_EQ(slurm_spank_task_init(spank, 0, nullptr), 0);

  EXPECT_EQ(spank_test::state()
                .parameterValues[QDMI_DEVICE_SESSION_PARAMETER_BASEURL],
            "forwarded-device");
  EXPECT_EQ(spank_test::state()
                .parameterValues[QDMI_DEVICE_SESSION_PARAMETER_AUTHFILE],
            "/forwarded/auth");
  EXPECT_EQ(
      spank_test::state().parameterValues[QDMI_DEVICE_SESSION_PARAMETER_REGION],
      "us-east-1");
  EXPECT_EQ(spank_test::state()
                .parameterValues[QDMI_DEVICE_SESSION_PARAMETER_RESERVATION_ARN],
            "reservation");
  EXPECT_EQ(
      spank_test::state().environment[AMAZON_BRAKET_QDMI_DEVICE_ENV_BASEURL],
      "forwarded-device");
  EXPECT_EQ(
      spank_test::state().environment[AMAZON_BRAKET_QDMI_DEVICE_ENV_AUTHFILE],
      "/forwarded/auth");
  EXPECT_EQ(
      spank_test::state().environment[AMAZON_BRAKET_QDMI_DEVICE_ENV_REGION],
      "us-east-1");
  EXPECT_EQ(spank_test::state()
                .environment[AMAZON_BRAKET_QDMI_DEVICE_ENV_RESERVATION_ARN],
            "reservation");
  ASSERT_EQ(spank_test::state().environmentOverwrites.size(), 4);
  for (const auto overwrite : spank_test::state().environmentOverwrites) {
    EXPECT_EQ(overwrite, 1);
  }
  EXPECT_EQ(spank_test::state().initializeCalls, 1);
}

TEST_F(SpankTest, RemoteValidationUsesOptionalJobEnvironment) {
  auto* const spank = initializeRemote();
  spank_test::configureOptIn();
  spank_test::state().jobEnvironment[AMAZON_BRAKET_QDMI_DEVICE_ENV_REGION] =
      "eu-north-1";
  spank_test::state()
      .jobEnvironment[AMAZON_BRAKET_QDMI_DEVICE_ENV_RESERVATION_ARN] =
      "job-reservation";

  ASSERT_EQ(slurm_spank_init_post_opt(spank, 0, nullptr), 0);
  ASSERT_EQ(slurm_spank_user_init(spank, 0, nullptr), 0);
  EXPECT_EQ(slurm_spank_task_init(spank, 0, nullptr), 0);

  EXPECT_EQ(
      spank_test::state().parameterValues[QDMI_DEVICE_SESSION_PARAMETER_REGION],
      "eu-north-1");
  EXPECT_EQ(spank_test::state()
                .parameterValues[QDMI_DEVICE_SESSION_PARAMETER_RESERVATION_ARN],
            "job-reservation");
  EXPECT_EQ(
      spank_test::state().environment[AMAZON_BRAKET_QDMI_DEVICE_ENV_REGION],
      "eu-north-1");
  EXPECT_EQ(spank_test::state()
                .environment[AMAZON_BRAKET_QDMI_DEVICE_ENV_RESERVATION_ARN],
            "job-reservation");
}

TEST_F(SpankTest, RemoteValidationIgnoresDaemonOnlyEnvironment) {
  const ScopedEnvironment baseUrlEnvironment{
      AMAZON_BRAKET_QDMI_DEVICE_ENV_BASEURL, "daemon-device"};
  const ScopedEnvironment authFileEnvironment{
      AMAZON_BRAKET_QDMI_DEVICE_ENV_AUTHFILE, "/daemon/auth"};
  const ScopedEnvironment regionEnvironment{
      AMAZON_BRAKET_QDMI_DEVICE_ENV_REGION, "daemon-region"};
  const ScopedEnvironment reservationEnvironment{
      AMAZON_BRAKET_QDMI_DEVICE_ENV_RESERVATION_ARN, "daemon-reservation"};

  auto* const spank = initializeRemote();
  spank_test::configureOptIn();
  ASSERT_EQ(slurm_spank_init_post_opt(spank, 0, nullptr), 0);
  ASSERT_EQ(slurm_spank_user_init(spank, 0, nullptr), 0);
  EXPECT_EQ(slurm_spank_task_init(spank, 0, nullptr), 0);

  EXPECT_TRUE(spank_test::state().sessionEnvironmentAtInit.empty());
  const auto* restoredBaseUrl =
      std::getenv(AMAZON_BRAKET_QDMI_DEVICE_ENV_BASEURL);
  const auto* restoredAuthFile =
      std::getenv(AMAZON_BRAKET_QDMI_DEVICE_ENV_AUTHFILE);
  const auto* restoredRegion =
      std::getenv(AMAZON_BRAKET_QDMI_DEVICE_ENV_REGION);
  const auto* restoredReservation =
      std::getenv(AMAZON_BRAKET_QDMI_DEVICE_ENV_RESERVATION_ARN);
  ASSERT_NE(restoredBaseUrl, nullptr);
  ASSERT_NE(restoredAuthFile, nullptr);
  ASSERT_NE(restoredRegion, nullptr);
  ASSERT_NE(restoredReservation, nullptr);
  EXPECT_STREQ(restoredBaseUrl, "daemon-device");
  EXPECT_STREQ(restoredAuthFile, "/daemon/auth");
  EXPECT_STREQ(restoredRegion, "daemon-region");
  EXPECT_STREQ(restoredReservation, "daemon-reservation");
}

TEST_F(SpankTest, RemoteAcceptsIdleAndBusyDevices) {
  for (const auto status : {QDMI_DEVICE_STATUS_IDLE, QDMI_DEVICE_STATUS_BUSY}) {
    spank_test::reset();
    auto* const spank = initializeRemote();
    spank_test::configureOptIn();
    spank_test::state().deviceStatus = status;
    ASSERT_EQ(slurm_spank_init_post_opt(spank, 0, nullptr), 0);
    ASSERT_EQ(slurm_spank_user_init(spank, 0, nullptr), 0);
    EXPECT_EQ(slurm_spank_task_init(spank, 0, nullptr), 0);
  }
}

TEST_F(SpankTest, RemoteRejectsUnavailableDevices) {
  for (const auto status :
       {QDMI_DEVICE_STATUS_OFFLINE, QDMI_DEVICE_STATUS_MAINTENANCE}) {
    spank_test::reset();
    auto* const spank = initializeRemote();
    spank_test::configureOptIn();
    spank_test::state().deviceStatus = status;
    ASSERT_EQ(slurm_spank_init_post_opt(spank, 0, nullptr), 0);
    ASSERT_EQ(slurm_spank_user_init(spank, 0, nullptr), 0);
    EXPECT_LT(slurm_spank_task_init(spank, 0, nullptr), 0);
    EXPECT_EQ(spank_test::state().sessionFreeCalls, 1);
    EXPECT_EQ(spank_test::state().finalizeCalls, 1);
  }
}

TEST_F(SpankTest, RemoteValidationFailureLogsOnceAcrossTasks) {
  auto* const spank = initializeRemote();
  spank_test::configureOptIn();
  spank_test::state().sessionInitResult = QDMI_ERROR_PERMISSIONDENIED;
  ASSERT_EQ(slurm_spank_init_post_opt(spank, 0, nullptr), 0);
  ASSERT_EQ(slurm_spank_user_init(spank, 0, nullptr), 0);

  spank_test::state().taskId = 0;
  EXPECT_LT(slurm_spank_task_init(spank, 0, nullptr), 0);
  spank_test::state().taskId = 1;
  EXPECT_LT(slurm_spank_task_init(spank, 0, nullptr), 0);

  EXPECT_EQ(std::ranges::count(
                spank_test::state().logs,
                "amazon-braket-qdmi: job rejected because QDMI validation "
                "failed"),
            1);
}

TEST_F(SpankTest, RemoteQdmiFailuresRejectAndCleanUp) {
  struct Failure {
    const char* name;
    int* result;
    int value;
    bool sessionWasAllocated;
    bool shouldFinalize;
  };

  const std::array failures = {
      Failure{.name = "initialize",
              .result = &spank_test::state().deviceInitializeResult,
              .value = QDMI_ERROR_FATAL,
              .sessionWasAllocated = false,
              .shouldFinalize = false},
      Failure{.name = "allocate",
              .result = &spank_test::state().sessionAllocResult,
              .value = QDMI_ERROR_OUTOFMEM,
              .sessionWasAllocated = false,
              .shouldFinalize = true},
      Failure{.name = "parameter",
              .result = &spank_test::state().setParameterResult,
              .value = QDMI_ERROR_NOTSUPPORTED,
              .sessionWasAllocated = true,
              .shouldFinalize = true},
      Failure{.name = "session init",
              .result = &spank_test::state().sessionInitResult,
              .value = QDMI_ERROR_PERMISSIONDENIED,
              .sessionWasAllocated = true,
              .shouldFinalize = true},
      Failure{.name = "status query",
              .result = &spank_test::state().queryStatusResult,
              .value = QDMI_ERROR_FATAL,
              .sessionWasAllocated = true,
              .shouldFinalize = true},
  };

  for (const auto& failure : failures) {
    spank_test::reset();
    auto* const spank = initializeRemote();
    spank_test::configureOptIn();
    *failure.result = failure.value;
    ASSERT_EQ(slurm_spank_init_post_opt(spank, 0, nullptr), 0) << failure.name;
    ASSERT_EQ(slurm_spank_user_init(spank, 0, nullptr), 0) << failure.name;
    EXPECT_LT(slurm_spank_task_init(spank, 0, nullptr), 0) << failure.name;
    EXPECT_EQ(spank_test::state().sessionFreeCalls,
              failure.sessionWasAllocated ? 1 : 0)
        << failure.name;
    EXPECT_EQ(spank_test::state().finalizeCalls, failure.shouldFinalize ? 1 : 0)
        << failure.name;
  }
}

TEST_F(SpankTest, RemoteQdmiExceptionRejectsAndCleansUp) {
  auto* const spank = initializeRemote();
  spank_test::configureOptIn();
  spank_test::state().sessionInitThrows = true;

  ASSERT_EQ(slurm_spank_init_post_opt(spank, 0, nullptr), 0);
  int userInitResult = -1;
  EXPECT_NO_THROW(userInitResult = slurm_spank_user_init(spank, 0, nullptr));
  EXPECT_EQ(userInitResult, 0);
  EXPECT_LT(slurm_spank_task_init(spank, 0, nullptr), 0);
  EXPECT_EQ(spank_test::state().sessionFreeCalls, 1);
  EXPECT_EQ(spank_test::state().finalizeCalls, 1);
  EXPECT_NE(std::ranges::find(
                spank_test::state().logs,
                "amazon-braket-qdmi: QDMI validation raised an exception: "
                "simulated QDMI session initialization failure"),
            spank_test::state().logs.end());
}

TEST_F(SpankTest, RemoteQdmiInitializeExceptionRejectsAndFinalizes) {
  auto* const spank = initializeRemote();
  spank_test::configureOptIn();
  spank_test::state().deviceInitializeThrows = true;

  ASSERT_EQ(slurm_spank_init_post_opt(spank, 0, nullptr), 0);
  int userInitResult = -1;
  EXPECT_NO_THROW(userInitResult = slurm_spank_user_init(spank, 0, nullptr));
  EXPECT_EQ(userInitResult, 0);
  EXPECT_LT(slurm_spank_task_init(spank, 0, nullptr), 0);
  EXPECT_EQ(spank_test::state().sessionAllocCalls, 0);
  EXPECT_EQ(spank_test::state().finalizeCalls, 1);
  EXPECT_NE(std::ranges::find(
                spank_test::state().logs,
                "amazon-braket-qdmi: QDMI validation raised an exception: "
                "simulated QDMI device initialization failure"),
            spank_test::state().logs.end());
}

TEST_F(SpankTest, RemoteUnknownQdmiExceptionRejectsAndCleansUp) {
  auto* const spank = initializeRemote();
  spank_test::configureOptIn();
  spank_test::state().sessionInitThrowsUnknown = true;

  ASSERT_EQ(slurm_spank_init_post_opt(spank, 0, nullptr), 0);
  int userInitResult = -1;
  EXPECT_NO_THROW(userInitResult = slurm_spank_user_init(spank, 0, nullptr));
  EXPECT_EQ(userInitResult, 0);
  EXPECT_LT(slurm_spank_task_init(spank, 0, nullptr), 0);
  EXPECT_EQ(spank_test::state().sessionFreeCalls, 1);
  EXPECT_EQ(spank_test::state().finalizeCalls, 1);
  EXPECT_NE(
      std::ranges::find(spank_test::state().logs,
                        "amazon-braket-qdmi: QDMI validation raised an unknown "
                        "exception"),
      spank_test::state().logs.end());
}

TEST_F(SpankTest, EnvironmentFailureRejectsJob) {
  auto* const spank = initializeRemote();
  spank_test::configureOptIn();
  spank_test::state().environmentResult = ESPANK_ERROR;
  ASSERT_EQ(slurm_spank_init_post_opt(spank, 0, nullptr), 0);
  ASSERT_EQ(slurm_spank_user_init(spank, 0, nullptr), 0);
  EXPECT_LT(slurm_spank_task_init(spank, 0, nullptr), 0);
  EXPECT_EQ(spank_test::state().finalizeCalls, 1);
}

TEST_F(SpankTest, LocalContextIsUnaffected) {
  auto* const spank = initializeAllocator();
  spank_test::configureOptIn();
  spank_test::state().context = S_CTX_LOCAL;
  EXPECT_EQ(slurm_spank_init_post_opt(spank, 0, nullptr), 0);
  EXPECT_EQ(slurm_spank_user_init(spank, 0, nullptr), 0);
  EXPECT_EQ(slurm_spank_task_init(spank, 0, nullptr), 0);
  EXPECT_EQ(spank_test::state().initializeCalls, 0);
}

// NOLINTEND(cppcoreguidelines-owning-memory)

} // namespace
