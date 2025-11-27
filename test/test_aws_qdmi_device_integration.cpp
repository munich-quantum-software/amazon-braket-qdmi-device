#include <array>
#include <aws_qdmi/device.h>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <gtest/gtest.h>
#include <iostream>
#include <map>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

class AWSQDMIIntegrationTest : public testing::Test {
protected:
  AWS_QDMI_Device_Session session = nullptr;

  void SetUp() override {
    EXPECT_EQ(AWS_QDMI_device_initialize(), QDMI_SUCCESS);
    
    const auto* region_env = std::getenv("AWS_REGION");
    if (region_env == nullptr) {
      std::cerr << "[WARN] Environment variable AWS_REGION is not set. "
                   "Using default us-east-1.\n";
    }
    const std::string region = (region_env != nullptr) ? region_env : "us-east-1";

    const auto* device_arn_env = std::getenv("AWS_DEVICE_ARN");
    if (device_arn_env == nullptr) {
      std::cerr << "[WARN] Environment variable AWS_DEVICE_ARN is not set. "
                   "Some tests may fail.\n";
    }
    const std::string device_arn = (device_arn_env != nullptr) ? device_arn_env : "";

    const auto* s3_bucket_env = std::getenv("AWS_S3_BUCKET");
    const std::string s3_bucket = (s3_bucket_env != nullptr) ? s3_bucket_env : "";

    ASSERT_EQ(AWS_QDMI_device_session_alloc(&session), QDMI_SUCCESS);
    
    // Set session parameters
    ASSERT_EQ(AWS_QDMI_device_session_set_parameter(
        session, static_cast<QDMI_Device_Session_Parameter>(QDMI_DEVICE_SESSION_PARAMETER_REGION), 
        region.length() + 1, region.c_str()), QDMI_SUCCESS);
    
    if (!device_arn.empty()) {
      ASSERT_EQ(AWS_QDMI_device_session_set_parameter(
          session, static_cast<QDMI_Device_Session_Parameter>(QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN),
          device_arn.length() + 1, device_arn.c_str()), QDMI_SUCCESS);
    }
    
    if (!s3_bucket.empty()) {
      ASSERT_EQ(AWS_QDMI_device_session_set_parameter(
          session, static_cast<QDMI_Device_Session_Parameter>(QDMI_DEVICE_SESSION_PARAMETER_S3BUCKET),
          s3_bucket.length() + 1, s3_bucket.c_str()), QDMI_SUCCESS);
    }
    
    ASSERT_EQ(AWS_QDMI_device_session_init(session), QDMI_SUCCESS);
  }

  void TearDown() override {
    AWS_QDMI_device_session_free(session);
    EXPECT_EQ(AWS_QDMI_device_finalize(), QDMI_SUCCESS);
  }

  // Simple Bell state circuit in OpenQASM 3
  static constexpr auto TEST_CIRCUIT_QASM3 = R"(
OPENQASM 3.0;
qubit[2] q;
bit[2] c;

h q[0];
cnot q[0], q[1];

c[0] = measure q[0];
c[1] = measure q[1];
)";

  // GHZ state circuit in OpenQASM 2
  static constexpr auto TEST_CIRCUIT_QASM2 = R"(
OPENQASM 2.0;
include "qelib1.inc";
qreg q[3];
creg c[3];

h q[0];
cx q[0], q[1];
cx q[1], q[2];

