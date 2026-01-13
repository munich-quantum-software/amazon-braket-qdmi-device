
/* ============================================================================
 * ============================================================================
 * Internal Implementation Header - C++ Classes
 * ============================================================================
 *
 * This header defines the internal C++ implementation structures that back
 * the C API defined in device.h. These should not be used directly by
 * client code - use the C API functions instead.
 *
 * ARCHITECTURE:
 *
 * Device (Singleton)
 *   ├─ Manages global device state
 *   ├─ Tracks all active sessions
 *   └─ Provides unique job IDs
 *
 * Device_Session
 *   ├─ Represents connection to AWS Braket
 *   ├─ Stores device topology (qubits, gates, connectivity)
 *   ├─ Creates and manages jobs
 *   └─ Wraps AWS Braket Client
 *
 * Device_Job
 *   ├─ Represents a quantum task/circuit
 *   ├─ Handles async execution
 *   ├─ Stores results (measurement outcomes)
 *   └─ Maps to AWS Braket Quantum Task
 *
 * Site
 *   └─ Represents a physical qubit with coherence times
 *
 * Operation
 *   └─ Represents a quantum gate with parameters and fidelity
 */

#pragma once

#include "device.h"

#include <atomic>
#include <aws/braket/BraketClient.h>
#include <cstddef>
#include <cstdint>
#include <future>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// Forward declarations
struct AWS_QDMI_Site_impl_d;
struct AWS_QDMI_Operation_impl_d;
struct AWS_QDMI_Device_Job_impl_d;

namespace aws_qdmi {

/**
 * @brief Main device singleton managing AWS Braket device access.
 */
class Device final {
  std::string name_;
  std::string provider_;
  std::string deviceArn_;
  std::string deviceType_;
  size_t qubitsNum_ = 0;
  std::atomic<QDMI_Device_Status> status_{QDMI_DEVICE_STATUS_OFFLINE};

  std::unordered_map<AWS_QDMI_Device_Session,
                     std::unique_ptr<AWS_QDMI_Device_Session_impl_d>>
      sessions_;
  mutable std::mutex sessionsMutex_;

  std::mt19937_64 rng_{std::random_device{}()};
  mutable std::mutex rngMutex_;
  std::uniform_int_distribution<> dis_{0, std::numeric_limits<int>::max()};

  std::atomic<size_t> runningJobs_{0};

  Device();

public:
  Device(Device&&) = delete;
  Device& operator=(Device&&) = delete;
  Device(const Device&) = delete;
  Device& operator=(const Device&) = delete;

  [[nodiscard]] static Device& get() {
    static Device instance;
    return instance;
  }

  ~Device() = default;

  auto sessionAlloc(AWS_QDMI_Device_Session* session) -> QDMI_STATUS;
  auto sessionFree(AWS_QDMI_Device_Session session) -> void;
  auto queryProperty(QDMI_Device_Property prop, size_t size, void* value,
                     size_t* sizeRet) const -> QDMI_STATUS;
  auto generateUniqueID() -> int;
  auto setStatus(QDMI_Device_Status status) -> void;
  auto increaseRunningJobs() -> void;
  auto decreaseRunningJobs() -> void;
};

} // namespace aws_qdmi

/**
 * @brief Device session implementation.
 */
struct AWS_QDMI_Device_Session_impl_d {
private:
  enum class Status : uint8_t {
    ALLOCATED,
    INITIALIZED,
  };
  Status status_ = Status::ALLOCATED;
  std::string region_;
  std::string deviceArn_;
  std::string s3Bucket_;
  std::string name_;
  std::string provider_;
  std::string deviceType_;

  // Device architecture data
  std::vector<std::unique_ptr<AWS_QDMI_Site_impl_d>> sites_;
  std::vector<AWS_QDMI_Site_impl_d*> sites_ptr_;
  std::unordered_map<std::string, AWS_QDMI_Site_impl_d*> sites_map_;

  std::vector<std::unique_ptr<AWS_QDMI_Operation_impl_d>> operations_;
  std::vector<AWS_QDMI_Operation_impl_d*> operations_ptr_;
  std::unordered_map<std::string, AWS_QDMI_Operation_impl_d*> operations_map_;

  std::vector<std::pair<AWS_QDMI_Site_impl_d*, AWS_QDMI_Site_impl_d*>>
      connectivity_;

  size_t qubitsNum_ = 0;

