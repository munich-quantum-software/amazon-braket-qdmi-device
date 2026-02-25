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

#include "amazon-braket-qdmi-device/Constants.hpp"
#include "amazon_braket_qdmi/device.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
// Common QASM programs for testing
constexpr const char* BELL_STATE_PROGRAM = "OPENQASM 3.0;\n"
                                           "qubit[2] q;\n"
                                           "bit[2] c;\n"
                                           "h q[0];\n"
                                           "cnot q[0], q[1];\n"
                                           "c[0] = measure q[0];\n"
                                           "c[1] = measure q[1];\n";

// Helper to setup AWS credentials from environment
void setupCredentials(AMAZON_BRAKET_QDMI_Device_Session session,
                      bool failOnMissing = true) {
  const char* credsFileEnv = std::getenv("AWS_CREDENTIALS_FILE");
  if (credsFileEnv != nullptr && strlen(credsFileEnv) > 0) {
    if (AMAZON_BRAKET_QDMI_device_session_set_parameter(
            session, QDMI_DEVICE_SESSION_PARAMETER_AUTHFILE,
            strlen(credsFileEnv) + 1, credsFileEnv) != QDMI_SUCCESS) {
      if (failOnMissing)
        throw std::runtime_error("Failed to set credentials file");
    }
    return;
  }

  const char* accessKeyEnv = std::getenv("AWS_ACCESS_KEY_ID");
  const char* secretKeyEnv = std::getenv("AWS_SECRET_ACCESS_KEY");
  const char* sessionTokenEnv = std::getenv("AWS_SESSION_TOKEN");

  if (accessKeyEnv != nullptr && secretKeyEnv != nullptr) {
    if (AMAZON_BRAKET_QDMI_device_session_set_parameter(
            session, QDMI_DEVICE_SESSION_PARAMETER_AWS_ACCESS_KEY_ID,
            strlen(accessKeyEnv) + 1, accessKeyEnv) != QDMI_SUCCESS) {
      if (failOnMissing)
        throw std::runtime_error("Failed to set AWS_ACCESS_KEY_ID");
    }
    if (AMAZON_BRAKET_QDMI_device_session_set_parameter(
            session, QDMI_DEVICE_SESSION_PARAMETER_AWS_SECRET_ACCESS_KEY,
            strlen(secretKeyEnv) + 1, secretKeyEnv) != QDMI_SUCCESS) {
      if (failOnMissing)
        throw std::runtime_error("Failed to set AWS_SECRET_ACCESS_KEY");
    }
    if (sessionTokenEnv != nullptr && strlen(sessionTokenEnv) > 0) {
      AMAZON_BRAKET_QDMI_device_session_set_parameter(
          session, QDMI_DEVICE_SESSION_PARAMETER_AWS_SESSION_TOKEN,
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

    // Configure to use SV1 state vector simulator
    const char* deviceArn =
        "arn:aws:braket:::device/quantum-simulator/amazon/sv1";
    ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                  session, QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN,
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

class AmazonBraketQDMIJobSpecificationTest
    : public AmazonBraketQDMISpecificationTest {
protected:
  // Shared job and session for the whole test suite
  static AMAZON_BRAKET_QDMI_Device_Session sharedSession;
  static AMAZON_BRAKET_QDMI_Device_Job sharedJob;

  // Collected data
  static bool submittedOk;
  static int waitResult;
  static bool hasShots;
  static std::string shotsData;
  static bool hasHist;
  static std::vector<std::string> histKeys;
  static std::vector<size_t> histValues;

  AMAZON_BRAKET_QDMI_Device_Job job = nullptr;

  static void SetUpTestSuite() {
    // Create a session for the shared job
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

    // Check for credentials - Method 1: credentials file (local development)
    const char* credsFileEnv = std::getenv("AWS_CREDENTIALS_FILE");
    if (credsFileEnv != nullptr && strlen(credsFileEnv) > 0) {
      std::cerr << "INFO: Using credentials file: " << credsFileEnv << "\n";
      if (AMAZON_BRAKET_QDMI_device_session_set_parameter(
              sharedSession, QDMI_DEVICE_SESSION_PARAMETER_AUTHFILE,
              strlen(credsFileEnv) + 1, credsFileEnv) != QDMI_SUCCESS) {
        GTEST_FAIL() << "Failed to set credentials file in SetUpTestSuite";
        return;
      }
    } else {
      // Method 2: direct credentials via environment variables (CI/CD)
      const char* accessKeyEnv = std::getenv("AWS_ACCESS_KEY_ID");
      const char* secretKeyEnv = std::getenv("AWS_SECRET_ACCESS_KEY");
      const char* sessionTokenEnv = std::getenv("AWS_SESSION_TOKEN");

      if (accessKeyEnv != nullptr && secretKeyEnv != nullptr) {
        std::cerr << "INFO: Using direct credential environment variables\n";
        if (AMAZON_BRAKET_QDMI_device_session_set_parameter(
                sharedSession, QDMI_DEVICE_SESSION_PARAMETER_AWS_ACCESS_KEY_ID,
                strlen(accessKeyEnv) + 1, accessKeyEnv) != QDMI_SUCCESS) {
          GTEST_FAIL() << "Failed to set AWS_ACCESS_KEY_ID in SetUpTestSuite";
          return;
        }
        if (AMAZON_BRAKET_QDMI_device_session_set_parameter(
                sharedSession,
                QDMI_DEVICE_SESSION_PARAMETER_AWS_SECRET_ACCESS_KEY,
                strlen(secretKeyEnv) + 1, secretKeyEnv) != QDMI_SUCCESS) {
          GTEST_FAIL()
              << "Failed to set AWS_SECRET_ACCESS_KEY in SetUpTestSuite";
          return;
        }
        if (sessionTokenEnv != nullptr && strlen(sessionTokenEnv) > 0) {
          if (AMAZON_BRAKET_QDMI_device_session_set_parameter(
                  sharedSession,
                  QDMI_DEVICE_SESSION_PARAMETER_AWS_SESSION_TOKEN,
                  strlen(sessionTokenEnv) + 1,
                  sessionTokenEnv) != QDMI_SUCCESS) {
            GTEST_FAIL() << "Failed to set AWS_SESSION_TOKEN in SetUpTestSuite";
            return;
          }
        }
      } else {
        GTEST_FAIL() << "No credentials provided. Set either:\n"
                     << "  1. AWS_CREDENTIALS_FILE (path to credentials file)\n"
                     << "  2. AWS_ACCESS_KEY_ID + AWS_SECRET_ACCESS_KEY "
                        "(direct credentials)";
        return;
      }
    }

    // Configure to use SV1 state vector simulator
    const char* deviceArn =
        "arn:aws:braket:::device/quantum-simulator/amazon/sv1";
    if (AMAZON_BRAKET_QDMI_device_session_set_parameter(
            sharedSession, QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN,
            strlen(deviceArn) + 1, deviceArn) != QDMI_SUCCESS) {
      GTEST_FAIL() << "Failed to set device ARN in SetUpTestSuite";
      return;
    }

    if (AMAZON_BRAKET_QDMI_device_session_init(sharedSession) != QDMI_SUCCESS) {
      GTEST_SKIP() << "session_init failed in SetUpTestSuite; skipping job "
                      "submission tests";
      return;
    }

    // create job
    if (AMAZON_BRAKET_QDMI_device_session_create_device_job(
            sharedSession, &sharedJob) != QDMI_SUCCESS) {
      GTEST_SKIP() << "create_device_job failed in SetUpTestSuite; skipping "
                      "job submission tests";
      return;
    }

    // set program/format/shots
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

    // Set S3 bucket from environment variable (required)
    const char* s3BucketEnv = std::getenv("AWS_S3_BUCKET");
    if (s3BucketEnv == nullptr || strlen(s3BucketEnv) == 0) {
      GTEST_SKIP()
          << "AWS_S3_BUCKET environment variable not set; skipping job "
             "submission tests";
      return;
    }
    AMAZON_BRAKET_QDMI_device_job_set_parameter(
        sharedJob, QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3BUCKET,
        strlen(s3BucketEnv) + 1, s3BucketEnv);

    // submit
    if (const auto submitStatus =
            AMAZON_BRAKET_QDMI_device_job_submit(sharedJob);
        submitStatus != QDMI_SUCCESS) {
      // allow not-supported
      if (submitStatus == QDMI_ERROR_NOTSUPPORTED) {
        GTEST_SKIP() << "job_submit not supported; skipping job result tests";
        return;
      }
      GTEST_FAIL() << "job_submit failed with status " << submitStatus;
      return;
    }
    submittedOk = true;

    // wait for completion (timeout: 120 seconds)
    waitResult = AMAZON_BRAKET_QDMI_device_job_wait(sharedJob, 120000);

    if (waitResult == QDMI_SUCCESS) {
      // check final status
      QDMI_Job_Status finalStatus = QDMI_JOB_STATUS_CREATED;
      if (AMAZON_BRAKET_QDMI_device_job_check(sharedJob, &finalStatus) ==
              QDMI_SUCCESS &&
          finalStatus == QDMI_JOB_STATUS_DONE) {
        // fetch SHOTS
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
        // fetch histogram keys
        size_t keysSize = 0;
        if (AMAZON_BRAKET_QDMI_device_job_get_results(
                sharedJob, QDMI_JOB_RESULT_HIST_KEYS, 0, nullptr, &keysSize) ==
                QDMI_SUCCESS &&
            keysSize > 0) {
          std::vector<char> keysData(keysSize);
          if (AMAZON_BRAKET_QDMI_device_job_get_results(
                  sharedJob, QDMI_JOB_RESULT_HIST_KEYS, keysSize,
                  keysData.data(), nullptr) == QDMI_SUCCESS) {
            // parse null-separated keys
            const char* ptr = keysData.data();
            const char* end = ptr + keysSize;
            while (ptr < end && *ptr != '\0') {
              histKeys.emplace_back(ptr);
              ptr += strlen(ptr) + 1;
            }
          }
        }
        // fetch histogram values
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

  void SetUp() override {
    // use shared job handle
    job = sharedJob;
  }

  void TearDown() override {
    job = nullptr; // do not free shared job here
  }
};

// Static member definitions
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

TEST_F(AmazonBraketQDMISpecificationTest, SessionAlloc) {
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_alloc(nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AmazonBraketQDMISpecificationTest, SessionInit) {
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_init(session),
            QDMI_ERROR_BADSTATE);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_init(nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AmazonBraketQDMISpecificationTest, SessionSetParameter) {
  AMAZON_BRAKET_QDMI_Device_Session uninitializedSession = nullptr;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_alloc(&uninitializedSession),
            QDMI_SUCCESS);
  EXPECT_THAT(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                  uninitializedSession, QDMI_DEVICE_SESSION_PARAMETER_BASEURL,
                  20, "https://example.com"),
              testing::AnyOf(QDMI_SUCCESS, QDMI_ERROR_NOTSUPPORTED,
                             QDMI_ERROR_INVALIDARGUMENT));
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                session, QDMI_DEVICE_SESSION_PARAMETER_BASEURL, 20,
                "https://example.com"),
            QDMI_ERROR_BADSTATE);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                session, QDMI_DEVICE_SESSION_PARAMETER_MAX, 0, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
  AMAZON_BRAKET_QDMI_device_session_free(uninitializedSession);
}

TEST_F(AmazonBraketQDMISpecificationTest, SessionCredentialsFile) {
  // Test that we can set a credentials file parameter
  AMAZON_BRAKET_QDMI_Device_Session credsSession = nullptr;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_alloc(&credsSession),
            QDMI_SUCCESS);

  // Try to use the local credentials file if it exists
  const char* credsFile = ".aws/credentials";
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                credsSession, QDMI_DEVICE_SESSION_PARAMETER_AUTHFILE,
                strlen(credsFile) + 1, credsFile),
            QDMI_SUCCESS)
      << "Failed to set credentials file parameter";

  // Set device ARN
  const char* deviceArn =
      "arn:aws:braket:::device/quantum-simulator/amazon/sv1";
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                credsSession, QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN,
                strlen(deviceArn) + 1, deviceArn),
            QDMI_SUCCESS);

  // Try to initialize with credentials file
  // This may fail if file doesn't exist or has invalid credentials
  auto initResult = AMAZON_BRAKET_QDMI_device_session_init(credsSession);
  if (initResult == QDMI_SUCCESS) {
    std::cerr
        << "INFO: Successfully initialized session with credentials file\n";
  } else {
    std::cerr << "INFO: Could not initialize with credentials file (may not "
                 "exist or invalid)\n";
  }

  AMAZON_BRAKET_QDMI_device_session_free(credsSession);
}

