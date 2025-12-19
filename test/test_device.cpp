/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

#include "aws-qdmi/qdmi/aws/device.h"


#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <functional>
#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>
// NOLINTNEXTLINE(misc-include-cleaner)
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {
/// Hash function for a pair
struct PairHash {
  template <class T, class U>
  auto operator()(const std::pair<T, U>& p) const noexcept -> std::size_t {
    // Use the hash of the first and second element of the pair
    return std::hash<T>{}(p.first) ^ std::hash<U>{}(p.second);
  }
};
[[nodiscard]] auto querySites(AWS_QDMI_Device_Session session)
    -> std::vector<AWS_QDMI_Site> {
  size_t size = 0;
  if (AWS_QDMI_device_session_query_device_property(
          session, QDMI_DEVICE_PROPERTY_SITES, 0, nullptr, &size) !=
      QDMI_SUCCESS) {
    throw std::runtime_error("Failed to query sites");
  }
  if (size == 0) {
    throw std::runtime_error("No sites available");
  }
  std::vector<AWS_QDMI_Site> sites(size / sizeof(AWS_QDMI_Site));
  if (AWS_QDMI_device_session_query_device_property(
          session, QDMI_DEVICE_PROPERTY_SITES, size,
          static_cast<void*>(sites.data()), nullptr) != QDMI_SUCCESS) {
    throw std::runtime_error("Failed to query sites");
  }
  return sites;
}
[[nodiscard]] auto queryOperations(AWS_QDMI_Device_Session session)
    -> std::vector<AWS_QDMI_Operation> {
  size_t size = 0;
  if (AWS_QDMI_device_session_query_device_property(
          session, QDMI_DEVICE_PROPERTY_OPERATIONS, 0, nullptr, &size) !=
      QDMI_SUCCESS) {
    throw std::runtime_error("Failed to query operations");
  }
  if (size == 0) {
    throw std::runtime_error("No operations available");
  }
  std::vector<AWS_QDMI_Operation> operations(size /
                                                sizeof(AWS_QDMI_Operation));
  if (AWS_QDMI_device_session_query_device_property(
          session, QDMI_DEVICE_PROPERTY_OPERATIONS, size,
          static_cast<void*>(operations.data()), nullptr) != QDMI_SUCCESS) {
    throw std::runtime_error("Failed to query operations");
  }
  return operations;
}
} // namespace

class AWSQDMISpecificationTest : public ::testing::Test {
protected:
  AWS_QDMI_Device_Session session = nullptr;

  void SetUp() override {
    ASSERT_EQ(AWS_QDMI_device_initialize(), QDMI_SUCCESS)
        << "Failed to initialize the device";

    ASSERT_EQ(AWS_QDMI_device_session_alloc(&session), QDMI_SUCCESS)
        << "Failed to allocate a session";

    const char* device_arn_env = std::getenv("AWS_DEVICE_ARN");
    std::string device_arn = (device_arn_env != nullptr) ? device_arn_env : "arn:aws:braket:::device/quantum-simulator/amazon/sv1";
    
    AWS_QDMI_device_session_set_parameter(
        session, static_cast<QDMI_Device_Session_Parameter>(QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN),
        device_arn.length() + 1, device_arn.c_str());

    const char* s3_bucket_env = std::getenv("AWS_S3_BUCKET");
    if (s3_bucket_env != nullptr) {
        AWS_QDMI_device_session_set_parameter(
            session, static_cast<QDMI_Device_Session_Parameter>(QDMI_DEVICE_SESSION_PARAMETER_S3BUCKET),
            strlen(s3_bucket_env) + 1, s3_bucket_env);
    }

    ASSERT_EQ(AWS_QDMI_device_session_init(session), QDMI_SUCCESS)
        << "Failed to initialize a session. Potential errors: Wrong or missing "
           "authentication information, device status is offline, or in "
           "maintenance.";
  }

  void TearDown() override {
    if (session != nullptr) {
      AWS_QDMI_device_session_free(session);
      session = nullptr;
    }
    AWS_QDMI_device_finalize();
  }
};