  std::unordered_map<AWS_QDMI_Device_Job,
                     std::unique_ptr<AWS_QDMI_Device_Job_impl_d>>
      jobs_;
  mutable std::mutex jobsMutex_;

  std::unique_ptr<Aws::Braket::BraketClient> client_;

public:
  auto init() -> QDMI_STATUS;
  auto setParameter(QDMI_Device_Session_Parameter param, size_t size,
                    const void* value) -> QDMI_STATUS;
  auto createDeviceJob(AWS_QDMI_Device_Job* job) -> QDMI_STATUS;
  auto freeDeviceJob(AWS_QDMI_Device_Job job) -> void;
  auto queryDeviceProperty(QDMI_Device_Property prop, size_t size, void* value,
                           size_t* sizeRet) const -> QDMI_STATUS;
  static auto querySiteProperty(AWS_QDMI_Site_impl_d* site,
                                QDMI_Site_Property prop, size_t size,
                                void* value, size_t* sizeRet) -> QDMI_STATUS;
  static auto queryOperationProperty(AWS_QDMI_Operation_impl_d* operation,
                                     size_t numSites,
                                     const AWS_QDMI_Site_impl_d* const* sites,
                                     size_t numParams, const double* params,
                                     QDMI_Operation_Property prop, size_t size,
                                     void* value, size_t* sizeRet)
      -> QDMI_STATUS;

private:
  auto fetchDeviceArchitecture() -> QDMI_STATUS;

  [[nodiscard]] auto getClient() const -> Aws::Braket::BraketClient* {
    return client_.get();
  }
  [[nodiscard]] auto getDeviceArn() const -> const std::string& {
    return deviceArn_;
  }
  [[nodiscard]] auto getS3Bucket() const -> const std::string& {
    return s3Bucket_;
  }
  [[nodiscard]] auto getRegion() const -> const std::string& { return region_; }

  // Allow Job to access session internals
  friend struct AWS_QDMI_Device_Job_impl_d;
};

/**
 * @brief Site implementation structure.
 */
struct AWS_QDMI_Site_impl_d {
  std::string name_;
  size_t id_ = 0;
  uint64_t t1_ = 0; // T1 coherence time in microseconds
  uint64_t t2_ = 0; // T2 coherence time in microseconds
};

/**
 * @brief Operation implementation structure.
 */
struct AWS_QDMI_Operation_impl_d {
  std::string name_;
  size_t numQubits_ = 0;
  size_t numParams_ = 0;
  double fidelity_ = 0.0;
  std::vector<std::vector<AWS_QDMI_Site_impl_d*>> applicable_sites_;
};

/**
 * @brief Device job implementation.
 */
struct AWS_QDMI_Device_Job_impl_d {
private:
  AWS_QDMI_Device_Session_impl_d* session_;
  int id_ = 0;
  mutable std::atomic<QDMI_Job_Status> status_{QDMI_JOB_STATUS_CREATED};

  QDMI_Program_Format format_ = QDMI_PROGRAM_FORMAT_QASM3;
  std::string program_;
  size_t shots_ = 100;
  std::string taskArn_;

  std::future<void> jobHandle_;
  mutable std::map<std::string, size_t> counts_;
  mutable std::string shotsString_; // Comma-separated shots: "00,11,00,..."
  mutable bool resultsFetched_ = false;
  mutable std::string outputS3Bucket_;
  mutable std::string outputS3Directory_;

  // Helper to fetch and parse results from S3
  auto fetchResults() const -> QDMI_STATUS;

public:
  explicit AWS_QDMI_Device_Job_impl_d(AWS_QDMI_Device_Session_impl_d* session)
      : session_(session), id_(aws_qdmi::Device::get().generateUniqueID()) {}

  auto free() -> void;
  auto setParameter(QDMI_Device_Job_Parameter param, size_t size,
                    const void* value) -> QDMI_STATUS;
  auto queryProperty(QDMI_Device_Job_Property prop, size_t size, void* value,
                     size_t* sizeRet) const -> QDMI_STATUS;
  auto submit() -> QDMI_STATUS;
  auto cancel() -> QDMI_STATUS;
  auto check(QDMI_Job_Status* status) const -> QDMI_STATUS;
  auto wait(size_t timeout) const -> QDMI_STATUS;
  auto getResults(QDMI_Job_Result result, size_t size, void* data,
                  size_t* sizeRet) const -> QDMI_STATUS;
};