TEST_F(AmazonBraketQDMISpecificationTest, SessionDirectCredentials) {
  // Test that we can set credentials directly via CUSTOM parameters
  AMAZON_BRAKET_QDMI_Device_Session credsSession = nullptr;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_alloc(&credsSession),
            QDMI_SUCCESS);

  // Set credentials via custom parameters
  const char* accessKey = "AKIAIOSFODNN7EXAMPLE";
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                credsSession, QDMI_DEVICE_SESSION_PARAMETER_AWS_ACCESS_KEY_ID,
                strlen(accessKey) + 1, accessKey),
            QDMI_SUCCESS)
      << "Failed to set AWS_ACCESS_KEY_ID parameter";

  const char* secretKey = "wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY";
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                credsSession,
                QDMI_DEVICE_SESSION_PARAMETER_AWS_SECRET_ACCESS_KEY,
                strlen(secretKey) + 1, secretKey),
            QDMI_SUCCESS)
      << "Failed to set AWS_SECRET_ACCESS_KEY parameter";

  // Session token is optional
  const char* sessionToken = "FakeSessionToken123";
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                credsSession, QDMI_DEVICE_SESSION_PARAMETER_AWS_SESSION_TOKEN,
                strlen(sessionToken) + 1, sessionToken),
            QDMI_SUCCESS)
      << "Failed to set AWS_SESSION_TOKEN parameter";

  // Note: We don't actually initialize because these are fake credentials
  // Just testing that the parameters can be set
  AMAZON_BRAKET_QDMI_device_session_free(credsSession);
}

