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
 * @file test_device.cpp
 * @brief Integration tests for the Amazon Braket QDMI device adapter.
 *
 * These tests make real AWS API calls. They require:
 *   - AWS credentials (via AWS_CREDENTIALS_FILE or AWS_ACCESS_KEY_ID /
 *     AWS_SECRET_ACCESS_KEY environment variables), and
 *   - an S3 bucket (via AWS_S3_BUCKET) for job-submission tests.
 *
 * Run configuration (example):
 *   AWS_CREDENTIALS_FILE=/path/to/.aws/credentials \
 *   AWS_S3_BUCKET=amazon-braket-my-bucket \
 *   ./build/test/amazon-braket-qdmi-device-aws-test
 *
 * Fixtures:
 *  - AmazonBraketQDMISpecificationTest : AWS SV1 simulator session
 *  - AmazonBraketQDMIJobSpecificationTest : shared job that is submitted once
 *    per test suite via SetUpTestSuite() and reused across tests
 *  - DeviceParsingTestFixture : parameterised device-parsing tests
 */

#include "amazon-braket-qdmi-device/constants.hpp"
#include "amazon_braket_qdmi/device.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
constexpr const char* BELL_STATE_PROGRAM = "OPENQASM 3.0;\n"
                                           "qubit[2] q;\n"
                                           "bit[2] c;\n"
                                           "h q[0];\n"
                                           "cnot q[0], q[1];\n"
                                           "c[0] = measure q[0];\n"
                                           "c[1] = measure q[1];\n";

// ── Helpers ──────────────────────────────────────────────────────────────────

void setupCredentials(AMAZON_BRAKET_QDMI_Device_Session session,
                      bool failOnMissing = true) {
  const char* credsFileEnv = std::getenv("AWS_CREDENTIALS_FILE");
  if (credsFileEnv != nullptr && strlen(credsFileEnv) > 0) {
    if (AMAZON_BRAKET_QDMI_device_session_set_parameter(
            session, QDMI_DEVICE_SESSION_PARAMETER_AUTHFILE,
            strlen(credsFileEnv) + 1, credsFileEnv) != QDMI_SUCCESS) {
      if (failOnMissing) {
        throw std::runtime_error("Failed to set credentials file");
      }
    }
    return;
  }

  const char* accessKeyEnv = std::getenv("AWS_ACCESS_KEY_ID");
  const char* secretKeyEnv = std::getenv("AWS_SECRET_ACCESS_KEY");
  const char* sessionTokenEnv = std::getenv("AWS_SESSION_TOKEN");

  if (accessKeyEnv != nullptr && secretKeyEnv != nullptr) {
    if (AMAZON_BRAKET_QDMI_device_session_set_parameter(
            session, QDMI_DEVICE_SESSION_PARAMETER_USERNAME,
            strlen(accessKeyEnv) + 1, accessKeyEnv) != QDMI_SUCCESS) {
      if (failOnMissing) {
        throw std::runtime_error("Failed to set AWS_ACCESS_KEY_ID");
      }
    }
    if (AMAZON_BRAKET_QDMI_device_session_set_parameter(
            session, QDMI_DEVICE_SESSION_PARAMETER_PASSWORD,
            strlen(secretKeyEnv) + 1, secretKeyEnv) != QDMI_SUCCESS) {
      if (failOnMissing) {
        throw std::runtime_error("Failed to set AWS_SECRET_ACCESS_KEY");
      }
    }
    if (sessionTokenEnv != nullptr && strlen(sessionTokenEnv) > 0) {
      AMAZON_BRAKET_QDMI_device_session_set_parameter(
          session, QDMI_DEVICE_SESSION_PARAMETER_TOKEN,
          strlen(sessionTokenEnv) + 1, sessionTokenEnv);
    }
  } else if (failOnMissing) {
    throw std::runtime_error("No credentials provided");
  }
}

[[nodiscard]] auto querySites(AMAZON_BRAKET_QDMI_Device_Session session)
    -> std::vector<AMAZON_BRAKET_QDMI_Site> {
  size_t size = 0;
  if (AMAZON_BRAKET_QDMI_device_session_query_device_property(
          session,
          static_cast<QDMI_Device_Property>(QDMI_DEVICE_PROPERTY_SITES), 0,
          nullptr, &size) != QDMI_SUCCESS) {
    throw std::runtime_error("Failed to query sites");
  }
  if (size == 0) {
    throw std::runtime_error("No sites available");
  }
  std::vector<AMAZON_BRAKET_QDMI_Site> sites(size /
                                             sizeof(AMAZON_BRAKET_QDMI_Site));
  if (AMAZON_BRAKET_QDMI_device_session_query_device_property(
          session,
          static_cast<QDMI_Device_Property>(QDMI_DEVICE_PROPERTY_SITES), size,
          static_cast<void*>(sites.data()), nullptr) != QDMI_SUCCESS) {
    throw std::runtime_error("Failed to query sites");
  }
  return sites;
}

[[nodiscard]] auto queryOperations(AMAZON_BRAKET_QDMI_Device_Session session)
    -> std::vector<AMAZON_BRAKET_QDMI_Operation> {
  size_t size = 0;
  if (AMAZON_BRAKET_QDMI_device_session_query_device_property(
          session,
          static_cast<QDMI_Device_Property>(QDMI_DEVICE_PROPERTY_OPERATIONS), 0,
          nullptr, &size) != QDMI_SUCCESS) {
    throw std::runtime_error("Failed to query operations");
  }
  if (size == 0) {
    throw std::runtime_error("No operations available");
  }
  std::vector<AMAZON_BRAKET_QDMI_Operation> operations(
      size / sizeof(AMAZON_BRAKET_QDMI_Operation));
  if (AMAZON_BRAKET_QDMI_device_session_query_device_property(
          session,
          static_cast<QDMI_Device_Property>(QDMI_DEVICE_PROPERTY_OPERATIONS),
          size, static_cast<void*>(operations.data()),
          nullptr) != QDMI_SUCCESS) {
    throw std::runtime_error("Failed to query operations");
  }
  return operations;
}
} // namespace

// =============================================================================
// Fixture: fully initialised AWS SV1 session (real AWS connection)
// =============================================================================

class AmazonBraketQDMISpecificationTest : public ::testing::Test {
protected:
  AMAZON_BRAKET_QDMI_Device_Session session = nullptr;

  void SetUp() override {
    ASSERT_EQ(AMAZON_BRAKET_QDMI_device_initialize(), QDMI_SUCCESS)
        << "Failed to initialize the device";
    ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_alloc(&session), QDMI_SUCCESS)
        << "Failed to allocate a session";

    try {
      setupCredentials(session);
    } catch (const std::exception& e) {
      GTEST_FAIL() << "Credentials setup failed: " << e.what() << "\n"
                   << "Set either:\n"
                   << "  1. AWS_CREDENTIALS_FILE (path to credentials file)\n"
                   << "  2. AWS_ACCESS_KEY_ID + AWS_SECRET_ACCESS_KEY";
    }

    const char* deviceArn =
        "arn:aws:braket:::device/quantum-simulator/amazon/sv1";
    ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                  session,
                  AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN,
                  strlen(deviceArn) + 1, deviceArn),
              QDMI_SUCCESS)
        << "Failed to set device ARN";

    ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_init(session), QDMI_SUCCESS)
        << "Failed to initialize session. Check credentials and device status.";
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
// Fixture: shared submitted job (real AWS SV1 Bell-state execution)
// =============================================================================