measure q[0] -> c[0];
measure q[1] -> c[1];
measure q[2] -> c[2];
)";

  // Helper to get string property
  std::string getStringProperty(QDMI_Device_Property prop) const {
    size_t size = 0;
    EXPECT_EQ(AWS_QDMI_device_session_query_device_property(
        session, prop, 0, nullptr, &size), QDMI_SUCCESS);
    if (size == 0) return "";
    
    std::string result(size - 1, '\0');
    EXPECT_EQ(AWS_QDMI_device_session_query_device_property(
        session, prop, size, result.data(), nullptr), QDMI_SUCCESS);
    return result;
  }

  // Helper to get size_t property
  size_t getSizeTProperty(QDMI_Device_Property prop) const {
    size_t value = 0;
    EXPECT_EQ(AWS_QDMI_device_session_query_device_property(
        session, prop, sizeof(size_t), &value, nullptr), QDMI_SUCCESS);
    return value;
  }

  // Helper to get list property
  template<typename T>
  std::vector<T> getListProperty(QDMI_Device_Property prop) const {
    size_t size = 0;
    EXPECT_EQ(AWS_QDMI_device_session_query_device_property(
        session, prop, 0, nullptr, &size), QDMI_SUCCESS);
    if (size == 0) return {};
    
    std::vector<T> result(size / sizeof(T));
    EXPECT_EQ(AWS_QDMI_device_session_query_device_property(
        session, prop, size, result.data(), nullptr), QDMI_SUCCESS);
    return result;
  }

  // Helper to get site string property
  std::string getSiteStringProperty(AWS_QDMI_Site site, QDMI_Site_Property prop) const {
    size_t size = 0;
    EXPECT_EQ(AWS_QDMI_device_session_query_site_property(
        session, site, prop, 0, nullptr, &size), QDMI_SUCCESS);
    if (size == 0) return "";
    
    std::string result(size - 1, '\0');
    EXPECT_EQ(AWS_QDMI_device_session_query_site_property(
        session, site, prop, size, result.data(), nullptr), QDMI_SUCCESS);
    return result;
  }

  // Helper to get site uint64_t property
  uint64_t getSiteUInt64Property(AWS_QDMI_Site site, QDMI_Site_Property prop) const {
    uint64_t value = 0;
    EXPECT_EQ(AWS_QDMI_device_session_query_site_property(
        session, site, prop, sizeof(uint64_t), &value, nullptr), QDMI_SUCCESS);
    return value;
  }

  // Helper to get operation string property
  std::string getOperationStringProperty(AWS_QDMI_Operation op, QDMI_Operation_Property prop) const {
    size_t size = 0;
    EXPECT_EQ(AWS_QDMI_device_session_query_operation_property(
        session, op, 0, nullptr, 0, nullptr, prop, 0, nullptr, &size), QDMI_SUCCESS);
    if (size == 0) return "";
    
    std::string result(size - 1, '\0');
    EXPECT_EQ(AWS_QDMI_device_session_query_operation_property(
        session, op, 0, nullptr, 0, nullptr, prop, size, result.data(), nullptr), QDMI_SUCCESS);
    return result;
  }

  // Helper to get operation size_t property
  size_t getOperationSizeTProperty(AWS_QDMI_Operation op, QDMI_Operation_Property prop) const {
    size_t value = 0;
    EXPECT_EQ(AWS_QDMI_device_session_query_operation_property(
        session, op, 0, nullptr, 0, nullptr, prop, sizeof(size_t), &value, nullptr), QDMI_SUCCESS);
    return value;
  }

  // Helper to get operation double property
  double getOperationDoubleProperty(AWS_QDMI_Operation op, QDMI_Operation_Property prop) const {
    double value = 0.0;
    EXPECT_EQ(AWS_QDMI_device_session_query_operation_property(
        session, op, 0, nullptr, 0, nullptr, prop, sizeof(double), &value, nullptr), QDMI_SUCCESS);
    return value;
  }
};