TEST_F(AmazonBraketQDMISpecificationTest, JobCreate) {
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

TEST_F(AmazonBraketQDMISpecificationTest, JobSetParameter) {
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                nullptr, QDMI_DEVICE_JOB_PARAMETER_MAX, 0, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AmazonBraketQDMIJobSpecificationTest, JobSetParameter) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_create_device_job(sharedSession,
                                                                &freshJob),
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

TEST_F(AmazonBraketQDMIJobSpecificationTest, JobSetParameterProgram) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_create_device_job(sharedSession,
                                                                &freshJob),
            QDMI_SUCCESS);

  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                freshJob, QDMI_DEVICE_JOB_PARAMETER_PROGRAM,
                strlen(BELL_STATE_PROGRAM) + 1, BELL_STATE_PROGRAM),
            QDMI_SUCCESS);

  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

TEST_F(AmazonBraketQDMIJobSpecificationTest, JobSetParameterS3Bucket) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_create_device_job(sharedSession,
                                                                &freshJob),
            QDMI_SUCCESS);

  // Test setting S3 bucket
  const char* s3Bucket = "test-job-specific-results-bucket";
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                freshJob, QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3BUCKET,
                strlen(s3Bucket) + 1, s3Bucket),
            QDMI_SUCCESS);

  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