class AWSQDMIJobSpecificationTest : public AWSQDMISpecificationTest {
protected:
  AWS_QDMI_Device_Job job = nullptr;

  void SetUp() override {
    AWSQDMISpecificationTest::SetUp();
    ASSERT_EQ(AWS_QDMI_device_session_create_device_job(session, &job),
              QDMI_SUCCESS)
        << "Failed to create a device job.";
  }

  void TearDown() override {
    if (job != nullptr) {
      AWS_QDMI_device_job_free(job);
      job = nullptr;
    }
    AWSQDMISpecificationTest::TearDown();
  }
};

TEST_F(AWSQDMISpecificationTest, SessionAlloc) {
  EXPECT_EQ(AWS_QDMI_device_session_alloc(nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AWSQDMISpecificationTest, SessionInit) {
  EXPECT_EQ(AWS_QDMI_device_session_init(session), QDMI_ERROR_BADSTATE);
  EXPECT_EQ(AWS_QDMI_device_session_init(nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AWSQDMISpecificationTest, SessionSetParameter) {
  AWS_QDMI_Device_Session uninitializedSession = nullptr;
  ASSERT_EQ(AWS_QDMI_device_session_alloc(&uninitializedSession),
            QDMI_SUCCESS);
  EXPECT_THAT(AWS_QDMI_device_session_set_parameter(
                  uninitializedSession, QDMI_DEVICE_SESSION_PARAMETER_BASEURL,
                  20, "https://example.com"),
              testing::AnyOf(QDMI_SUCCESS, QDMI_ERROR_NOTSUPPORTED,
                             QDMI_ERROR_INVALIDARGUMENT));
  EXPECT_EQ(AWS_QDMI_device_session_set_parameter(
                session, QDMI_DEVICE_SESSION_PARAMETER_BASEURL, 20,
                "https://example.com"),
            QDMI_ERROR_BADSTATE);
  EXPECT_EQ(AWS_QDMI_device_session_set_parameter(
                session, QDMI_DEVICE_SESSION_PARAMETER_MAX, 0, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AWSQDMISpecificationTest, JobCreate) {
  AWS_QDMI_Device_Session uninitializedSession = nullptr;
  AWS_QDMI_Device_Job job = nullptr;
  ASSERT_EQ(AWS_QDMI_device_session_alloc(&uninitializedSession),
            QDMI_SUCCESS);
  EXPECT_EQ(
      AWS_QDMI_device_session_create_device_job(uninitializedSession, &job),
      QDMI_ERROR_BADSTATE);
  EXPECT_EQ(AWS_QDMI_device_session_create_device_job(session, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_EQ(AWS_QDMI_device_session_create_device_job(nullptr, &job),
            QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_THAT(AWS_QDMI_device_session_create_device_job(session, &job),
              testing::AnyOf(QDMI_SUCCESS, QDMI_ERROR_NOTSUPPORTED));
  AWS_QDMI_device_job_free(job);
}

TEST_F(AWSQDMISpecificationTest, JobSetParameter) {
  EXPECT_EQ(AWS_QDMI_device_job_set_parameter(
                nullptr, QDMI_DEVICE_JOB_PARAMETER_MAX, 0, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AWSQDMIJobSpecificationTest, JobSetParameter) {
  QDMI_Program_Format value = QDMI_PROGRAM_FORMAT_QASM2;
  EXPECT_THAT(AWS_QDMI_device_job_set_parameter(
                  job, QDMI_DEVICE_JOB_PARAMETER_PROGRAMFORMAT,
                  sizeof(QDMI_Program_Format), &value),
              testing::AnyOf(QDMI_SUCCESS, QDMI_ERROR_NOTSUPPORTED));
  EXPECT_EQ(AWS_QDMI_device_job_set_parameter(
                job, QDMI_DEVICE_JOB_PARAMETER_MAX, 0, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AWSQDMISpecificationTest, JobQueryProperty) {
  EXPECT_EQ(AWS_QDMI_device_job_query_property(
                nullptr, QDMI_DEVICE_JOB_PROPERTY_MAX, 0, nullptr, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AWSQDMIJobSpecificationTest, JobQueryProperty) {
  EXPECT_THAT(AWS_QDMI_device_job_query_property(
                  job, QDMI_DEVICE_JOB_PROPERTY_ID, 0, nullptr, nullptr),
              testing::AnyOf(QDMI_SUCCESS, QDMI_ERROR_NOTSUPPORTED));
  EXPECT_EQ(AWS_QDMI_device_job_query_property(
                job, QDMI_DEVICE_JOB_PROPERTY_MAX, 0, nullptr, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AWSQDMIJobSpecificationTest, QueryJobId) {
  size_t size = 0;
  const auto status = AWS_QDMI_device_job_query_property(
      job, QDMI_DEVICE_JOB_PROPERTY_ID, 0, nullptr, &size);
  ASSERT_THAT(status, testing::AnyOf(QDMI_SUCCESS, QDMI_ERROR_NOTSUPPORTED));
  if (status == QDMI_ERROR_NOTSUPPORTED) {
    GTEST_SKIP() << "Job ID property is not supported by the device";
  }
  ASSERT_GT(size, 0);
  std::string id(size - 1, '\0');
  EXPECT_THAT(AWS_QDMI_device_job_query_property(
                  job, QDMI_DEVICE_JOB_PROPERTY_ID, size, id.data(), nullptr),
              testing::AnyOf(QDMI_SUCCESS, QDMI_ERROR_NOTSUPPORTED));
}

TEST_F(AWSQDMISpecificationTest, JobSubmit) {
  EXPECT_EQ(AWS_QDMI_device_job_submit(nullptr), QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AWSQDMIJobSpecificationTest, JobSubmit) {
  const auto status = AWS_QDMI_device_job_submit(job);
  ASSERT_THAT(status, testing::AnyOf(QDMI_SUCCESS, QDMI_ERROR_NOTSUPPORTED));
}

TEST_F(AWSQDMISpecificationTest, JobCancel) {
  EXPECT_EQ(AWS_QDMI_device_job_cancel(nullptr), QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AWSQDMIJobSpecificationTest, JobCancel) {
  const auto status = AWS_QDMI_device_job_cancel(job);
  ASSERT_THAT(status, testing::AnyOf(QDMI_SUCCESS, QDMI_ERROR_INVALIDARGUMENT,
                                     QDMI_ERROR_NOTSUPPORTED));
}

TEST_F(AWSQDMISpecificationTest, JobCheck) {
  EXPECT_EQ(AWS_QDMI_device_job_check(nullptr, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AWSQDMIJobSpecificationTest, JobCheck) {
  QDMI_Job_Status jobStatus = QDMI_JOB_STATUS_RUNNING;
  const auto status = AWS_QDMI_device_job_check(job, &jobStatus);
  ASSERT_THAT(status, testing::AnyOf(QDMI_SUCCESS, QDMI_ERROR_NOTSUPPORTED));
}

TEST_F(AWSQDMISpecificationTest, JobWait) {
  EXPECT_EQ(AWS_QDMI_device_job_wait(nullptr, 0),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AWSQDMIJobSpecificationTest, JobWait) {
  const auto status = AWS_QDMI_device_job_wait(job, 1);
  ASSERT_THAT(status, testing::AnyOf(QDMI_SUCCESS, QDMI_ERROR_NOTSUPPORTED,
                                     QDMI_ERROR_TIMEOUT));
}

TEST_F(AWSQDMISpecificationTest, JobGetResults) {
  EXPECT_EQ(AWS_QDMI_device_job_get_results(nullptr, QDMI_JOB_RESULT_MAX, 0,
                                               nullptr, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AWSQDMIJobSpecificationTest, JobGetResults) {
  EXPECT_THAT(AWS_QDMI_device_job_get_results(job, QDMI_JOB_RESULT_SHOTS, 0,
                                                 nullptr, nullptr),
              testing::AnyOf(QDMI_SUCCESS, QDMI_ERROR_NOTSUPPORTED));
  EXPECT_EQ(AWS_QDMI_device_job_get_results(job, QDMI_JOB_RESULT_MAX, 0,
                                               nullptr, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AWSQDMISpecificationTest, QueryDeviceProperty) {
  AWS_QDMI_Device_Session uninitializedSession = nullptr;
  ASSERT_EQ(AWS_QDMI_device_session_alloc(&uninitializedSession),
            QDMI_SUCCESS);
  EXPECT_EQ(
      AWS_QDMI_device_session_query_device_property(
          uninitializedSession, QDMI_DEVICE_PROPERTY_NAME, 0, nullptr, nullptr),
      QDMI_ERROR_BADSTATE);
  EXPECT_EQ(AWS_QDMI_device_session_query_device_property(
                nullptr, QDMI_DEVICE_PROPERTY_NAME, 0, nullptr, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_EQ(AWS_QDMI_device_session_query_device_property(
                session, QDMI_DEVICE_PROPERTY_MAX, 0, nullptr, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_THAT(
      AWS_QDMI_device_session_query_device_property(
          session, QDMI_DEVICE_PROPERTY_COUPLINGMAP, 0, nullptr, nullptr),
      testing::AnyOf(QDMI_SUCCESS, QDMI_ERROR_NOTSUPPORTED));
}

TEST_F(AWSQDMISpecificationTest, QuerySiteProperty) {
  AWS_QDMI_Site site = querySites(session).front();
  EXPECT_EQ(
      AWS_QDMI_device_session_query_site_property(
          session, nullptr, QDMI_SITE_PROPERTY_INDEX, 0, nullptr, nullptr),
      QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_EQ(AWS_QDMI_device_session_query_site_property(
                nullptr, site, QDMI_SITE_PROPERTY_INDEX, 0, nullptr, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_EQ(AWS_QDMI_device_session_query_site_property(
                session, site, QDMI_SITE_PROPERTY_MAX, 0, nullptr, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_THAT(AWS_QDMI_device_session_query_site_property(
                  session, site, QDMI_SITE_PROPERTY_NAME, 0, nullptr, nullptr),
              testing::AnyOf(QDMI_SUCCESS, QDMI_ERROR_NOTSUPPORTED));
}

TEST_F(AWSQDMISpecificationTest, QueryOperationProperty) {
  AWS_QDMI_Operation operation = queryOperations(session).front();
  EXPECT_EQ(AWS_QDMI_device_session_query_operation_property(
                nullptr, operation, 0, nullptr, 0, nullptr,
                QDMI_OPERATION_PROPERTY_NAME, 0, nullptr, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_EQ(AWS_QDMI_device_session_query_operation_property(
                session, operation, 0, nullptr, 0, nullptr,
                QDMI_OPERATION_PROPERTY_MAX, 0, nullptr, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_THAT(AWS_QDMI_device_session_query_operation_property(
                  session, operation, 0, nullptr, 0, nullptr,
                  QDMI_OPERATION_PROPERTY_QUBITSNUM, 0, nullptr, nullptr),
              testing::AnyOf(QDMI_SUCCESS, QDMI_ERROR_NOTSUPPORTED));
}

TEST_F(AWSQDMISpecificationTest, QueryDeviceName) {
  size_t size = 0;
  ASSERT_EQ(AWS_QDMI_device_session_query_device_property(
                session, QDMI_DEVICE_PROPERTY_NAME, 0, nullptr, &size),
            QDMI_SUCCESS)
      << "Devices must provide a name";
  std::string value(size - 1, '\0');
  ASSERT_EQ(
      AWS_QDMI_device_session_query_device_property(
          session, QDMI_DEVICE_PROPERTY_NAME, size, value.data(), nullptr),
      QDMI_SUCCESS)
      << "Devices must provide a name";
  EXPECT_FALSE(value.empty()) << "Devices must provide a name";
}

TEST_F(AWSQDMISpecificationTest, QueryDeviceVersion) {
  size_t size = 0;
  ASSERT_EQ(AWS_QDMI_device_session_query_device_property(
                session, QDMI_DEVICE_PROPERTY_VERSION, 0, nullptr, &size),
            QDMI_SUCCESS)
      << "Devices must provide a version";
  std::string value(size - 1, '\0');
  ASSERT_EQ(
      AWS_QDMI_device_session_query_device_property(
          session, QDMI_DEVICE_PROPERTY_VERSION, size, value.data(), nullptr),
      QDMI_SUCCESS)
      << "Devices must provide a version";
  EXPECT_FALSE(value.empty()) << "Devices must provide a version";
}

TEST_F(AWSQDMISpecificationTest, QueryDeviceLibraryVersion) {
  size_t size = 0;
  ASSERT_EQ(
      AWS_QDMI_device_session_query_device_property(
          session, QDMI_DEVICE_PROPERTY_LIBRARYVERSION, 0, nullptr, &size),
      QDMI_SUCCESS)
      << "Devices must provide a library version";
  std::string value(size - 1, '\0');
  ASSERT_EQ(AWS_QDMI_device_session_query_device_property(
                session, QDMI_DEVICE_PROPERTY_LIBRARYVERSION, size,
                value.data(), nullptr),
            QDMI_SUCCESS)
      << "Devices must provide a library version";
  EXPECT_FALSE(value.empty()) << "Devices must provide a library version";
}

TEST_F(AWSQDMISpecificationTest, QueryDeviceDurationUnit) {
  size_t size = 0;
  ASSERT_EQ(AWS_QDMI_device_session_query_device_property(
                session, QDMI_DEVICE_PROPERTY_DURATIONUNIT, 0, nullptr, &size),
            QDMI_SUCCESS);
  std::string value(size - 1, '\0');
  ASSERT_EQ(AWS_QDMI_device_session_query_device_property(
                session, QDMI_DEVICE_PROPERTY_DURATIONUNIT, size, value.data(),
                nullptr),
            QDMI_SUCCESS);
  EXPECT_THAT(value, testing::AnyOf("ns", "us", "ms"));
  double scaleFactor = 0.;
  const auto result = AWS_QDMI_device_session_query_device_property(
      session, QDMI_DEVICE_PROPERTY_DURATIONSCALEFACTOR, sizeof(double),
      &scaleFactor, nullptr);
  EXPECT_THAT(result, testing::AnyOf(QDMI_SUCCESS, QDMI_ERROR_NOTSUPPORTED));
  if (result == QDMI_SUCCESS) {
    EXPECT_GT(scaleFactor, 0.);
  }
}

TEST_F(AWSQDMISpecificationTest, QuerySiteIndex) {
  size_t id = 0;
  EXPECT_NO_THROW(for (auto* site : querySites(session)) {
    EXPECT_EQ(AWS_QDMI_device_session_query_site_property(
                  session, site, QDMI_SITE_PROPERTY_INDEX, sizeof(size_t), &id,
                  nullptr),
              QDMI_SUCCESS)
        << "Devices must provide a site id";
  }) << "Devices must provide a list of sites";
}

TEST_F(AWSQDMISpecificationTest, QueryOperationName) {
  size_t nameSize = 0;
  EXPECT_NO_THROW(for (auto* operation : queryOperations(session)) {
    EXPECT_EQ(AWS_QDMI_device_session_query_operation_property(
                  session, operation, 0, nullptr, 0, nullptr,
                  QDMI_OPERATION_PROPERTY_NAME, 0, nullptr, &nameSize),
              QDMI_SUCCESS)
        << "Devices must provide a operation name";
    std::string name(nameSize - 1, '\0');
    EXPECT_EQ(AWS_QDMI_device_session_query_operation_property(
                  session, operation, 0, nullptr, 0, nullptr,
                  QDMI_OPERATION_PROPERTY_NAME, nameSize, name.data(), nullptr),
              QDMI_SUCCESS)
        << "Devices must provide a operation name";
  }) << "Devices must provide a list of operations";
}

TEST_F(AWSQDMISpecificationTest, QueryDeviceQubitNum) {
  size_t numQubits = 0;
  EXPECT_EQ(AWS_QDMI_device_session_query_device_property(
                session, QDMI_DEVICE_PROPERTY_QUBITSNUM, sizeof(size_t),
                &numQubits, nullptr),
            QDMI_SUCCESS);
}

class AWSDeviceTest : public AWSQDMISpecificationTest {
protected:
  void SetUp() override {
    AWSQDMISpecificationTest::SetUp();
  }

  void TearDown() override { AWSQDMISpecificationTest::TearDown(); }
};

TEST_F(AWSDeviceTest, QuerySiteData) {
  std::vector<AWS_QDMI_Site> sites;
  EXPECT_NO_THROW(sites = querySites(session))
      << "Devices must provide a sites";
  EXPECT_GT(sites.size(), 0);
  for (auto* site : sites) {
    uint64_t t1 = 0;
    EXPECT_EQ(AWS_QDMI_device_session_query_site_property(
                  session, site, QDMI_SITE_PROPERTY_T1, sizeof(uint64_t), &t1,
                  nullptr),
              QDMI_SUCCESS);
    
    uint64_t t2 = 0;
    EXPECT_EQ(AWS_QDMI_device_session_query_site_property(
                  session, site, QDMI_SITE_PROPERTY_T2, sizeof(uint64_t), &t2,
                  nullptr),
              QDMI_SUCCESS);
  }
}

TEST_F(AWSDeviceTest, QueryOperationData) {
  std::vector<AWS_QDMI_Operation> operations;
  EXPECT_NO_THROW(operations = queryOperations(session));
  for (auto* operation : operations) {
    size_t nameSize = 0;
    ASSERT_EQ(AWS_QDMI_device_session_query_operation_property(
                  session, operation, 0, nullptr, 0, nullptr,
                  QDMI_OPERATION_PROPERTY_NAME, 0, nullptr, &nameSize),
              QDMI_SUCCESS);
    std::string name(nameSize - 1, '\0');
    EXPECT_EQ(AWS_QDMI_device_session_query_operation_property(
                  session, operation, 0, nullptr, 0, nullptr,
                  QDMI_OPERATION_PROPERTY_NAME, nameSize, name.data(), nullptr),
              QDMI_SUCCESS);
    
    double fidelity = 0;
    auto result = AWS_QDMI_device_session_query_operation_property(
        session, operation, 0, nullptr, 0, nullptr,
        QDMI_OPERATION_PROPERTY_FIDELITY, sizeof(double), &fidelity, nullptr);
    EXPECT_THAT(result, testing::AnyOf(QDMI_SUCCESS, QDMI_ERROR_NOTSUPPORTED));
    if (result == QDMI_SUCCESS) {
      EXPECT_GE(fidelity, 0.);
      EXPECT_LE(fidelity, 1.);
    }
    
    size_t numQubits = 0;
    result = AWS_QDMI_device_session_query_operation_property(
        session, operation, 0, nullptr, 0, nullptr,
        QDMI_OPERATION_PROPERTY_QUBITSNUM, sizeof(size_t), &numQubits, nullptr);
    EXPECT_THAT(result, testing::AnyOf(QDMI_SUCCESS, QDMI_ERROR_NOTSUPPORTED));
    if (result == QDMI_SUCCESS) {
      EXPECT_GT(numQubits, 0);
    }

    size_t numParameters = 0;
    result = AWS_QDMI_device_session_query_operation_property(
                  session, operation, 0, nullptr, 0, nullptr,
                  QDMI_OPERATION_PROPERTY_PARAMETERSNUM, sizeof(size_t),
                  &numParameters, nullptr);
    EXPECT_THAT(result, testing::AnyOf(QDMI_SUCCESS, QDMI_ERROR_NOTSUPPORTED));
  }
}
