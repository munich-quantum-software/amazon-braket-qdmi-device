#include "amazon-braket-qdmi-device/qdmi/device.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <utility>
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
    const std::string deviceArn =
        (deviceArnEnv != nullptr)
            ? deviceArnEnv
            : "arn:aws:braket:::device/quantum-simulator/amazon/sv1";

    AMAZON_BRAKET_QDMI_device_session_set_parameter(
        session,
        static_cast<QDMI_Device_Session_Parameter>(
            QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN),
        deviceArn.length() + 1, deviceArn.c_str());

    const char* s3BucketEnv = std::getenv("AWS_S3_BUCKET");
    if (s3BucketEnv != nullptr) {
      AMAZON_BRAKET_QDMI_device_session_set_parameter(
          session,
          static_cast<QDMI_Device_Session_Parameter>(
              QDMI_DEVICE_SESSION_PARAMETER_S3BUCKET),
          strlen(s3BucketEnv) + 1, s3BucketEnv);
    }

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
  static AMAZON_BRAKET_QDMI_Device_Session shared_session;
  static AMAZON_BRAKET_QDMI_Device_Job shared_job;

  // Collected data
  static bool submitted_ok;
  static int wait_result;
  static bool has_task_arn;
  static std::string task_arn;
  static bool has_shots;
  static std::string shots_data;
  static bool has_hist;
  static std::vector<std::string> hist_keys;
  static std::vector<size_t> hist_values;

  AMAZON_BRAKET_QDMI_Device_Job job = nullptr;

  static void SetUpTestSuite() {
    // Create a session for the shared job
    submitted_ok = false;
    wait_result = QDMI_ERROR_NOTSUPPORTED;
    has_task_arn = false;
    task_arn.clear();
    has_shots = false;
    shots_data.clear();
    has_hist = false;
    hist_keys.clear();
    hist_values.clear();

    if (AMAZON_BRAKET_QDMI_device_initialize() != QDMI_SUCCESS) {
      GTEST_FAIL()
          << "AMAZON_BRAKET_QDMI_device_initialize failed in SetUpTestSuite";
      return;
    }

    if (AMAZON_BRAKET_QDMI_device_session_alloc(&shared_session) !=
        QDMI_SUCCESS) {
      GTEST_FAIL() << "session_alloc failed in SetUpTestSuite";
      return;
    }

    const char* deviceArnEnv = std::getenv("AWS_DEVICE_ARN");
    const std::string deviceArn =
        (deviceArnEnv != nullptr)
            ? deviceArnEnv
            : "arn:aws:braket:::device/quantum-simulator/amazon/sv1";
    AMAZON_BRAKET_QDMI_device_session_set_parameter(
        shared_session,
        static_cast<QDMI_Device_Session_Parameter>(
            QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN),
        deviceArn.length() + 1, deviceArn.c_str());

    const char* s3BucketEnv = std::getenv("AWS_S3_BUCKET");
    if (s3BucketEnv != nullptr) {
      AMAZON_BRAKET_QDMI_device_session_set_parameter(
          shared_session,
          static_cast<QDMI_Device_Session_Parameter>(
              QDMI_DEVICE_SESSION_PARAMETER_S3BUCKET),
          strlen(s3BucketEnv) + 1, s3BucketEnv);
    }

    const char* regionEnv = std::getenv("AWS_DEFAULT_REGION");
    if (regionEnv != nullptr) {
      AMAZON_BRAKET_QDMI_device_session_set_parameter(
          shared_session,
          static_cast<QDMI_Device_Session_Parameter>(
              QDMI_DEVICE_SESSION_PARAMETER_REGION),
          strlen(regionEnv) + 1, regionEnv);
    }

    if (AMAZON_BRAKET_QDMI_device_session_init(shared_session) !=
        QDMI_SUCCESS) {
      GTEST_SKIP() << "session_init failed in SetUpTestSuite; skipping job "
                      "submission tests";
      return;
    }

    // create job
    if (AMAZON_BRAKET_QDMI_device_session_create_device_job(
            shared_session, &shared_job) != QDMI_SUCCESS) {
      GTEST_SKIP() << "create_device_job failed in SetUpTestSuite; skipping "
                      "job submission tests";
      return;
    }

    // set program/format/shots
    const char* program = "OPENQASM 3.0;\nqubit[2] q;\nh q[0];\ncnot q[0], "
                          "q[1];\nbit[2] c;\nc = measure q;\n";
    AMAZON_BRAKET_QDMI_device_job_set_parameter(
        shared_job, QDMI_DEVICE_JOB_PARAMETER_PROGRAM, strlen(program) + 1,
        program);
    QDMI_Program_Format format = QDMI_PROGRAM_FORMAT_QASM3;
    AMAZON_BRAKET_QDMI_device_job_set_parameter(
        shared_job, QDMI_DEVICE_JOB_PARAMETER_PROGRAMFORMAT, sizeof(format),
        &format);
    size_t shots = 100;
    AMAZON_BRAKET_QDMI_device_job_set_parameter(
        shared_job, QDMI_DEVICE_JOB_PARAMETER_SHOTSNUM, sizeof(shots), &shots);

    // submit
    const auto submit_status = AMAZON_BRAKET_QDMI_device_job_submit(shared_job);
    if (submit_status != QDMI_SUCCESS) {
      // allow not-supported
      if (submit_status == QDMI_ERROR_NOTSUPPORTED) {
        GTEST_SKIP() << "job_submit not supported; skipping job result tests";
        return;
      }
      GTEST_FAIL() << "job_submit failed with status " << submit_status;
      return;
    }
    submitted_ok = true;

    // get task arn if available
    size_t arnSize = 0;
    if (AMAZON_BRAKET_QDMI_device_job_query_property(
            shared_job,
            static_cast<QDMI_Device_Job_Property>(
                QDMI_DEVICE_JOB_PROPERTY_TASKARN),
            0, nullptr, &arnSize) == QDMI_SUCCESS &&
        arnSize > 0) {
      std::string arn(arnSize - 1, '\0');
      if (AMAZON_BRAKET_QDMI_device_job_query_property(
              shared_job,
              static_cast<QDMI_Device_Job_Property>(
                  QDMI_DEVICE_JOB_PROPERTY_TASKARN),
              arnSize, arn.data(), nullptr) == QDMI_SUCCESS) {
        has_task_arn = true;
        task_arn = arn;
      }
    }

    // wait for completion (timeout: 120 seconds)
    wait_result = AMAZON_BRAKET_QDMI_device_job_wait(shared_job, 120000);

    if (wait_result == QDMI_SUCCESS) {
      // check final status
      QDMI_Job_Status finalStatus = QDMI_JOB_STATUS_CREATED;
      if (AMAZON_BRAKET_QDMI_device_job_check(shared_job, &finalStatus) ==
              QDMI_SUCCESS &&
          finalStatus == QDMI_JOB_STATUS_DONE) {
        // fetch SHOTS
        size_t shotsSize = 0;
        if (AMAZON_BRAKET_QDMI_device_job_get_results(
                shared_job, QDMI_JOB_RESULT_SHOTS, 0, nullptr, &shotsSize) ==
                QDMI_SUCCESS &&
            shotsSize > 0) {
          std::string shotsStr(shotsSize - 1, '\0');
          if (AMAZON_BRAKET_QDMI_device_job_get_results(
                  shared_job, QDMI_JOB_RESULT_SHOTS, shotsSize, shotsStr.data(),
                  nullptr) == QDMI_SUCCESS) {
            has_shots = true;
            shots_data = shotsStr;
          }
        }
        // fetch histogram keys
        size_t keysSize = 0;
        if (AMAZON_BRAKET_QDMI_device_job_get_results(
                shared_job, QDMI_JOB_RESULT_HIST_KEYS, 0, nullptr, &keysSize) ==
                QDMI_SUCCESS &&
            keysSize > 0) {
          std::vector<char> keysData(keysSize);
          if (AMAZON_BRAKET_QDMI_device_job_get_results(
                  shared_job, QDMI_JOB_RESULT_HIST_KEYS, keysSize,
                  keysData.data(), nullptr) == QDMI_SUCCESS) {
            // parse null-separated keys
            const char* ptr = keysData.data();
            const char* end = ptr + keysSize;
            while (ptr < end && *ptr != '\0') {
              hist_keys.emplace_back(ptr);
              ptr += strlen(ptr) + 1;
            }
          }
        }
        // fetch histogram values
        size_t valuesSize = 0;
        if (AMAZON_BRAKET_QDMI_device_job_get_results(
                shared_job, QDMI_JOB_RESULT_HIST_VALUES, 0, nullptr,
                &valuesSize) == QDMI_SUCCESS &&
            valuesSize > 0) {
          if (valuesSize % sizeof(size_t) == 0) {
            const size_t n = valuesSize / sizeof(size_t);
            hist_values.resize(n);
            if (AMAZON_BRAKET_QDMI_device_job_get_results(
                    shared_job, QDMI_JOB_RESULT_HIST_VALUES, valuesSize,
                    hist_values.data(), nullptr) == QDMI_SUCCESS) {
              has_hist = !hist_keys.empty() && !hist_values.empty();
            }
          }
        }
      }
    }
  }

  static void TearDownTestSuite() {
    if (shared_job != nullptr) {
      AMAZON_BRAKET_QDMI_device_job_free(shared_job);
      shared_job = nullptr;
    }
    if (shared_session != nullptr) {
      AMAZON_BRAKET_QDMI_device_session_free(shared_session);
      shared_session = nullptr;
    }
    AMAZON_BRAKET_QDMI_device_finalize();
  }

  void SetUp() override {
    // use shared job handle
    job = shared_job;
  }

  void TearDown() override {
    job = nullptr; // do not free shared job here
  }
};