TEST_F(AmazonBraketQDMIJobSpecificationTest, JobSetParameterS3Prefix) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_create_device_job(sharedSession,
                                                                &freshJob),
            QDMI_SUCCESS);

  // Test setting S3 prefix
  const char* s3Prefix = "my-experiment/run-42/";
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                freshJob, QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3PREFIX,
                strlen(s3Prefix) + 1, s3Prefix),
            QDMI_SUCCESS);

  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

TEST_F(AmazonBraketQDMIJobSpecificationTest, JobSetParameterS3InvalidArgument) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_create_device_job(sharedSession,
                                                                &freshJob),
            QDMI_SUCCESS);

  // Test null value for S3 bucket
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                freshJob, QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3BUCKET, 0, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);

  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

// Integration test: Submit job with S3 configuration
TEST(AmazonBraketQDMIPerJobS3Test, SubmitJobWithPerJobS3) {
  // Only run if we have AWS credentials and S3 bucket configured
  const char* s3BucketEnv = std::getenv("AWS_S3_BUCKET");
  if (s3BucketEnv == nullptr || strlen(s3BucketEnv) == 0) {
    GTEST_SKIP() << "AWS_S3_BUCKET not set, skipping S3 integration test";
  }

  // Initialize and create session
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_initialize(), QDMI_SUCCESS);

  AMAZON_BRAKET_QDMI_Device_Session session = nullptr;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_alloc(&session), QDMI_SUCCESS);

  // Setup credentials
  try {
    ::setupCredentials(session, true);
  } catch (const std::exception& e) {
    GTEST_SKIP() << "Credentials not available: " << e.what();
  }

  // Configure SV1 state vector simulator programmatically
  const char* deviceArn =
      "arn:aws:braket:::device/quantum-simulator/amazon/sv1";
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                session, QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN,
                strlen(deviceArn) + 1, deviceArn),
            QDMI_SUCCESS);

  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_init(session), QDMI_SUCCESS);

  // Create job
  AMAZON_BRAKET_QDMI_Device_Job job = nullptr;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &job),
            QDMI_SUCCESS);

  // Set program (simple Bell state)
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                job, QDMI_DEVICE_JOB_PARAMETER_PROGRAM,
                strlen(BELL_STATE_PROGRAM) + 1, BELL_STATE_PROGRAM),
            QDMI_SUCCESS);

  // Set format
  QDMI_Program_Format format = QDMI_PROGRAM_FORMAT_QASM3;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                job, QDMI_DEVICE_JOB_PARAMETER_PROGRAMFORMAT, sizeof(format),
                &format),
            QDMI_SUCCESS);

  // Set shots
  size_t shots = 100;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                job, QDMI_DEVICE_JOB_PARAMETER_SHOTSNUM, sizeof(shots), &shots),
            QDMI_SUCCESS);

  // Set S3 bucket
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                job, QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3BUCKET,
                strlen(s3BucketEnv) + 1, s3BucketEnv),
            QDMI_SUCCESS);

  // Submit job
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_job_submit(job), QDMI_SUCCESS)
      << "Job submission should succeed with S3 configuration";

  // Wait for completion (60 seconds for SV1 should be plenty)
  int waitStatus = AMAZON_BRAKET_QDMI_device_job_wait(job, 60000);
  EXPECT_EQ(waitStatus, QDMI_SUCCESS) << "Job should complete successfully";

  // Check final status
  QDMI_Job_Status finalStatus = QDMI_JOB_STATUS_CREATED;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_job_check(job, &finalStatus),
            QDMI_SUCCESS);
  EXPECT_EQ(finalStatus, QDMI_JOB_STATUS_DONE)
      << "Job should reach DONE status";

  // Verify we can retrieve results
  size_t shotsSize = 0;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_get_results(
                job, QDMI_JOB_RESULT_SHOTS, 0, nullptr, &shotsSize),
            QDMI_SUCCESS);
  EXPECT_GT(shotsSize, 0u) << "Should have measurement results";

  // Verify histogram results (Bell state should give 00 and 11)
  size_t histKeysSize = 0;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_job_get_results(
                job, QDMI_JOB_RESULT_HIST_KEYS, 0, nullptr, &histKeysSize),
            QDMI_SUCCESS);
  ASSERT_GT(histKeysSize, 0u);

  std::string histKeysData(histKeysSize - 1, '\0');
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_job_get_results(
                job, QDMI_JOB_RESULT_HIST_KEYS, histKeysSize,
                histKeysData.data(), nullptr),
            QDMI_SUCCESS);

  // Parse keys (null-terminated strings)
  bool found00 = histKeysData.find("00") != std::string::npos;
  bool found11 = histKeysData.find("11") != std::string::npos;
  EXPECT_TRUE(found00 && found11)
      << "Bell state should produce 00 and 11 outcomes";

  // Cleanup
  AMAZON_BRAKET_QDMI_device_job_free(job);
  AMAZON_BRAKET_QDMI_device_session_free(session);
  AMAZON_BRAKET_QDMI_device_finalize();
}