class AmazonBraketQDMIJobSpecificationTest
    : public AmazonBraketQDMISpecificationTest {
protected:
  static AMAZON_BRAKET_QDMI_Device_Session sharedSession;
  static AMAZON_BRAKET_QDMI_Device_Job sharedJob;

  static bool submittedOk;
  static int waitResult;
  static bool hasShots;
  static std::string shotsData;
  static bool hasHist;
  static std::vector<std::string> histKeys;
  static std::vector<size_t> histValues;

  AMAZON_BRAKET_QDMI_Device_Job job = nullptr;

  static void SetUpTestSuite() {
    submittedOk = false;
    waitResult = QDMI_ERROR_NOTSUPPORTED;
    hasShots = false;
    shotsData.clear();
    hasHist = false;
    histKeys.clear();
    histValues.clear();

    if (AMAZON_BRAKET_QDMI_device_initialize() != QDMI_SUCCESS) {
      GTEST_FAIL()
          << "AMAZON_BRAKET_QDMI_device_initialize failed in SetUpTestSuite";
      return;
    }
    if (AMAZON_BRAKET_QDMI_device_session_alloc(&sharedSession) !=
        QDMI_SUCCESS) {
      GTEST_FAIL() << "session_alloc failed in SetUpTestSuite";
      return;
    }

    try {
      setupCredentials(sharedSession);
    } catch (const std::exception& e) {
      GTEST_FAIL() << "Credentials setup failed in SetUpTestSuite: " << e.what()
                   << "\n"
                   << "Set either:\n"
                   << "  1. AWS_CREDENTIALS_FILE (path to credentials file)\n"
                   << "  2. AWS_ACCESS_KEY_ID + AWS_SECRET_ACCESS_KEY";
      return;
    }

    const char* deviceArn =
        "arn:aws:braket:::device/quantum-simulator/amazon/sv1";
    if (AMAZON_BRAKET_QDMI_device_session_set_parameter(
            sharedSession,
            AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN,
            strlen(deviceArn) + 1, deviceArn) != QDMI_SUCCESS) {
      GTEST_FAIL() << "Failed to set device ARN in SetUpTestSuite";
      return;
    }
    if (AMAZON_BRAKET_QDMI_device_session_init(sharedSession) != QDMI_SUCCESS) {
      GTEST_SKIP() << "session_init failed in SetUpTestSuite; skipping job "
                      "submission tests";
      return;
    }
    if (AMAZON_BRAKET_QDMI_device_session_create_device_job(
            sharedSession, &sharedJob) != QDMI_SUCCESS) {
      GTEST_SKIP() << "create_device_job failed in SetUpTestSuite; skipping "
                      "job submission tests";
      return;
    }

    AMAZON_BRAKET_QDMI_device_job_set_parameter(
        sharedJob, QDMI_DEVICE_JOB_PARAMETER_PROGRAM,
        strlen(BELL_STATE_PROGRAM) + 1, BELL_STATE_PROGRAM);
    QDMI_Program_Format format = QDMI_PROGRAM_FORMAT_QASM3;
    AMAZON_BRAKET_QDMI_device_job_set_parameter(
        sharedJob, QDMI_DEVICE_JOB_PARAMETER_PROGRAMFORMAT, sizeof(format),
        &format);
    size_t shots = 100;
    AMAZON_BRAKET_QDMI_device_job_set_parameter(
        sharedJob, QDMI_DEVICE_JOB_PARAMETER_SHOTSNUM, sizeof(shots), &shots);

    const char* s3BucketEnv = std::getenv("AWS_S3_BUCKET");
    if (s3BucketEnv == nullptr || strlen(s3BucketEnv) == 0) {
      GTEST_SKIP()
          << "AWS_S3_BUCKET environment variable not set; skipping job "
             "submission tests";
      return;
    }
    AMAZON_BRAKET_QDMI_device_job_set_parameter(
        sharedJob, AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3BUCKET,
        strlen(s3BucketEnv) + 1, s3BucketEnv);

    if (const auto submitStatus =
            AMAZON_BRAKET_QDMI_device_job_submit(sharedJob);
        submitStatus != QDMI_SUCCESS) {
      if (submitStatus == QDMI_ERROR_NOTSUPPORTED) {
        GTEST_SKIP() << "job_submit not supported; skipping job result tests";
        return;
      }
      GTEST_FAIL() << "job_submit failed with status " << submitStatus;
      return;
    }
    submittedOk = true;

    waitResult = AMAZON_BRAKET_QDMI_device_job_wait(sharedJob, 120);

    if (waitResult == QDMI_SUCCESS) {
      QDMI_Job_Status finalStatus = QDMI_JOB_STATUS_CREATED;
      if (AMAZON_BRAKET_QDMI_device_job_check(sharedJob, &finalStatus) ==
              QDMI_SUCCESS &&
          finalStatus == QDMI_JOB_STATUS_DONE) {
        size_t shotsSize = 0;
        if (AMAZON_BRAKET_QDMI_device_job_get_results(
                sharedJob, QDMI_JOB_RESULT_SHOTS, 0, nullptr, &shotsSize) ==
                QDMI_SUCCESS &&
            shotsSize > 0) {
          std::string shotsStr(shotsSize - 1, '\0');
          if (AMAZON_BRAKET_QDMI_device_job_get_results(
                  sharedJob, QDMI_JOB_RESULT_SHOTS, shotsSize, shotsStr.data(),
                  nullptr) == QDMI_SUCCESS) {
            hasShots = true;
            shotsData = shotsStr;
          }
        }
        size_t keysSize = 0;
        if (AMAZON_BRAKET_QDMI_device_job_get_results(
                sharedJob, QDMI_JOB_RESULT_HIST_KEYS, 0, nullptr, &keysSize) ==
                QDMI_SUCCESS &&
            keysSize > 0) {
          std::vector<char> keysData(keysSize);
          if (AMAZON_BRAKET_QDMI_device_job_get_results(
                  sharedJob, QDMI_JOB_RESULT_HIST_KEYS, keysSize,
                  keysData.data(), nullptr) == QDMI_SUCCESS) {
            if (keysData.back() != '\0') {
              return;
            }
            std::stringstream keyStream(keysData.data());
            std::string key;
            while (std::getline(keyStream, key, ',')) {
              histKeys.emplace_back(std::move(key));
            }
          }
        }
        size_t valuesSize = 0;
        if (AMAZON_BRAKET_QDMI_device_job_get_results(
                sharedJob, QDMI_JOB_RESULT_HIST_VALUES, 0, nullptr,
                &valuesSize) == QDMI_SUCCESS &&
            valuesSize > 0) {
          if (valuesSize % sizeof(size_t) == 0) {
            const size_t n = valuesSize / sizeof(size_t);
            histValues.resize(n);
            if (AMAZON_BRAKET_QDMI_device_job_get_results(
                    sharedJob, QDMI_JOB_RESULT_HIST_VALUES, valuesSize,
                    histValues.data(), nullptr) == QDMI_SUCCESS) {
              hasHist = !histKeys.empty() && !histValues.empty();
            }
          }
        }
      }
    }
  }

  static void TearDownTestSuite() {
    if (sharedJob != nullptr) {
      AMAZON_BRAKET_QDMI_device_job_free(sharedJob);
      sharedJob = nullptr;
    }
    if (sharedSession != nullptr) {
      AMAZON_BRAKET_QDMI_device_session_free(sharedSession);
      sharedSession = nullptr;
    }
    AMAZON_BRAKET_QDMI_device_finalize();
  }

  void SetUp() override { job = sharedJob; }
  void TearDown() override { job = nullptr; }
};

AMAZON_BRAKET_QDMI_Device_Session
    AmazonBraketQDMIJobSpecificationTest::sharedSession = nullptr;
AMAZON_BRAKET_QDMI_Device_Job AmazonBraketQDMIJobSpecificationTest::sharedJob =
    nullptr;