TEST_F(AWSQDMIIntegrationTest, QueryDeviceProperties) {
  const auto device_name = getStringProperty(QDMI_DEVICE_PROPERTY_NAME);
  ASSERT_FALSE(device_name.empty()) << "Device must provide a name";

  const auto version = getStringProperty(QDMI_DEVICE_PROPERTY_VERSION);
  ASSERT_FALSE(version.empty()) << "Device must provide a version";

  const auto library_version = getStringProperty(QDMI_DEVICE_PROPERTY_LIBRARYVERSION);
  ASSERT_FALSE(library_version.empty()) << "Device must provide a library version";

  QDMI_Device_Status status;
  EXPECT_EQ(AWS_QDMI_device_session_query_device_property(
      session, QDMI_DEVICE_PROPERTY_STATUS, sizeof(status), &status, nullptr), QDMI_SUCCESS);
  EXPECT_TRUE(status == QDMI_DEVICE_STATUS_IDLE || status == QDMI_DEVICE_STATUS_BUSY);

  const auto qubits_num = getSizeTProperty(QDMI_DEVICE_PROPERTY_QUBITSNUM);
  ASSERT_GT(qubits_num, 0) << "Device must have at least one qubit";

  // AWS-specific properties
  const auto provider = getStringProperty(static_cast<QDMI_Device_Property>(QDMI_DEVICE_PROPERTY_PROVIDER));
  EXPECT_FALSE(provider.empty());

  // Duration unit and scale factor
  const auto duration_unit = getStringProperty(QDMI_DEVICE_PROPERTY_DURATIONUNIT);
  ASSERT_FALSE(duration_unit.empty());

  double duration_scale_factor = 0.0;
  EXPECT_EQ(AWS_QDMI_device_session_query_device_property(
      session, QDMI_DEVICE_PROPERTY_DURATIONSCALEFACTOR, 
      sizeof(double), &duration_scale_factor, nullptr), QDMI_SUCCESS);
  ASSERT_GT(duration_scale_factor, 0.0);

  // Sites
  auto sites = getListProperty<AWS_QDMI_Site>(QDMI_DEVICE_PROPERTY_SITES);
  EXPECT_EQ(sites.size(), qubits_num);

  // Operations
  auto operations = getListProperty<AWS_QDMI_Operation>(QDMI_DEVICE_PROPERTY_OPERATIONS);
  ASSERT_GT(operations.size(), 0) << "Device must support at least one operation";

  // Coupling map
  auto coupling_map = getListProperty<AWS_QDMI_Site>(QDMI_DEVICE_PROPERTY_COUPLINGMAP);
  if (qubits_num == 1) {
    EXPECT_TRUE(coupling_map.empty());
  } else {
    EXPECT_GT(coupling_map.size(), 0);
    EXPECT_EQ(coupling_map.size() % 2, 0) << "Coupling map should have pairs of sites";
  }

  // The MAX property is not valid
  EXPECT_EQ(AWS_QDMI_device_session_query_device_property(
      session, QDMI_DEVICE_PROPERTY_MAX, 0, nullptr, nullptr),
      QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AWSQDMIIntegrationTest, QuerySiteProperties) {
  const auto qubits_num = getSizeTProperty(QDMI_DEVICE_PROPERTY_QUBITSNUM);
  auto sites = getListProperty<AWS_QDMI_Site>(QDMI_DEVICE_PROPERTY_SITES);
  
  EXPECT_EQ(sites.size(), qubits_num);

  for (const auto& site : sites) {
    const auto site_id = getSiteUInt64Property(site, QDMI_SITE_PROPERTY_INDEX);
    EXPECT_LT(site_id, qubits_num);

    const auto site_name = getSiteStringProperty(site, QDMI_SITE_PROPERTY_NAME);
    EXPECT_FALSE(site_name.empty());

    // T1 and T2 are optional
    uint64_t t1 = 0;
    auto ret = AWS_QDMI_device_session_query_site_property(
        session, site, QDMI_SITE_PROPERTY_T1, sizeof(uint64_t), &t1, nullptr);
    if (ret == QDMI_SUCCESS) {
      EXPECT_GT(t1, 0) << "T1 should be positive when available";
    } else {
      EXPECT_EQ(ret, QDMI_ERROR_NOTSUPPORTED);
    }

    uint64_t t2 = 0;
    ret = AWS_QDMI_device_session_query_site_property(
        session, site, QDMI_SITE_PROPERTY_T2, sizeof(uint64_t), &t2, nullptr);
    if (ret == QDMI_SUCCESS) {
      EXPECT_GT(t2, 0) << "T2 should be positive when available";
    } else {
      EXPECT_EQ(ret, QDMI_ERROR_NOTSUPPORTED);
    }

    // MAX property is invalid
    EXPECT_EQ(AWS_QDMI_device_session_query_site_property(
        session, site, QDMI_SITE_PROPERTY_MAX, 0, nullptr, nullptr),
        QDMI_ERROR_INVALIDARGUMENT);
  }
}

TEST_F(AWSQDMIIntegrationTest, QueryOperationProperties) {
  auto operations = getListProperty<AWS_QDMI_Operation>(QDMI_DEVICE_PROPERTY_OPERATIONS);
  ASSERT_GT(operations.size(), 0);

  for (const auto& op : operations) {
    const auto op_name = getOperationStringProperty(op, QDMI_OPERATION_PROPERTY_NAME);
    EXPECT_FALSE(op_name.empty());

    const auto num_qubits = getOperationSizeTProperty(op, QDMI_OPERATION_PROPERTY_QUBITSNUM);
    EXPECT_GT(num_qubits, 0);
    EXPECT_LE(num_qubits, 2); // Most gates are 1 or 2 qubits

    const auto num_params = getOperationSizeTProperty(op, QDMI_OPERATION_PROPERTY_PARAMETERSNUM);
    // num_params can be 0 (e.g., for fixed gates like CZ)

    // Fidelity is optional
    double fidelity = 0.0;
    auto ret = AWS_QDMI_device_session_query_operation_property(
        session, op, 0, nullptr, 0, nullptr, 
        QDMI_OPERATION_PROPERTY_FIDELITY, sizeof(double), &fidelity, nullptr);
    if (ret == QDMI_SUCCESS) {
      EXPECT_GE(fidelity, 0.0);
      EXPECT_LE(fidelity, 1.0);
    } else {
      EXPECT_EQ(ret, QDMI_ERROR_NOTSUPPORTED);
    }

    // MAX property is invalid
    EXPECT_EQ(AWS_QDMI_device_session_query_operation_property(
        session, op, 0, nullptr, 0, nullptr, 
        QDMI_OPERATION_PROPERTY_MAX, 0, nullptr, nullptr),
        QDMI_ERROR_INVALIDARGUMENT);
  }
}

TEST_F(AWSQDMIIntegrationTest, JobCycleQASM3) {
  AWS_QDMI_Device_Job job;
  ASSERT_EQ(AWS_QDMI_device_session_create_device_job(session, &job), QDMI_SUCCESS);

  // Set job parameters
  size_t shots = 100;
  EXPECT_EQ(AWS_QDMI_device_job_set_parameter(
      job, QDMI_DEVICE_JOB_PARAMETER_SHOTSNUM, sizeof(size_t), &shots), QDMI_SUCCESS);

  QDMI_Program_Format format = QDMI_PROGRAM_FORMAT_QASM3;
  EXPECT_EQ(AWS_QDMI_device_job_set_parameter(
      job, QDMI_DEVICE_JOB_PARAMETER_PROGRAMFORMAT, 
      sizeof(QDMI_Program_Format), &format), QDMI_SUCCESS);

  EXPECT_EQ(AWS_QDMI_device_job_set_parameter(
      job, QDMI_DEVICE_JOB_PARAMETER_PROGRAM, 
      strlen(TEST_CIRCUIT_QASM3) + 1, TEST_CIRCUIT_QASM3), QDMI_SUCCESS);

  // Query job properties before submission
  char job_id[256];
  size_t job_id_size;
  EXPECT_EQ(AWS_QDMI_device_job_query_property(
      job, QDMI_DEVICE_JOB_PROPERTY_ID, sizeof(job_id), job_id, &job_id_size), QDMI_SUCCESS);

  // Submit job
  EXPECT_EQ(AWS_QDMI_device_job_submit(job), QDMI_SUCCESS);

  // Wait for completion
  EXPECT_EQ(AWS_QDMI_device_job_wait(job, 60000), QDMI_SUCCESS); // 60 second timeout

  // Check status
  QDMI_Job_Status status;
  EXPECT_EQ(AWS_QDMI_device_job_check(job, &status), QDMI_SUCCESS);
  EXPECT_TRUE(status == QDMI_JOB_STATUS_DONE || status == QDMI_JOB_STATUS_FAILED);

  if (status == QDMI_JOB_STATUS_DONE) {
    // Get histogram results
    size_t keys_size = 0;
    EXPECT_EQ(AWS_QDMI_device_job_get_results(
        job, QDMI_JOB_RESULT_HIST_KEYS, 0, nullptr, &keys_size), QDMI_SUCCESS);
    
    if (keys_size > 0) {
      std::vector<char> keys(keys_size);
      EXPECT_EQ(AWS_QDMI_device_job_get_results(
          job, QDMI_JOB_RESULT_HIST_KEYS, keys_size, keys.data(), nullptr), QDMI_SUCCESS);

      size_t values_size = 0;
      EXPECT_EQ(AWS_QDMI_device_job_get_results(
          job, QDMI_JOB_RESULT_HIST_VALUES, 0, nullptr, &values_size), QDMI_SUCCESS);
      
      std::vector<size_t> values(values_size / sizeof(size_t));
      EXPECT_EQ(AWS_QDMI_device_job_get_results(
          job, QDMI_JOB_RESULT_HIST_VALUES, values_size, values.data(), nullptr), QDMI_SUCCESS);

      // Verify total counts
      size_t total = 0;
      for (const auto& count : values) {
        total += count;
      }
      EXPECT_EQ(total, shots);
    }
  }

  AWS_QDMI_device_job_free(job);
}

TEST_F(AWSQDMIIntegrationTest, JobCycleQASM2) {
  AWS_QDMI_Device_Job job;
  ASSERT_EQ(AWS_QDMI_device_session_create_device_job(session, &job), QDMI_SUCCESS);

  size_t shots = 50;
  EXPECT_EQ(AWS_QDMI_device_job_set_parameter(
      job, QDMI_DEVICE_JOB_PARAMETER_SHOTSNUM, sizeof(size_t), &shots), QDMI_SUCCESS);

  QDMI_Program_Format format = QDMI_PROGRAM_FORMAT_QASM2;
  EXPECT_EQ(AWS_QDMI_device_job_set_parameter(
      job, QDMI_DEVICE_JOB_PARAMETER_PROGRAMFORMAT, 
      sizeof(QDMI_Program_Format), &format), QDMI_SUCCESS);

  EXPECT_EQ(AWS_QDMI_device_job_set_parameter(
      job, QDMI_DEVICE_JOB_PARAMETER_PROGRAM, 
      strlen(TEST_CIRCUIT_QASM2) + 1, TEST_CIRCUIT_QASM2), QDMI_SUCCESS);

  EXPECT_EQ(AWS_QDMI_device_job_submit(job), QDMI_SUCCESS);
  EXPECT_EQ(AWS_QDMI_device_job_wait(job, 60000), QDMI_SUCCESS);

  QDMI_Job_Status status;
  EXPECT_EQ(AWS_QDMI_device_job_check(job, &status), QDMI_SUCCESS);
  EXPECT_TRUE(status == QDMI_JOB_STATUS_DONE || status == QDMI_JOB_STATUS_FAILED);

  AWS_QDMI_device_job_free(job);
}

TEST_F(AWSQDMIIntegrationTest, JobCancellation) {
  AWS_QDMI_Device_Job job;
  ASSERT_EQ(AWS_QDMI_device_session_create_device_job(session, &job), QDMI_SUCCESS);

  size_t shots = 100;
  EXPECT_EQ(AWS_QDMI_device_job_set_parameter(
      job, QDMI_DEVICE_JOB_PARAMETER_SHOTSNUM, sizeof(size_t), &shots), QDMI_SUCCESS);

  QDMI_Program_Format format = QDMI_PROGRAM_FORMAT_QASM3;
  EXPECT_EQ(AWS_QDMI_device_job_set_parameter(
      job, QDMI_DEVICE_JOB_PARAMETER_PROGRAMFORMAT, 
      sizeof(QDMI_Program_Format), &format), QDMI_SUCCESS);

  EXPECT_EQ(AWS_QDMI_device_job_set_parameter(
      job, QDMI_DEVICE_JOB_PARAMETER_PROGRAM, 
      strlen(TEST_CIRCUIT_QASM3) + 1, TEST_CIRCUIT_QASM3), QDMI_SUCCESS);

  EXPECT_EQ(AWS_QDMI_device_job_submit(job), QDMI_SUCCESS);
  EXPECT_EQ(AWS_QDMI_device_job_cancel(job), QDMI_SUCCESS);

  QDMI_Job_Status status;
  EXPECT_EQ(AWS_QDMI_device_job_check(job, &status), QDMI_SUCCESS);
  EXPECT_EQ(status, QDMI_JOB_STATUS_CANCELED);

  AWS_QDMI_device_job_free(job);
}

TEST_F(AWSQDMIIntegrationTest, JobCornerCases) {
  AWS_QDMI_Device_Job job;
  
  // Null checks
  EXPECT_EQ(AWS_QDMI_device_session_create_device_job(nullptr, &job),
            QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_EQ(AWS_QDMI_device_session_create_device_job(session, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);

  ASSERT_EQ(AWS_QDMI_device_session_create_device_job(session, &job), QDMI_SUCCESS);

  // Invalid program format - should return NOTSUPPORTED since we only support OpenQASM
  QDMI_Program_Format invalid_format = QDMI_PROGRAM_FORMAT_MAX;
  EXPECT_EQ(AWS_QDMI_device_job_set_parameter(
      job, QDMI_DEVICE_JOB_PARAMETER_PROGRAMFORMAT, 
      sizeof(QDMI_Program_Format), &invalid_format),
      QDMI_ERROR_NOTSUPPORTED);

  // Invalid parameter
  EXPECT_EQ(AWS_QDMI_device_job_set_parameter(
      job, QDMI_DEVICE_JOB_PARAMETER_MAX, 0, nullptr),
      QDMI_ERROR_INVALIDARGUMENT);

  // Null checks on operations
  EXPECT_EQ(AWS_QDMI_device_job_submit(nullptr), QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_EQ(AWS_QDMI_device_job_cancel(nullptr), QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_EQ(AWS_QDMI_device_job_wait(nullptr, 0), QDMI_ERROR_INVALIDARGUMENT);
  
  QDMI_Job_Status status;
  EXPECT_EQ(AWS_QDMI_device_job_check(nullptr, &status), QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_EQ(AWS_QDMI_device_job_check(job, nullptr), QDMI_ERROR_INVALIDARGUMENT);

  AWS_QDMI_device_job_free(job);
}

TEST_F(AWSQDMIIntegrationTest, ResultTypesCornerCases) {
  AWS_QDMI_Device_Job job;
  ASSERT_EQ(AWS_QDMI_device_session_create_device_job(session, &job), QDMI_SUCCESS);

  size_t shots = 10;
  EXPECT_EQ(AWS_QDMI_device_job_set_parameter(
      job, QDMI_DEVICE_JOB_PARAMETER_SHOTSNUM, sizeof(size_t), &shots), QDMI_SUCCESS);

  QDMI_Program_Format format = QDMI_PROGRAM_FORMAT_QASM3;
  EXPECT_EQ(AWS_QDMI_device_job_set_parameter(
      job, QDMI_DEVICE_JOB_PARAMETER_PROGRAMFORMAT, 
      sizeof(QDMI_Program_Format), &format), QDMI_SUCCESS);

  EXPECT_EQ(AWS_QDMI_device_job_set_parameter(
      job, QDMI_DEVICE_JOB_PARAMETER_PROGRAM, 
      strlen(TEST_CIRCUIT_QASM3) + 1, TEST_CIRCUIT_QASM3), QDMI_SUCCESS);

  EXPECT_EQ(AWS_QDMI_device_job_submit(job), QDMI_SUCCESS);
  EXPECT_EQ(AWS_QDMI_device_job_wait(job, 60000), QDMI_SUCCESS);

  // MAX result type is invalid
  EXPECT_EQ(AWS_QDMI_device_job_get_results(
      job, QDMI_JOB_RESULT_MAX, 0, nullptr, nullptr),
      QDMI_ERROR_INVALIDARGUMENT);

  // Buffer too small
  size_t size = 0;
  EXPECT_EQ(AWS_QDMI_device_job_get_results(
      job, QDMI_JOB_RESULT_HIST_KEYS, 0, nullptr, &size), QDMI_SUCCESS);
  if (size > 1) {
    std::vector<char> small_buffer(size - 1);
    EXPECT_EQ(AWS_QDMI_device_job_get_results(
        job, QDMI_JOB_RESULT_HIST_KEYS, small_buffer.size(), 
        small_buffer.data(), nullptr),
        QDMI_ERROR_INVALIDARGUMENT);
  }

  AWS_QDMI_device_job_free(job);
}

TEST_F(AWSQDMIIntegrationTest, SessionParameterCornerCases) {
  AWS_QDMI_Device_Session test_session;
  ASSERT_EQ(AWS_QDMI_device_session_alloc(&test_session), QDMI_SUCCESS);

  // Set parameters before init should succeed
  const std::string region = "us-west-2";
  EXPECT_EQ(AWS_QDMI_device_session_set_parameter(
      test_session, static_cast<QDMI_Device_Session_Parameter>(QDMI_DEVICE_SESSION_PARAMETER_REGION),
      region.length() + 1, region.c_str()), QDMI_SUCCESS);

  // Invalid parameter
  EXPECT_EQ(AWS_QDMI_device_session_set_parameter(
      test_session, static_cast<QDMI_Device_Session_Parameter>(QDMI_DEVICE_SESSION_PARAMETER_MAX), 0, nullptr),
      QDMI_ERROR_INVALIDARGUMENT);

  // Null checks
  EXPECT_EQ(AWS_QDMI_device_session_set_parameter(
      nullptr, static_cast<QDMI_Device_Session_Parameter>(QDMI_DEVICE_SESSION_PARAMETER_REGION), 0, nullptr),
      QDMI_ERROR_INVALIDARGUMENT);

  AWS_QDMI_device_session_free(test_session);
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