TEST_F(AmazonBraketQDMISpecificationTest, JobQueryProperty) {
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_query_property(
                nullptr, QDMI_DEVICE_JOB_PROPERTY_MAX, 0, nullptr, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AmazonBraketQDMIJobSpecificationTest, JobQueryProperty) {
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_query_property(
                job, QDMI_DEVICE_JOB_PROPERTY_MAX, 0, nullptr, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AmazonBraketQDMISpecificationTest, JobSubmit) {
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_submit(nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AmazonBraketQDMIJobSpecificationTest, JobSubmit) {
  if (!submittedOk) {
    GTEST_SKIP() << "Shared job was not submitted in suite setup";
  }
  EXPECT_TRUE(submittedOk);
}

TEST_F(AmazonBraketQDMISpecificationTest, JobCancel) {
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_cancel(nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AmazonBraketQDMIJobSpecificationTest, JobCancel) {
  const auto status = AMAZON_BRAKET_QDMI_device_job_cancel(job);
  ASSERT_THAT(status, testing::AnyOf(QDMI_SUCCESS, QDMI_ERROR_INVALIDARGUMENT,
                                     QDMI_ERROR_NOTSUPPORTED));
}

TEST_F(AmazonBraketQDMISpecificationTest, JobCheck) {
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_check(nullptr, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AmazonBraketQDMIJobSpecificationTest, JobCheck) {
  QDMI_Job_Status jobStatus = QDMI_JOB_STATUS_RUNNING;
  const auto status = AMAZON_BRAKET_QDMI_device_job_check(job, &jobStatus);
  ASSERT_EQ(status, QDMI_SUCCESS);
}

TEST_F(AmazonBraketQDMISpecificationTest, JobWait) {
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_wait(nullptr, 0),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AmazonBraketQDMIJobSpecificationTest, JobWait) {
  if (!submittedOk) {
    GTEST_SKIP() << "Job was not submitted in suite setup";
  }
  // Check the stored wait result
  EXPECT_EQ(waitResult, QDMI_SUCCESS);
}

TEST_F(AmazonBraketQDMISpecificationTest, JobGetResults) {
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_get_results(
                nullptr, QDMI_JOB_RESULT_MAX, 0, nullptr, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AmazonBraketQDMIJobSpecificationTest, JobGetResults) {
  if (!submittedOk) {
    GTEST_SKIP() << "Job was not submitted in suite setup";
  }
  ASSERT_TRUE(hasShots) << "SHOTS results must be available for all devices";
  // Basic checks on fetched shots data
  EXPECT_GT(shotsData.size(), 0U);
  // Ensure the API can be called again for size and retrieval
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

  // For a Bell state, we expect 00 and 11
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
        << "Devices must provide a operation name";
    std::string name(nameSize - 1, '\0');
    EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_query_operation_property(
                  session, operation, 0, nullptr, 0, nullptr,
                  QDMI_OPERATION_PROPERTY_NAME, nameSize, name.data(), nullptr),
              QDMI_SUCCESS)
        << "Devices must provide a operation name";
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
  EXPECT_NO_THROW(sites = querySites(session))
      << "Devices must provide a sites";
  EXPECT_GT(sites.size(), 0);
  for (auto* site : sites) {
    uint64_t t1 = 0;
    // T1/T2 may not be supported for simulators
    EXPECT_THAT(AMAZON_BRAKET_QDMI_device_session_query_site_property(
                    session, site, QDMI_SITE_PROPERTY_T1, sizeof(uint64_t), &t1,
                    nullptr),
                testing::AnyOf(QDMI_SUCCESS, QDMI_ERROR_NOTSUPPORTED));

    uint64_t t2 = 0;
    // T1/T2 may not be supported for simulators
    EXPECT_THAT(AMAZON_BRAKET_QDMI_device_session_query_site_property(
                    session, site, QDMI_SITE_PROPERTY_T2, sizeof(uint64_t), &t2,
                    nullptr),
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
    EXPECT_THAT(result, testing::AnyOf(QDMI_SUCCESS, QDMI_ERROR_NOTSUPPORTED));
    if (result == QDMI_SUCCESS) {
      EXPECT_GE(fidelity, 0.);
      EXPECT_LE(fidelity, 1.);
    }

    size_t numQubits = 0;
    result = AMAZON_BRAKET_QDMI_device_session_query_operation_property(
        session, operation, 0, nullptr, 0, nullptr,
        QDMI_OPERATION_PROPERTY_QUBITSNUM, sizeof(size_t), &numQubits, nullptr);
    EXPECT_EQ(result, QDMI_SUCCESS);
    EXPECT_GT(numQubits, 0);

    size_t numParameters = 0;
    result = AMAZON_BRAKET_QDMI_device_session_query_operation_property(
        session, operation, 0, nullptr, 0, nullptr,
        QDMI_OPERATION_PROPERTY_PARAMETERSNUM, sizeof(size_t), &numParameters,
        nullptr);
    EXPECT_EQ(result, QDMI_SUCCESS);
  }
}

// =============================================================================
// Device-Specific Parsing Tests
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
                  session, QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN,
                  strlen(deviceArn) + 1, deviceArn),
              QDMI_SUCCESS);

    auto initResult = AMAZON_BRAKET_QDMI_device_session_init(session);
    if (initResult != QDMI_SUCCESS) {
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

  std::vector<std::string> getSiteNames(size_t maxCount = 5) {
    auto sites = querySites(session);
    std::vector<std::string> names;

    for (size_t i = 0; i < std::min(maxCount, sites.size()); ++i) {
      size_t nameSize = 0;
      AMAZON_BRAKET_QDMI_device_session_query_site_property(
          session, sites[i], QDMI_SITE_PROPERTY_NAME, 0, nullptr, &nameSize);
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
      AMAZON_BRAKET_QDMI_device_session_query_operation_property(
          session, op, 0, nullptr, 0, nullptr, QDMI_OPERATION_PROPERTY_NAME, 0,
          nullptr, &nameSize);
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
  const char* deviceArn =
      "arn:aws:braket:::device/quantum-simulator/amazon/sv1";
  initializeDevice(deviceArn);

  // Verify device name
  std::string deviceName = getDeviceName();
  EXPECT_EQ(deviceName, "SV1") << "SV1 device name should be 'SV1'";

  // Verify qubit count
  size_t qubitCount = getQubitCount();
  EXPECT_GT(qubitCount, 0u) << "SV1 should have qubits";
  EXPECT_LE(qubitCount, 34u) << "SV1 supports up to 34 qubits";

  // Verify sites use standard numeric IDs (Q0, Q1, Q2, ...)
  auto sites = querySites(session);
  ASSERT_GT(sites.size(), 0u) << "SV1 should have sites";
  EXPECT_EQ(sites.size(), qubitCount)
      << "Number of sites should match qubit count";

  auto siteNames = getSiteNames(3);
  for (size_t i = 0; i < siteNames.size(); ++i) {
    std::string expectedName = "Q" + std::to_string(i);
    EXPECT_EQ(siteNames[i], expectedName)
        << "SV1 sites should use standard format Q0, Q1, Q2, ...";
  }

  // Verify full connectivity
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
                  connectivity.data(), nullptr),
              QDMI_SUCCESS);

    size_t numEdges = connectivity.size() / 2;
    size_t expectedEdges = qubitCount * (qubitCount - 1);
    EXPECT_EQ(numEdges, expectedEdges)
        << "SV1 should have full connectivity (all-to-all)";
  }

  // Verify standard gate set
  std::vector<std::string> expectedGates = {"h",    "x",  "y",  "z",
                                            "cnot", "rx", "ry", "rz"};
  for (const auto& gate : expectedGates) {
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

  // Verify device name
  std::string deviceName = getDeviceName();
  EXPECT_FALSE(deviceName.empty()) << "IQM device should have a name";
  std::cerr << "IQM device name: " << deviceName << "\n";

  // Verify qubit count
  size_t qubitCount = getQubitCount();
  EXPECT_GT(qubitCount, 0u) << "IQM device should have qubits";

  // Verify sites use IQM-specific naming (not standard Q0, Q1 format)
  auto sites = querySites(session);
  ASSERT_GT(sites.size(), 0u) << "IQM device should have sites";
  EXPECT_EQ(sites.size(), qubitCount)
      << "Number of sites should match qubit count";

  auto siteNames = getSiteNames(5);
  for (size_t i = 0; i < siteNames.size(); ++i) {
    std::cerr << "IQM site " << i << ": " << siteNames[i] << "\n";
  }

  // Verify IQM parser was used (not standard simulator format)
  bool usesStandardFormat = true;
  for (size_t i = 0; i < siteNames.size(); ++i) {
    std::string expectedStandardName = "Q" + std::to_string(i);
    if (siteNames[i] != expectedStandardName) {
      usesStandardFormat = false;
      break;
    }
  }
  EXPECT_FALSE(usesStandardFormat) << "IQM sites should use device-specific "
                                      "naming (verified IQM parser was used)";

  // Verify limited connectivity
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
                  connectivity.data(), nullptr),
              QDMI_SUCCESS);

    size_t numEdges = connectivity.size() / 2;
    size_t fullConnectivityEdges = qubitCount * (qubitCount - 1);
    EXPECT_LT(numEdges, fullConnectivityEdges)
        << "IQM should have limited connectivity (not all-to-all)";
    EXPECT_GT(numEdges, 0u) << "IQM should have some connectivity";
  }

  // Verify IQM-specific gates
  auto operations = queryOperations(session);
  ASSERT_GT(operations.size(), 0u) << "IQM device should have operations";

  EXPECT_TRUE(hasGate("cz")) << "IQM devices typically support CZ gate";
  EXPECT_TRUE(hasGate("prx")) << "IQM devices typically support PRX gate";
}