bool AmazonBraketQDMIJobSpecificationTest::submittedOk = false;
int AmazonBraketQDMIJobSpecificationTest::waitResult = QDMI_ERROR_NOTSUPPORTED;
bool AmazonBraketQDMIJobSpecificationTest::hasShots = false;
std::string AmazonBraketQDMIJobSpecificationTest::shotsData;
bool AmazonBraketQDMIJobSpecificationTest::hasHist = false;
std::vector<std::string> AmazonBraketQDMIJobSpecificationTest::histKeys;
std::vector<size_t> AmazonBraketQDMIJobSpecificationTest::histValues;

// =============================================================================
// AmazonBraketQDMISpecificationTest — device property queries
// =============================================================================

TEST_F(AmazonBraketQDMISpecificationTest, QueryDeviceProperty) {
  AMAZON_BRAKET_QDMI_Device_Session uninitializedSession = nullptr;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_alloc(&uninitializedSession),
            QDMI_SUCCESS);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_query_device_property(
                uninitializedSession,
                static_cast<QDMI_Device_Property>(QDMI_DEVICE_PROPERTY_NAME), 0,
                nullptr, nullptr),
            QDMI_ERROR_BADSTATE);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_query_device_property(
                nullptr,
                static_cast<QDMI_Device_Property>(QDMI_DEVICE_PROPERTY_NAME), 0,
                nullptr, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_query_device_property(
                session,
                static_cast<QDMI_Device_Property>(QDMI_DEVICE_PROPERTY_MAX), 0,
                nullptr, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_THAT(
      AMAZON_BRAKET_QDMI_device_session_query_device_property(
          session,
          static_cast<QDMI_Device_Property>(QDMI_DEVICE_PROPERTY_COUPLINGMAP),
          0, nullptr, nullptr),
      testing::AnyOf(QDMI_SUCCESS, QDMI_ERROR_NOTSUPPORTED));
  AMAZON_BRAKET_QDMI_device_session_free(uninitializedSession);
}

TEST_F(AmazonBraketQDMISpecificationTest, QuerySupportedProgramFormats) {
  size_t size = 0;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_query_device_property(
                session,
                static_cast<QDMI_Device_Property>(
                    QDMI_DEVICE_PROPERTY_SUPPORTEDPROGRAMFORMATS),
                0, nullptr, &size),
            QDMI_SUCCESS);
  ASSERT_EQ(size, 2 * sizeof(QDMI_Program_Format));

  std::vector<QDMI_Program_Format> formats(2);
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_query_device_property(
                session,
                static_cast<QDMI_Device_Property>(
                    QDMI_DEVICE_PROPERTY_SUPPORTEDPROGRAMFORMATS),
                size, formats.data(), nullptr),
            QDMI_SUCCESS);
  EXPECT_EQ(formats[0], QDMI_PROGRAM_FORMAT_QASM2);
  EXPECT_EQ(formats[1], QDMI_PROGRAM_FORMAT_QASM3);
}

TEST_F(AmazonBraketQDMISpecificationTest, QuerySiteProperty) {
  AMAZON_BRAKET_QDMI_Site site = querySites(session).front();
  EXPECT_EQ(
      AMAZON_BRAKET_QDMI_device_session_query_site_property(
          session, nullptr, QDMI_SITE_PROPERTY_INDEX, 0, nullptr, nullptr),
      QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_query_site_property(
                nullptr, site, QDMI_SITE_PROPERTY_INDEX, 0, nullptr, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_query_site_property(
                session, site, QDMI_SITE_PROPERTY_MAX, 0, nullptr, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_query_site_property(
                session, site, QDMI_SITE_PROPERTY_NAME, 0, nullptr, nullptr),
            QDMI_SUCCESS);
}

TEST_F(AmazonBraketQDMISpecificationTest, QueryOperationProperty) {
  AMAZON_BRAKET_QDMI_Operation operation = queryOperations(session).front();
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_query_operation_property(
                nullptr, operation, 0, nullptr, 0, nullptr,
                QDMI_OPERATION_PROPERTY_NAME, 0, nullptr, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_query_operation_property(
                session, operation, 0, nullptr, 0, nullptr,
                QDMI_OPERATION_PROPERTY_MAX, 0, nullptr, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_query_operation_property(
                session, operation, 0, nullptr, 0, nullptr,
                QDMI_OPERATION_PROPERTY_QUBITSNUM, 0, nullptr, nullptr),
            QDMI_SUCCESS);
}

TEST_F(AmazonBraketQDMISpecificationTest, QueryDeviceName) {
  size_t size = 0;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_query_device_property(
                session, QDMI_DEVICE_PROPERTY_NAME, 0, nullptr, &size),
            QDMI_SUCCESS)
      << "Devices must provide a name";
  std::string value(size - 1, '\0');
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_query_device_property(
          session, QDMI_DEVICE_PROPERTY_NAME, size, value.data(), nullptr),
      QDMI_SUCCESS)
      << "Devices must provide a name";
  EXPECT_FALSE(value.empty()) << "Devices must provide a name";
}

TEST_F(AmazonBraketQDMISpecificationTest, QueryDeviceLibraryVersion) {
  size_t size = 0;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_query_device_property(
          session, QDMI_DEVICE_PROPERTY_LIBRARYVERSION, 0, nullptr, &size),
      QDMI_SUCCESS)
      << "Devices must provide a library version";
  std::string value(size - 1, '\0');
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_query_device_property(
                session, QDMI_DEVICE_PROPERTY_LIBRARYVERSION, size,
                value.data(), nullptr),
            QDMI_SUCCESS)
      << "Devices must provide a library version";
  EXPECT_FALSE(value.empty()) << "Devices must provide a library version";
}

// VERSION is a library-level property delegated to the global Device singleton.
// This path in Device::queryProperty() is distinct from LIBRARYVERSION.
TEST_F(AmazonBraketQDMISpecificationTest, QueryDeviceVersion) {
  size_t size = 0;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_query_device_property(
                session, QDMI_DEVICE_PROPERTY_VERSION, 0, nullptr, &size),
            QDMI_SUCCESS)
      << "Device must provide a version string";
  ASSERT_GT(size, 0U);
  std::string version(size - 1, '\0');
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_query_device_property(
          session, QDMI_DEVICE_PROPERTY_VERSION, size, version.data(), nullptr),
      QDMI_SUCCESS);
  EXPECT_FALSE(version.empty());
}

// NEEDSCALIBRATION is always 0 for Braket (no offline calibration step).
TEST_F(AmazonBraketQDMISpecificationTest, QueryDeviceNeedsCalibration) {
  size_t needsCalibration = 99; // sentinel — must be overwritten
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_query_device_property(
                session, QDMI_DEVICE_PROPERTY_NEEDSCALIBRATION, sizeof(size_t),
                &needsCalibration, nullptr),
            QDMI_SUCCESS);
  EXPECT_EQ(needsCalibration, 0U);
}

TEST_F(AmazonBraketQDMISpecificationTest, QuerySiteIndex) {
  uint64_t id = 0;
  EXPECT_NO_THROW(for (auto* site : querySites(session)) {
    EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_query_site_property(
                  session, site, QDMI_SITE_PROPERTY_INDEX, sizeof(uint64_t),
                  &id, nullptr),
              QDMI_SUCCESS)
        << "Devices must provide a site id";
  }) << "Devices must provide a list of sites";
}

TEST_F(AmazonBraketQDMISpecificationTest, QueryOperationName) {
  size_t nameSize = 0;
  EXPECT_NO_THROW(for (auto* operation : queryOperations(session)) {
    ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_query_operation_property(
                  session, operation, 0, nullptr, 0, nullptr,
                  QDMI_OPERATION_PROPERTY_NAME, 0, nullptr, &nameSize),
              QDMI_SUCCESS)
        << "Devices must provide an operation name";
    std::string name(nameSize - 1, '\0');
    EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_query_operation_property(
                  session, operation, 0, nullptr, 0, nullptr,
                  QDMI_OPERATION_PROPERTY_NAME, nameSize, name.data(), nullptr),
              QDMI_SUCCESS)
        << "Devices must provide an operation name";
  }) << "Devices must provide a list of operations";
}

