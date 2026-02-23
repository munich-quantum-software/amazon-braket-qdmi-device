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

    const char* deviceArnEnv = std::getenv("AWS_DEVICE_ARN");
    (void)deviceArnEnv; // Suppress unused warning, handled by library env read

    const char* s3BucketEnv = std::getenv("AWS_S3_BUCKET");
    (void)s3BucketEnv; // Suppress unused warning, handled by library env read

    ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_init(session), QDMI_SUCCESS)
        << "Failed to initialize a session. Potential errors: Wrong or missing "
           "authentication information, device status is offline, or in "
           "maintenance.";
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

    const char* deviceArnEnv = std::getenv("AWS_DEVICE_ARN");
    (void)deviceArnEnv; // Suppress unused warning

    const char* s3BucketEnv = std::getenv("AWS_S3_BUCKET");
    (void)s3BucketEnv; // Suppress unused warning

    const char* regionEnv = std::getenv("AWS_DEFAULT_REGION");
    (void)regionEnv; // Unused variable warning suppression

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
    const char* program = "OPENQASM 3.0;\nqubit[2] q;\nh q[0];\ncnot q[0], "
                          "q[1];\nbit[2] c;\nc = measure q;\n";
    AMAZON_BRAKET_QDMI_device_job_set_parameter(
        sharedJob, QDMI_DEVICE_JOB_PARAMETER_PROGRAM, strlen(program) + 1,
        program);
    QDMI_Program_Format format = QDMI_PROGRAM_FORMAT_QASM3;
    AMAZON_BRAKET_QDMI_device_job_set_parameter(
        sharedJob, QDMI_DEVICE_JOB_PARAMETER_PROGRAMFORMAT, sizeof(format),
        &format);
    size_t shots = 100;
    AMAZON_BRAKET_QDMI_device_job_set_parameter(
        sharedJob, QDMI_DEVICE_JOB_PARAMETER_SHOTSNUM, sizeof(shots), &shots);

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

  const char* program = "OPENQASM 3.0;\n"
                        "qubit[2] q;\n"
                        "h q[0];\n"
                        "cnot q[0], q[1];\n"
                        "bit[2] c;\n"
                        "c = measure q;\n";
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                freshJob, QDMI_DEVICE_JOB_PARAMETER_PROGRAM,
                strlen(program) + 1, program),
            QDMI_SUCCESS);

  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
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

TEST_F(AmazonBraketQDMISpecificationTest, QueryDeviceVersion) {
  size_t size = 0;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_query_device_property(
                session, QDMI_DEVICE_PROPERTY_VERSION, 0, nullptr, &size),
            QDMI_SUCCESS)
      << "Devices must provide a version";
  std::string value(size - 1, '\0');
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_query_device_property(
          session, QDMI_DEVICE_PROPERTY_VERSION, size, value.data(), nullptr),
      QDMI_SUCCESS)
      << "Devices must provide a version";
  EXPECT_FALSE(value.empty()) << "Devices must provide a version";
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

TEST_F(AmazonBraketQDMISpecificationTest, QueryDeviceDurationUnit) {
  size_t size = 0;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_query_device_property(
                session, QDMI_DEVICE_PROPERTY_DURATIONUNIT, 0, nullptr, &size),
            QDMI_SUCCESS);
  std::string value(size - 1, '\0');
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_query_device_property(
                session, QDMI_DEVICE_PROPERTY_DURATIONUNIT, size, value.data(),
                nullptr),
            QDMI_SUCCESS);
  EXPECT_THAT(value, testing::AnyOf("ns", "us", "ms"));
  double scaleFactor = 0.;
  const auto result = AMAZON_BRAKET_QDMI_device_session_query_device_property(
      session, QDMI_DEVICE_PROPERTY_DURATIONSCALEFACTOR, sizeof(double),
      &scaleFactor, nullptr);
  EXPECT_EQ(result, QDMI_SUCCESS);
  EXPECT_GT(scaleFactor, 0.);
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