// Static member definitions
AMAZON_BRAKET_QDMI_Device_Session
    AmazonBraketQDMIJobSpecificationTest::shared_session = nullptr;
AMAZON_BRAKET_QDMI_Device_Job AmazonBraketQDMIJobSpecificationTest::shared_job =
    nullptr;
bool AmazonBraketQDMIJobSpecificationTest::submitted_ok = false;
int AmazonBraketQDMIJobSpecificationTest::wait_result = QDMI_ERROR_NOTSUPPORTED;
bool AmazonBraketQDMIJobSpecificationTest::has_task_arn = false;
std::string AmazonBraketQDMIJobSpecificationTest::task_arn;
bool AmazonBraketQDMIJobSpecificationTest::has_shots = false;
std::string AmazonBraketQDMIJobSpecificationTest::shots_data;
bool AmazonBraketQDMIJobSpecificationTest::has_hist = false;
std::vector<std::string> AmazonBraketQDMIJobSpecificationTest::hist_keys;
std::vector<size_t> AmazonBraketQDMIJobSpecificationTest::hist_values;

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
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_create_device_job(shared_session,
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
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_create_device_job(shared_session,
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
                job, QDMI_DEVICE_JOB_PROPERTY_ID, 0, nullptr, nullptr),
            QDMI_SUCCESS);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_query_property(
                job, QDMI_DEVICE_JOB_PROPERTY_MAX, 0, nullptr, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AmazonBraketQDMIJobSpecificationTest, QueryJobId) {
  size_t size = 0;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_job_query_property(
                job, QDMI_DEVICE_JOB_PROPERTY_ID, 0, nullptr, &size),
            QDMI_SUCCESS);
  ASSERT_EQ(size, sizeof(int));
  int id = 0;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_query_property(
                job, QDMI_DEVICE_JOB_PROPERTY_ID, sizeof(id), &id, nullptr),
            QDMI_SUCCESS);
}