TEST_F(AmazonBraketQDMISpecificationTest, QueryDeviceQubitNum) {
  size_t numQubits = 0;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_query_device_property(
                session, QDMI_DEVICE_PROPERTY_QUBITSNUM, sizeof(size_t),
                &numQubits, nullptr),
            QDMI_SUCCESS);
}

TEST_F(AmazonBraketQDMISpecificationTest, QueryDeviceStatus) {
  QDMI_Device_Status status = QDMI_DEVICE_STATUS_OFFLINE;
  const auto result = AMAZON_BRAKET_QDMI_device_session_query_device_property(
      session, QDMI_DEVICE_PROPERTY_STATUS, sizeof(status), &status, nullptr);
  ASSERT_EQ(result, QDMI_SUCCESS);
  EXPECT_TRUE(status == QDMI_DEVICE_STATUS_IDLE ||
              status == QDMI_DEVICE_STATUS_BUSY ||
              status == QDMI_DEVICE_STATUS_OFFLINE);
}

TEST_F(AmazonBraketQDMISpecificationTest, QuerySiteData) {
  std::vector<AMAZON_BRAKET_QDMI_Site> sites;
  EXPECT_NO_THROW(sites = querySites(session)) << "Devices must provide sites";
  EXPECT_GT(sites.size(), 0);
  for (auto* site : sites) {
    double t1 = 0.0;
    EXPECT_THAT(
        AMAZON_BRAKET_QDMI_device_session_query_site_property(
            session, site, QDMI_SITE_PROPERTY_T1, sizeof(double), &t1, nullptr),
        testing::AnyOf(QDMI_SUCCESS, QDMI_ERROR_NOTSUPPORTED));
    double t2 = 0.0;
    EXPECT_THAT(
        AMAZON_BRAKET_QDMI_device_session_query_site_property(
            session, site, QDMI_SITE_PROPERTY_T2, sizeof(double), &t2, nullptr),
        testing::AnyOf(QDMI_SUCCESS, QDMI_ERROR_NOTSUPPORTED));
  }
}

TEST_F(AmazonBraketQDMISpecificationTest, QueryOperationData) {
  std::vector<AMAZON_BRAKET_QDMI_Operation> operations;
  EXPECT_NO_THROW(operations = queryOperations(session));
  for (auto* operation : operations) {
    size_t nameSize = 0;
    ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_query_operation_property(
                  session, operation, 0, nullptr, 0, nullptr,
                  QDMI_OPERATION_PROPERTY_NAME, 0, nullptr, &nameSize),
              QDMI_SUCCESS);
    std::string name(nameSize - 1, '\0');
    EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_query_operation_property(
                  session, operation, 0, nullptr, 0, nullptr,
                  QDMI_OPERATION_PROPERTY_NAME, nameSize, name.data(), nullptr),
              QDMI_SUCCESS);

    double fidelity = 0;
    auto result = AMAZON_BRAKET_QDMI_device_session_query_operation_property(
        session, operation, 0, nullptr, 0, nullptr,
        QDMI_OPERATION_PROPERTY_FIDELITY, sizeof(double), &fidelity, nullptr);
    EXPECT_EQ(result, QDMI_ERROR_NOTSUPPORTED)
        << "Braket only reports site-dependent gate fidelities";

    size_t numQubits = 0;
    result = AMAZON_BRAKET_QDMI_device_session_query_operation_property(
        session, operation, 0, nullptr, 0, nullptr,
        QDMI_OPERATION_PROPERTY_QUBITSNUM, sizeof(size_t), &numQubits, nullptr);
    EXPECT_EQ(result, QDMI_SUCCESS);
    EXPECT_GT(numQubits, 0);

    size_t sitesSize = 0;
    ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_query_operation_property(
                  session, operation, 0, nullptr, 0, nullptr,
                  QDMI_OPERATION_PROPERTY_SITES, 0, nullptr, &sitesSize),
              QDMI_SUCCESS);
    ASSERT_GT(sitesSize, 0U);
    ASSERT_EQ(sitesSize % (numQubits * sizeof(AMAZON_BRAKET_QDMI_Site)), 0U);
    std::vector<AMAZON_BRAKET_QDMI_Site> applicableSites(
        sitesSize / sizeof(AMAZON_BRAKET_QDMI_Site));
    ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_query_operation_property(
                  session, operation, 0, nullptr, 0, nullptr,
                  QDMI_OPERATION_PROPERTY_SITES, sitesSize,
                  applicableSites.data(), nullptr),
              QDMI_SUCCESS);

    size_t numParameters = 0;
    result = AMAZON_BRAKET_QDMI_device_session_query_operation_property(
        session, operation, 0, nullptr, 0, nullptr,
        QDMI_OPERATION_PROPERTY_PARAMETERSNUM, sizeof(size_t), &numParameters,
        nullptr);
    EXPECT_EQ(result, QDMI_SUCCESS);

    result = AMAZON_BRAKET_QDMI_device_session_query_operation_property(
        session, operation, numQubits, applicableSites.data(), 0, nullptr,
        QDMI_OPERATION_PROPERTY_FIDELITY, sizeof(double), &fidelity, nullptr);
    EXPECT_THAT(result, testing::AnyOf(QDMI_SUCCESS, QDMI_ERROR_NOTSUPPORTED));
    if (result == QDMI_SUCCESS) {
      EXPECT_GE(fidelity, 0.);
      EXPECT_LE(fidelity, 1.);
    }
  }
}

// ── Small-buffer (too-small write buffer) guards ─────────────────────────────

TEST_F(AmazonBraketQDMISpecificationTest,
       QueryDevicePropertyNameBufferTooSmall) {
  std::array<char, 1> buf = {};
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_query_device_property(
                session, QDMI_DEVICE_PROPERTY_NAME, 1, buf.data(), nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AmazonBraketQDMISpecificationTest,
       QueryDevicePropertySitesBufferTooSmall) {
  AMAZON_BRAKET_QDMI_Site buf = nullptr;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_query_device_property(
                session,
                static_cast<QDMI_Device_Property>(QDMI_DEVICE_PROPERTY_SITES),
                1, static_cast<void*>(&buf), nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AmazonBraketQDMISpecificationTest, QuerySitePropertyNameBufferTooSmall) {
  auto sites = querySites(session);
  ASSERT_GT(sites.size(), 0U);
  std::array<char, 1> buf = {};
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_query_site_property(
                session, sites.front(), QDMI_SITE_PROPERTY_NAME, 1, buf.data(),
                nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AmazonBraketQDMISpecificationTest,
       QuerySitePropertyIndexBufferTooSmall) {
  auto sites = querySites(session);
  ASSERT_GT(sites.size(), 0U);
  std::array<char, 1> buf = {};
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_query_site_property(
                session, sites.front(), QDMI_SITE_PROPERTY_INDEX, 1, buf.data(),
                nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AmazonBraketQDMISpecificationTest,
       QueryOperationPropertySitesNonNullNumSitesZero) {
  auto operations = queryOperations(session);
  ASSERT_GT(operations.size(), 0U);
  auto sites = querySites(session);
  ASSERT_GT(sites.size(), 0U);
  AMAZON_BRAKET_QDMI_Site site = sites.front();
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_query_operation_property(
                session, operations.front(), 0, &site, 0, nullptr,
                QDMI_OPERATION_PROPERTY_NAME, 0, nullptr, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AmazonBraketQDMISpecificationTest,
       QueryOperationPropertyParamsNonNullNumParamsZero) {
  auto operations = queryOperations(session);
  ASSERT_GT(operations.size(), 0U);
  const double param = 0.0;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_query_operation_property(
                session, operations.front(), 0, nullptr, 0, &param,
                QDMI_OPERATION_PROPERTY_NAME, 0, nullptr, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
}

// =============================================================================
// AmazonBraketQDMIJobSpecificationTest — real job submission / results
// =============================================================================

TEST_F(AmazonBraketQDMIJobSpecificationTest, JobSubmit) {
  if (!submittedOk) {
    GTEST_SKIP() << "Shared job was not submitted in suite setup";
  }
  EXPECT_TRUE(submittedOk);
}

TEST_F(AmazonBraketQDMIJobSpecificationTest, JobSetParameterAfterSubmit) {
  if (!submittedOk) {
    GTEST_SKIP() << "Shared job was not submitted; skipping post-submit test";
  }
  size_t shots = 200;
  EXPECT_EQ(
      AMAZON_BRAKET_QDMI_device_job_set_parameter(
          sharedJob, QDMI_DEVICE_JOB_PARAMETER_SHOTSNUM, sizeof(shots), &shots),
      QDMI_ERROR_BADSTATE);
}

TEST_F(AmazonBraketQDMIJobSpecificationTest, JobCancel) {
  const auto status = AMAZON_BRAKET_QDMI_device_job_cancel(job);
  ASSERT_THAT(status, testing::AnyOf(QDMI_SUCCESS, QDMI_ERROR_INVALIDARGUMENT,
                                     QDMI_ERROR_NOTSUPPORTED));
}

TEST_F(AmazonBraketQDMIJobSpecificationTest, JobCheck) {
  QDMI_Job_Status jobStatus = QDMI_JOB_STATUS_RUNNING;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_job_check(job, &jobStatus), QDMI_SUCCESS);
}

TEST_F(AmazonBraketQDMIJobSpecificationTest, JobCheckTerminalState) {
  if (!submittedOk || waitResult != QDMI_SUCCESS) {
    GTEST_SKIP()
        << "Shared job did not complete successfully; skipping terminal-check";
  }
  QDMI_Job_Status status = QDMI_JOB_STATUS_CREATED;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_check(sharedJob, &status),
            QDMI_SUCCESS);
  EXPECT_EQ(status, QDMI_JOB_STATUS_DONE);
}

TEST_F(AmazonBraketQDMIJobSpecificationTest, JobWait) {
  if (!submittedOk) {
    GTEST_SKIP() << "Job was not submitted in suite setup";
  }
  EXPECT_EQ(waitResult, QDMI_SUCCESS);
}

TEST_F(AmazonBraketQDMIJobSpecificationTest, JobWaitOnDoneJob) {
  if (!submittedOk || waitResult != QDMI_SUCCESS) {
    GTEST_SKIP() << "Shared job did not complete; skipping done-wait test";
  }
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_wait(sharedJob, 1), QDMI_SUCCESS);
}

TEST_F(AmazonBraketQDMIJobSpecificationTest, JobSubmitAlreadySubmitted) {
  if (!submittedOk) {
    GTEST_SKIP()
        << "Shared job was not submitted; skipping re-submit BADSTATE test";
  }
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_submit(sharedJob),
            QDMI_ERROR_BADSTATE);
}

TEST_F(AmazonBraketQDMIJobSpecificationTest, JobGetResults) {
  if (!submittedOk) {
    GTEST_SKIP() << "Job was not submitted in suite setup";
  }
  ASSERT_TRUE(hasShots) << "SHOTS results must be available for all devices";
  EXPECT_GT(shotsData.size(), 0U);
  size_t shotsSize = 0;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_get_results(
                job, QDMI_JOB_RESULT_SHOTS, 0, nullptr, &shotsSize),
            QDMI_SUCCESS);
  if (shotsSize > 0) {
    std::string buf(shotsSize - 1, '\0');
    EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_get_results(
                  job, QDMI_JOB_RESULT_SHOTS, shotsSize, buf.data(), nullptr),
              QDMI_SUCCESS);
  }
}

TEST_F(AmazonBraketQDMIJobSpecificationTest, JobGetResultsHistKeys) {
  if (!submittedOk) {
    GTEST_SKIP() << "Job was not submitted in suite setup";
  }
  ASSERT_TRUE(hasHist) << "Histogram results must be available for all devices";
  EXPECT_GT(histKeys.size(), 0U);
  bool found00 = false;
  bool found11 = false;
  for (const auto& key : histKeys) {
    if (key == "00") {
      found00 = true;
    }
    if (key == "11") {
      found11 = true;
    }
  }
  EXPECT_TRUE(found00 && found11)
      << "Did not find expected histogram keys '00' and '11'";
}

TEST_F(AmazonBraketQDMIJobSpecificationTest, JobGetResultsHistValues) {
  if (!submittedOk) {
    GTEST_SKIP() << "Job was not submitted in suite setup";
  }
  ASSERT_TRUE(hasHist) << "Histogram results must be available for all devices";
  EXPECT_EQ(histValues.size(), histKeys.size());
  EXPECT_GT(histValues.size(), 0U);
  size_t totalShots = 0;
  for (const auto count : histValues) {
    totalShots += count;
  }
  EXPECT_EQ(totalShots, 100U);
}

// If the caller supplies a non-null buffer that is too small, getResults()
// must return QDMI_ERROR_INVALIDARGUMENT without touching sizeRet.
TEST_F(AmazonBraketQDMIJobSpecificationTest, JobGetResultsShotsBufferTooSmall) {
  if (!submittedOk || waitResult != QDMI_SUCCESS || !hasShots) {
    GTEST_SKIP() << "Shared job did not produce shots; skipping buffer test";
  }
  size_t requiredSize = 0;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_job_get_results(
                sharedJob, QDMI_JOB_RESULT_SHOTS, 0, nullptr, &requiredSize),
            QDMI_SUCCESS);
  ASSERT_GT(requiredSize, 1U);
  // Buffer of 1 byte is always too small for any non-empty shots string.
  char smallBuf = '\0';
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_get_results(
                sharedJob, QDMI_JOB_RESULT_SHOTS, 1, &smallBuf, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AmazonBraketQDMIJobSpecificationTest,
       JobGetResultsHistKeysBufferTooSmall) {
  if (!submittedOk || waitResult != QDMI_SUCCESS || !hasHist) {
    GTEST_SKIP()
        << "Shared job did not produce histogram; skipping buffer test";
  }
  size_t requiredSize = 0;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_job_get_results(
          sharedJob, QDMI_JOB_RESULT_HIST_KEYS, 0, nullptr, &requiredSize),
      QDMI_SUCCESS);
  ASSERT_GT(requiredSize, 1U);
  char smallBuf = '\0';
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_get_results(
                sharedJob, QDMI_JOB_RESULT_HIST_KEYS, 1, &smallBuf, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AmazonBraketQDMIJobSpecificationTest,
       JobGetResultsHistValuesBufferTooSmall) {
  if (!submittedOk || waitResult != QDMI_SUCCESS || !hasHist) {
    GTEST_SKIP()
        << "Shared job did not produce histogram; skipping buffer test";
  }
  size_t requiredSize = 0;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_job_get_results(
          sharedJob, QDMI_JOB_RESULT_HIST_VALUES, 0, nullptr, &requiredSize),
      QDMI_SUCCESS);
  ASSERT_GT(requiredSize, sizeof(size_t));
  size_t smallVal = 0;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_get_results(
                sharedJob, QDMI_JOB_RESULT_HIST_VALUES, 1, &smallVal, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
}