TEST_F(AmazonBraketQDMISpecificationTest, JobSubmit) {
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_submit(nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AmazonBraketQDMIJobSpecificationTest, JobSubmit) {
  if (!submitted_ok) {
    GTEST_SKIP() << "Shared job was not submitted in suite setup";
  }
  EXPECT_TRUE(submitted_ok);
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
  if (!submitted_ok) {
    GTEST_SKIP() << "Job was not submitted in suite setup";
  }
  // Check the stored wait result
  EXPECT_EQ(wait_result, QDMI_SUCCESS);
}

TEST_F(AmazonBraketQDMISpecificationTest, JobGetResults) {
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_get_results(
                nullptr, QDMI_JOB_RESULT_MAX, 0, nullptr, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AmazonBraketQDMIJobSpecificationTest, JobGetResults) {
  if (!submitted_ok) {
    GTEST_SKIP() << "Job was not submitted in suite setup";
  }
  if (!has_shots) {
    GTEST_SKIP() << "SHOTS results not available for this device/job";
  }
  // Basic checks on fetched shots data
  EXPECT_GT(shots_data.size(), 0u);
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
  if (!submitted_ok) {
    GTEST_SKIP() << "Job was not submitted in suite setup";
  }
  if (!has_hist) {
    GTEST_SKIP() << "Histogram not available for this job";
  }
  EXPECT_GT(hist_keys.size(), 0u);

  // For a Bell state, we expect 00 and 11
  bool found00 = false;
  bool found11 = false;
  for (const auto& key : hist_keys) {
    if (key == "00")
      found00 = true;
    if (key == "11")
      found11 = true;
  }
  EXPECT_TRUE(found00 && found11)
      << "Did not find expected histogram keys '00' and '11'";
}

TEST_F(AmazonBraketQDMIJobSpecificationTest, JobGetResultsHistValues) {
  if (!submitted_ok) {
    GTEST_SKIP() << "Job was not submitted in suite setup";
  }
  if (!has_hist) {
    GTEST_SKIP() << "Histogram not available for this job";
  }
  EXPECT_EQ(hist_values.size(), hist_keys.size());
  EXPECT_GT(hist_values.size(), 0u);

  size_t totalShots = 0;
  for (auto count : hist_values) {
    totalShots += count;
  }
  EXPECT_EQ(totalShots, 100u);
}

TEST_F(AmazonBraketQDMIJobSpecificationTest, JobTaskArn) {
  if (!submitted_ok) {
    GTEST_SKIP() << "Job was not submitted in suite setup";
  }
  if (!has_task_arn) {
    GTEST_SKIP() << "Task ARN not available for this device/job";
  }
  EXPECT_FALSE(task_arn.empty());
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

TEST_F(AmazonBraketQDMISpecificationTest, AwsSpecificDeviceProperties) {
  size_t size = 0;
  // Provider
  const auto providerStatus =
      AMAZON_BRAKET_QDMI_device_session_query_device_property(
          session,
          static_cast<QDMI_Device_Property>(QDMI_DEVICE_PROPERTY_PROVIDER), 0,
          nullptr, &size);
  ASSERT_EQ(providerStatus, QDMI_SUCCESS);
  if (size > 0) {
    std::string provider(size - 1, '\0');
    EXPECT_EQ(
        AMAZON_BRAKET_QDMI_device_session_query_device_property(
            session,
            static_cast<QDMI_Device_Property>(QDMI_DEVICE_PROPERTY_PROVIDER),
            size, provider.data(), nullptr),
        QDMI_SUCCESS);
    EXPECT_FALSE(provider.empty());
  }

  // Device ARN
  size = 0;
  const auto arnStatus =
      AMAZON_BRAKET_QDMI_device_session_query_device_property(
          session,
          static_cast<QDMI_Device_Property>(QDMI_DEVICE_PROPERTY_DEVICEARN), 0,
          nullptr, &size);
  ASSERT_EQ(arnStatus, QDMI_SUCCESS);
  if (size > 0) {
    std::string arn(size - 1, '\0');
    EXPECT_EQ(
        AMAZON_BRAKET_QDMI_device_session_query_device_property(
            session,
            static_cast<QDMI_Device_Property>(QDMI_DEVICE_PROPERTY_DEVICEARN),
            size, arn.data(), nullptr),
        QDMI_SUCCESS);
    EXPECT_FALSE(arn.empty());
  }

  // Device type
  size = 0;
  const auto typeStatus =
      AMAZON_BRAKET_QDMI_device_session_query_device_property(
          session,
          static_cast<QDMI_Device_Property>(QDMI_DEVICE_PROPERTY_DEVICETYPE), 0,
          nullptr, &size);
  ASSERT_EQ(typeStatus, QDMI_SUCCESS);
  if (size > 0) {
    std::string dtype(size - 1, '\0');
    EXPECT_EQ(
        AMAZON_BRAKET_QDMI_device_session_query_device_property(
            session,
            static_cast<QDMI_Device_Property>(QDMI_DEVICE_PROPERTY_DEVICETYPE),
            size, dtype.data(), nullptr),
        QDMI_SUCCESS);
    EXPECT_FALSE(dtype.empty());
  }
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
    EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_query_operation_property(
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
    EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_query_site_property(
                  session, site, QDMI_SITE_PROPERTY_T1, sizeof(uint64_t), &t1,
                  nullptr),
              QDMI_SUCCESS);

    uint64_t t2 = 0;
    EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_query_site_property(
                  session, site, QDMI_SITE_PROPERTY_T2, sizeof(uint64_t), &t2,
                  nullptr),
              QDMI_SUCCESS);
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