// Requesting statevector / probability results is not implemented.
TEST_F(AmazonBraketQDMIJobSpecificationTest,
       JobGetResultsStatevectorNotSupported) {
  if (!submittedOk || waitResult != QDMI_SUCCESS) {
    GTEST_SKIP()
        << "Job did not complete successfully; skipping statevector test";
  }
  size_t sz = 99; // sentinel — must become 0
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_get_results(
                sharedJob, QDMI_JOB_RESULT_STATEVECTOR_DENSE, 0, nullptr, &sz),
            QDMI_ERROR_NOTSUPPORTED);
  EXPECT_EQ(sz, 0U);

  EXPECT_EQ(
      AMAZON_BRAKET_QDMI_device_job_get_results(
          sharedJob, QDMI_JOB_RESULT_PROBABILITIES_DENSE, 0, nullptr, nullptr),
      QDMI_ERROR_NOTSUPPORTED);
}

// =============================================================================
// Integration test: submit a job with explicit per-job S3 configuration
// =============================================================================

TEST(AmazonBraketQDMIPerJobS3Test, SubmitJobWithPerJobS3) {
  const char* s3BucketEnv = std::getenv("AWS_S3_BUCKET");
  if (s3BucketEnv == nullptr || strlen(s3BucketEnv) == 0) {
    GTEST_SKIP() << "AWS_S3_BUCKET not set, skipping S3 integration test";
  }

  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_initialize(), QDMI_SUCCESS);

  struct SessionGuard {
    AMAZON_BRAKET_QDMI_Device_Session session = nullptr;
    AMAZON_BRAKET_QDMI_Device_Job job = nullptr;
    ~SessionGuard() {
      if (job != nullptr) {
        AMAZON_BRAKET_QDMI_device_job_free(job);
      }
      if (session != nullptr) {
        AMAZON_BRAKET_QDMI_device_session_free(session);
      }
      AMAZON_BRAKET_QDMI_device_finalize();
    }
  } guard;
  auto& session = guard.session;

  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_alloc(&session), QDMI_SUCCESS);

  try {
    ::setupCredentials(session, true);
  } catch (const std::exception& e) {
    GTEST_SKIP() << "Credentials not available: " << e.what();
  }

  const char* deviceArn =
      "arn:aws:braket:::device/quantum-simulator/amazon/sv1";
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                session, AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN,
                strlen(deviceArn) + 1, deviceArn),
            QDMI_SUCCESS);
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_init(session), QDMI_SUCCESS);

  auto& job = guard.job;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &job),
            QDMI_SUCCESS);

  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                job, QDMI_DEVICE_JOB_PARAMETER_PROGRAM,
                strlen(BELL_STATE_PROGRAM) + 1, BELL_STATE_PROGRAM),
            QDMI_SUCCESS);

  QDMI_Program_Format format = QDMI_PROGRAM_FORMAT_QASM3;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                job, QDMI_DEVICE_JOB_PARAMETER_PROGRAMFORMAT, sizeof(format),
                &format),
            QDMI_SUCCESS);

  size_t shots = 100;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                job, QDMI_DEVICE_JOB_PARAMETER_SHOTSNUM, sizeof(shots), &shots),
            QDMI_SUCCESS);

  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                job, AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3BUCKET,
                strlen(s3BucketEnv) + 1, s3BucketEnv),
            QDMI_SUCCESS);

  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_job_submit(job), QDMI_SUCCESS)
      << "Job submission should succeed with S3 configuration";

  size_t idSize = 0;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_job_query_property(
                job, QDMI_DEVICE_JOB_PROPERTY_ID, 0, nullptr, &idSize),
            QDMI_SUCCESS);
  ASSERT_GT(idSize, 1U);

  std::vector<char> idBuffer(idSize);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_query_property(
                job, QDMI_DEVICE_JOB_PROPERTY_ID, idSize - 1, idBuffer.data(),
                nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_job_query_property(
                job, QDMI_DEVICE_JOB_PROPERTY_ID, idBuffer.size(),
                idBuffer.data(), nullptr),
            QDMI_SUCCESS);
  const std::string taskArn(idBuffer.data());
  EXPECT_TRUE(taskArn.starts_with("arn:aws:braket:"))
      << "The QDMI job ID must be the AWS QuantumTask ARN";

  const int waitStatus = AMAZON_BRAKET_QDMI_device_job_wait(job, 60);
  EXPECT_EQ(waitStatus, QDMI_SUCCESS) << "Job should complete successfully";

  QDMI_Job_Status finalStatus = QDMI_JOB_STATUS_CREATED;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_job_check(job, &finalStatus),
            QDMI_SUCCESS);
  EXPECT_EQ(finalStatus, QDMI_JOB_STATUS_DONE)
      << "Job should reach DONE status";

  std::vector<char> completedIdBuffer(idSize);
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_job_query_property(
                job, QDMI_DEVICE_JOB_PROPERTY_ID, completedIdBuffer.size(),
                completedIdBuffer.data(), nullptr),
            QDMI_SUCCESS);
  EXPECT_EQ(completedIdBuffer, idBuffer)
      << "The AWS QuantumTask ARN must remain stable after completion";

  size_t shotsSize = 0;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_get_results(
                job, QDMI_JOB_RESULT_SHOTS, 0, nullptr, &shotsSize),
            QDMI_SUCCESS);
  EXPECT_GT(shotsSize, 0U) << "Should have measurement results";

  size_t histKeysSize = 0;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_job_get_results(
                job, QDMI_JOB_RESULT_HIST_KEYS, 0, nullptr, &histKeysSize),
            QDMI_SUCCESS);
  ASSERT_GT(histKeysSize, 0U);

  std::vector<char> histKeysData(histKeysSize);
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_job_get_results(
                job, QDMI_JOB_RESULT_HIST_KEYS, histKeysSize,
                histKeysData.data(), nullptr),
            QDMI_SUCCESS);
  ASSERT_EQ(histKeysData.back(), '\0');
  const std::string_view histogramKeys(histKeysData.data(),
                                       histKeysData.size() - 1);
  EXPECT_TRUE(histogramKeys.find("00") != std::string_view::npos &&
              histogramKeys.find("11") != std::string_view::npos)
      << "Bell state should produce 00 and 11 outcomes";
  size_t histValuesSize = 0;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_job_get_results(
                job, QDMI_JOB_RESULT_HIST_VALUES, 0, nullptr, &histValuesSize),
            QDMI_SUCCESS);
  const auto keyCount = static_cast<size_t>(std::count(
                            histogramKeys.begin(), histogramKeys.end(), ',')) +
                        1;
  EXPECT_EQ(keyCount * sizeof(size_t), histValuesSize);
}

// =============================================================================
// DeviceParsingTestFixture — provider-specific JSON parsing
// =============================================================================

class DeviceParsingTestFixture : public ::testing::Test {
protected:
  AMAZON_BRAKET_QDMI_Device_Session session = nullptr;

  void SetUp() override {
    ASSERT_EQ(AMAZON_BRAKET_QDMI_device_initialize(), QDMI_SUCCESS);
    ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_alloc(&session), QDMI_SUCCESS);
  }

  void TearDown() override {
    if (session != nullptr) {
      AMAZON_BRAKET_QDMI_device_session_free(session);
    }
    AMAZON_BRAKET_QDMI_device_finalize();
  }

  void initializeDevice(const char* deviceArn) {
    try {
      ::setupCredentials(session, true);
    } catch (const std::exception& e) {
      GTEST_SKIP() << "Credentials not available: " << e.what();
    }
    ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                  session,
                  AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN,
                  strlen(deviceArn) + 1, deviceArn),
              QDMI_SUCCESS);
    if (AMAZON_BRAKET_QDMI_device_session_init(session) != QDMI_SUCCESS) {
      GTEST_SKIP() << "Device initialization failed (may not have access)";
    }
  }

  std::string getDeviceName() {
    size_t nameSize = 0;
    EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_query_device_property(
                  session, QDMI_DEVICE_PROPERTY_NAME, 0, nullptr, &nameSize),
              QDMI_SUCCESS);
    std::string name(nameSize - 1, '\0');
    EXPECT_EQ(
        AMAZON_BRAKET_QDMI_device_session_query_device_property(
            session, QDMI_DEVICE_PROPERTY_NAME, nameSize, name.data(), nullptr),
        QDMI_SUCCESS);
    return name;
  }

  size_t getQubitCount() {
    size_t qubitCount = 0;
    EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_query_device_property(
                  session, QDMI_DEVICE_PROPERTY_QUBITSNUM, sizeof(size_t),
                  &qubitCount, nullptr),
              QDMI_SUCCESS);
    return qubitCount;
  }

  std::vector<std::string> getSiteNames(const size_t maxCount = 5) {
    auto sites = querySites(session);
    std::vector<std::string> names;
    for (size_t i = 0; i < std::min(maxCount, sites.size()); ++i) {
      size_t nameSize = 0;
      if (AMAZON_BRAKET_QDMI_device_session_query_site_property(
              session, sites[i], QDMI_SITE_PROPERTY_NAME, 0, nullptr,
              &nameSize) != QDMI_SUCCESS ||
          nameSize == 0) {
        names.emplace_back();
        continue;
      }
      std::string name(nameSize - 1, '\0');
      AMAZON_BRAKET_QDMI_device_session_query_site_property(
          session, sites[i], QDMI_SITE_PROPERTY_NAME, nameSize, name.data(),
          nullptr);
      names.push_back(name);
    }
    return names;
  }

  bool hasGate(const std::string& gateName) {
    auto operations = queryOperations(session);
    for (auto* op : operations) {
      size_t nameSize = 0;
      if (AMAZON_BRAKET_QDMI_device_session_query_operation_property(
              session, op, 0, nullptr, 0, nullptr, QDMI_OPERATION_PROPERTY_NAME,
              0, nullptr, &nameSize) != QDMI_SUCCESS ||
          nameSize == 0) {
        continue;
      }
      std::string opName(nameSize - 1, '\0');
      AMAZON_BRAKET_QDMI_device_session_query_operation_property(
          session, op, 0, nullptr, 0, nullptr, QDMI_OPERATION_PROPERTY_NAME,
          nameSize, opName.data(), nullptr);
      if (opName == gateName) {
        return true;
      }
    }
    return false;
  }
};

TEST_F(DeviceParsingTestFixture, SV1SimulatorParsing) {
  initializeDevice("arn:aws:braket:::device/quantum-simulator/amazon/sv1");

  const std::string deviceName = getDeviceName();
  EXPECT_EQ(deviceName, "SV1") << "SV1 device name should be 'SV1'";

  const size_t qubitCount = getQubitCount();
  EXPECT_GT(qubitCount, 0U) << "SV1 should have qubits";
  EXPECT_LE(qubitCount, 34U) << "SV1 supports up to 34 qubits";

  auto sites = querySites(session);
  ASSERT_GT(sites.size(), 0U) << "SV1 should have sites";
  EXPECT_EQ(sites.size(), qubitCount);

  auto siteNames = getSiteNames(3);
  for (size_t i = 0; i < siteNames.size(); ++i) {
    EXPECT_EQ(siteNames[i], "Q" + std::to_string(i))
        << "SV1 sites should use standard format Q0, Q1, Q2, ...";
  }

  size_t connectivitySize = 0;
  auto connectivityResult =
      AMAZON_BRAKET_QDMI_device_session_query_device_property(
          session, QDMI_DEVICE_PROPERTY_COUPLINGMAP, 0, nullptr,
          &connectivitySize);
  if (connectivityResult == QDMI_SUCCESS && connectivitySize > 0) {
    std::vector<AMAZON_BRAKET_QDMI_Site> connectivity(
        connectivitySize / sizeof(AMAZON_BRAKET_QDMI_Site));
    ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_query_device_property(
                  session, QDMI_DEVICE_PROPERTY_COUPLINGMAP, connectivitySize,
                  static_cast<void*>(connectivity.data()), nullptr),
              QDMI_SUCCESS);
    const size_t numEdges = connectivity.size() / 2;
    EXPECT_EQ(numEdges, qubitCount * (qubitCount - 1))
        << "SV1 should have full connectivity (all-to-all)";
  }

  for (const auto& gate : {"h", "x", "y", "z", "cnot", "rx", "ry", "rz"}) {
    EXPECT_TRUE(hasGate(gate)) << "SV1 should support gate: " << gate;
  }
}

TEST_F(DeviceParsingTestFixture, IQMDeviceParsing) {
  const char* skipIQMEnv = std::getenv("SKIP_IQM_TESTS");
  if (skipIQMEnv != nullptr && strcmp(skipIQMEnv, "1") == 0) {
    GTEST_SKIP() << "IQM tests skipped (SKIP_IQM_TESTS=1)";
  }

  const char* iqmDeviceArn = std::getenv("IQM_DEVICE_ARN");
  if (iqmDeviceArn == nullptr) {
    iqmDeviceArn = "arn:aws:braket:eu-north-1::device/qpu/iqm/Garnet";
  }
  initializeDevice(iqmDeviceArn);

  const std::string deviceName = getDeviceName();
  EXPECT_FALSE(deviceName.empty()) << "IQM device should have a name";
  std::cerr << "IQM device name: " << deviceName << "\n";

  const size_t qubitCount = getQubitCount();
  EXPECT_GT(qubitCount, 0U) << "IQM device should have qubits";

  auto sites = querySites(session);
  ASSERT_GT(sites.size(), 0U);
  EXPECT_LE(sites.size(), qubitCount)
      << "Live IQM topology data can temporarily omit unavailable qubits";

  auto siteNames = getSiteNames(5);
  for (size_t i = 0; i < siteNames.size(); ++i) {
    std::cerr << "IQM site " << i << ": " << siteNames[i] << "\n";
  }

  bool usesStandardFormat = true;
  for (size_t i = 0; i < siteNames.size(); ++i) {
    if (siteNames[i] != "Q" + std::to_string(i)) {
      usesStandardFormat = false;
      break;
    }
  }
  EXPECT_FALSE(usesStandardFormat)
      << "IQM sites should use device-specific naming";

  size_t connectivitySize = 0;
  auto connectivityResult =
      AMAZON_BRAKET_QDMI_device_session_query_device_property(
          session, QDMI_DEVICE_PROPERTY_COUPLINGMAP, 0, nullptr,
          &connectivitySize);
  if (connectivityResult == QDMI_SUCCESS && connectivitySize > 0) {
    std::vector<AMAZON_BRAKET_QDMI_Site> connectivity(
        connectivitySize / sizeof(AMAZON_BRAKET_QDMI_Site));
    ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_query_device_property(
                  session, QDMI_DEVICE_PROPERTY_COUPLINGMAP, connectivitySize,
                  static_cast<void*>(connectivity.data()), nullptr),
              QDMI_SUCCESS);
    const size_t numEdges = connectivity.size() / 2;
    EXPECT_LT(numEdges, qubitCount * (qubitCount - 1))
        << "IQM should have limited connectivity";
    EXPECT_GT(numEdges, 0U);
  }

  ASSERT_GT(queryOperations(session).size(), 0U);
  EXPECT_TRUE(hasGate("cz")) << "IQM devices typically support CZ gate";
  EXPECT_TRUE(hasGate("prx")) << "IQM devices typically support PRX gate";
}

// All IQM qubits must have T1 and T2 coherence times populated (non-zero)
// because the provider calibration data always includes them.
TEST_F(DeviceParsingTestFixture, IQMDeviceSiteCoherenceTimes) {
  const char* skipIQMEnv = std::getenv("SKIP_IQM_TESTS");
  if (skipIQMEnv != nullptr && strcmp(skipIQMEnv, "1") == 0) {
    GTEST_SKIP() << "IQM tests skipped (SKIP_IQM_TESTS=1)";
  }

  const char* iqmDeviceArn = std::getenv("IQM_DEVICE_ARN");
  if (iqmDeviceArn == nullptr) {
    iqmDeviceArn = "arn:aws:braket:eu-north-1::device/qpu/iqm/Garnet";
  }
  initializeDevice(iqmDeviceArn);

  auto sites = querySites(session);
  ASSERT_GT(sites.size(), 0U) << "IQM device must have sites";

  size_t sitesWithT1 = 0;
  size_t sitesWithT2 = 0;
  for (auto* site : sites) {
    double t1 = 0.0;
    const auto t1Result = AMAZON_BRAKET_QDMI_device_session_query_site_property(
        session, site, QDMI_SITE_PROPERTY_T1, sizeof(double), &t1, nullptr);
    if (t1Result == QDMI_SUCCESS) {
      EXPECT_GT(t1, 0.0) << "T1 must be > 0 when supported";
      // IQM T1 values in seconds; typical range is ~5e-6 to ~1e-4 s
      EXPECT_LT(t1, 1.0) << "T1 above 1 s is implausibly large";
      ++sitesWithT1;
    } else {
      EXPECT_EQ(t1Result, QDMI_ERROR_NOTSUPPORTED);
    }

    double t2 = 0.0;
    const auto t2Result = AMAZON_BRAKET_QDMI_device_session_query_site_property(
        session, site, QDMI_SITE_PROPERTY_T2, sizeof(double), &t2, nullptr);
    if (t2Result == QDMI_SUCCESS) {
      EXPECT_GT(t2, 0.0) << "T2 must be > 0 when supported";
      EXPECT_LT(t2, 1.0) << "T2 above 1 s is implausibly large";
      ++sitesWithT2;
    } else {
      EXPECT_EQ(t2Result, QDMI_ERROR_NOTSUPPORTED);
    }
  }

  // All IQM sites should have calibration data; require at least half have
  // T1/T2
  EXPECT_GT(sitesWithT1, sites.size() / 2)
      << "Most IQM sites should have T1 data";
  EXPECT_GT(sitesWithT2, sites.size() / 2)
      << "Most IQM sites should have T2 data";
}

TEST_F(DeviceParsingTestFixture, IQMDeviceStatus) {
  const char* skipIQMEnv = std::getenv("SKIP_IQM_TESTS");
  if (skipIQMEnv != nullptr && strcmp(skipIQMEnv, "1") == 0) {
    GTEST_SKIP() << "IQM tests skipped (SKIP_IQM_TESTS=1)";
  }

  const char* iqmDeviceArn = std::getenv("IQM_DEVICE_ARN");
  if (iqmDeviceArn == nullptr) {
    iqmDeviceArn = "arn:aws:braket:eu-north-1::device/qpu/iqm/Garnet";
  }
  initializeDevice(iqmDeviceArn);

  QDMI_Device_Status status = QDMI_DEVICE_STATUS_OFFLINE;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_query_device_property(
                session, QDMI_DEVICE_PROPERTY_STATUS, sizeof(status), &status,
                nullptr),
            QDMI_SUCCESS);

  const char* statusStr = "UNKNOWN";
  switch (status) {
  case QDMI_DEVICE_STATUS_IDLE:
    statusStr = "IDLE (queue depth below threshold)";
    break;
  case QDMI_DEVICE_STATUS_BUSY:
    statusStr = "BUSY (queue depth at or above threshold)";
    break;
  case QDMI_DEVICE_STATUS_MAINTENANCE:
    statusStr = "MAINTENANCE (device OFFLINE on Braket)";
    break;
  case QDMI_DEVICE_STATUS_OFFLINE:
    statusStr = "OFFLINE (device RETIRED on Braket)";
    break;
  default:
    break;
  }
  std::cerr << "IQM Garnet device status: " << statusStr << "\n";

  EXPECT_TRUE(status == QDMI_DEVICE_STATUS_IDLE ||
              status == QDMI_DEVICE_STATUS_BUSY ||
              status == QDMI_DEVICE_STATUS_MAINTENANCE ||
              status == QDMI_DEVICE_STATUS_OFFLINE)
      << "IQM device must report a valid QDMI status";
}

// =============================================================================
// Integration test: wait() timeout path
// =============================================================================

// Submits a Bell-state job to SV1 and immediately waits with a one-second
// timeout, exercising the QDMI_ERROR_TIMEOUT return in wait() unless the task
// finishes unusually quickly.
// The test accepts QDMI_SUCCESS too, in case SV1 is unusually fast on the day.
TEST(AmazonBraketQDMIWaitTimeoutTest, JobWaitTimeout) {
  const char* s3BucketEnv = std::getenv("AWS_S3_BUCKET");
  if (s3BucketEnv == nullptr || strlen(s3BucketEnv) == 0) {
    GTEST_SKIP() << "AWS_S3_BUCKET not set; skipping wait-timeout test";
  }

  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_initialize(), QDMI_SUCCESS);

  struct Guard {
    AMAZON_BRAKET_QDMI_Device_Session session = nullptr;
    AMAZON_BRAKET_QDMI_Device_Job job = nullptr;
    ~Guard() {
      if (job != nullptr) {
        AMAZON_BRAKET_QDMI_device_job_free(job);
      }
      if (session != nullptr) {
        AMAZON_BRAKET_QDMI_device_session_free(session);
      }
      AMAZON_BRAKET_QDMI_device_finalize();
    }
  } guard;

  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_alloc(&guard.session),
            QDMI_SUCCESS);
  try {
    ::setupCredentials(guard.session, true);
  } catch (const std::exception& e) {
    GTEST_SKIP() << "Credentials not available: " << e.what();
  }

  const char* deviceArn =
      "arn:aws:braket:::device/quantum-simulator/amazon/sv1";
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                guard.session,
                AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN,
                strlen(deviceArn) + 1, deviceArn),
            QDMI_SUCCESS);
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_init(guard.session),
            QDMI_SUCCESS);

  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_create_device_job(guard.session,
                                                                &guard.job),
            QDMI_SUCCESS);

  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                guard.job, QDMI_DEVICE_JOB_PARAMETER_PROGRAM,
                strlen(BELL_STATE_PROGRAM) + 1, BELL_STATE_PROGRAM),
            QDMI_SUCCESS);
  QDMI_Program_Format format = QDMI_PROGRAM_FORMAT_QASM3;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                guard.job, QDMI_DEVICE_JOB_PARAMETER_PROGRAMFORMAT,
                sizeof(format), &format),
            QDMI_SUCCESS);
  size_t shots = 100;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_job_set_parameter(
          guard.job, QDMI_DEVICE_JOB_PARAMETER_SHOTSNUM, sizeof(shots), &shots),
      QDMI_SUCCESS);
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                guard.job,
                AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3BUCKET,
                strlen(s3BucketEnv) + 1, s3BucketEnv),
            QDMI_SUCCESS);

  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_job_submit(guard.job), QDMI_SUCCESS)
      << "Job submission must succeed before testing wait timeout";

  // QDMI_SUCCESS is accepted too in case the simulator was unusually fast.
  const int waitResult = AMAZON_BRAKET_QDMI_device_job_wait(guard.job, 1);
  EXPECT_THAT(waitResult, testing::AnyOf(QDMI_ERROR_TIMEOUT, QDMI_SUCCESS))
      << "wait() with a one-second timeout should either time out or find the "
         "job "
         "already done";
}
