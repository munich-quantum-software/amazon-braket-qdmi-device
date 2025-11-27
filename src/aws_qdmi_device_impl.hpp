/*
 * Copyright (c) 2025 AWS QDMI Device Implementation
 * SPDX-License-Identifier: MIT
 * 
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

#include "aws_qdmi/device.h"

#include <atomic>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

// Forward declarations
struct AWS_QDMI_Site_impl_d;
struct AWS_QDMI_Operation_impl_d;

namespace Aws {
namespace Braket {
class BraketClient;
}
}

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
 * 
 * AWS Braket Mapping:
 * - Wraps Aws::Braket::BraketClient for API calls
 * - deviceArn_ → identifies the quantum device (e.g., "arn:aws:braket:us-east-1::device/quantum-simulator/amazon/sv1")
 * - region_ → AWS region for API calls (e.g., "us-east-1")
 * - s3Bucket_ → S3 bucket ARN for storing task results
 * - init() calls GetDevice API to fetch device capabilities
 * - Device capabilities JSON is parsed to populate sites_, operations_, connectivity_
 * 
 * AWS SDK Usage:
 * - GetDeviceRequest/Result: Fetch device metadata and capabilities
 * - Device capabilities format: JSON containing nativeGateSet, connectivity, timing
 * 
 * Example device ARNs:
 * - SV1 Simulator: arn:aws:braket:::device/quantum-simulator/amazon/sv1
 * - Rigetti: arn:aws:braket:us-west-1::device/qpu/rigetti/Aspen-M-3
 * - IonQ: arn:aws:braket:us-east-1::device/qpu/ionq/Aria-1
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

  // Device architecture data
  std::vector<std::unique_ptr<AWS_QDMI_Site_impl_d>> sites_;
  std::vector<AWS_QDMI_Site_impl_d*> sites_ptr_;
  std::unordered_map<std::string, AWS_QDMI_Site_impl_d*> sites_map_;
  
  std::vector<std::unique_ptr<AWS_QDMI_Operation_impl_d>> operations_;
  std::vector<AWS_QDMI_Operation_impl_d*> operations_ptr_;
  std::unordered_map<std::string, AWS_QDMI_Operation_impl_d*> operations_map_;
  
  std::vector<std::pair<AWS_QDMI_Site_impl_d*, AWS_QDMI_Site_impl_d*>> connectivity_;
  
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
  auto querySiteProperty(AWS_QDMI_Site_impl_d* site, QDMI_Site_Property prop,
                         size_t size, void* value, size_t* sizeRet) const -> QDMI_STATUS;
  auto queryOperationProperty(AWS_QDMI_Operation_impl_d* operation,
                              size_t num_sites, const AWS_QDMI_Site_impl_d* const* sites,
                              size_t num_params, const double* params,
                              QDMI_Operation_Property prop, size_t size,
                              void* value, size_t* sizeRet) const -> QDMI_STATUS;

private:
  auto fetchDeviceArchitecture() -> QDMI_STATUS;

public:
  [[nodiscard]] auto getClient() const -> Aws::Braket::BraketClient* {
    return client_.get();
  }
  [[nodiscard]] auto getDeviceArn() const -> const std::string& {
    return deviceArn_;
  }
  [[nodiscard]] auto getS3Bucket() const -> const std::string& {
    return s3Bucket_;
  }
};

/**
 * @brief Site implementation structure.
 * 
 * AWS Braket Mapping:
 * - Represents a physical qubit on the device
 * - Parsed from device capabilities JSON
 * - name_ → qubit identifier (e.g., "0", "1", "Q0", "Q1")
 * - id_ → numeric index for array access
 * - t1_ → T1 coherence time from device properties (microseconds)
 * - t2_ → T2 dephasing time from device properties (microseconds)
 * 
 * Example from Rigetti device capabilities:
 * {
 *   "qubits": {
 *     "0": {"T1": 25.0, "T2": 20.0},
 *     "1": {"T1": 28.0, "T2": 22.0}
 *   }
 * }
 */
struct AWS_QDMI_Site_impl_d {
  std::string name_;
  size_t id_ = 0;
  uint64_t t1_ = 0;  // T1 coherence time in microseconds
  uint64_t t2_ = 0;  // T2 coherence time in microseconds
};

/**
 * @brief Operation implementation structure.
 * 
 * AWS Braket Mapping:
 * - Represents a quantum gate operation supported by the device
 * - Parsed from device capabilities JSON "nativeGateSet" field
 * - name_ → gate identifier (e.g., "h", "rx", "cnot", "cz")
 * - num_qubits_ → gate arity (1 for single-qubit, 2 for two-qubit)
 * - num_params_ → number of rotation angles (0 for Hadamard, 1 for Rx/Ry/Rz)
 * - fidelity_ → gate fidelity from device calibration data
 * - applicable_sites_ → which qubit pairs can execute this gate (from connectivity)
 * 
 * Example from device capabilities:
 * {
 *   "nativeGateSet": ["h", "rx", "ry", "cnot", "cz"],
 *   "1Q": {"0": {"rx": {"fidelity": 0.999}, "h": {"fidelity": 0.998}}},
 *   "2Q": {"0-1": {"cnot": {"fidelity": 0.95}}}
 * }
 */
struct AWS_QDMI_Operation_impl_d {
  std::string name_;
  size_t num_qubits_ = 0;
  size_t num_params_ = 0;
  double fidelity_ = 0.0;
  std::vector<std::vector<AWS_QDMI_Site_impl_d*>> applicable_sites_;
};

/**
 * @brief Device job implementation.
 * 
 * AWS Braket Mapping:
 * - Represents a quantum task (circuit execution request)
 * - taskArn_ → AWS Braket task identifier returned from CreateQuantumTask
 * - format_ → QASM2/QASM3 maps to action field in CreateQuantumTaskRequest
 * - program_ → OpenQASM circuit string
 * - shots_ → number of measurements (SetShots())
 * - status_ → mapped from QuantumTaskStatus enum
 * 
 * AWS API Flow:
 * 1. submit() → CreateQuantumTask(action, deviceArn, shots, outputS3)
 *    Returns: taskArn_ = result.GetQuantumTaskArn()
 * 
 * 2. check() → GetQuantumTask(taskArn_)
 *    Returns: QuantumTaskStatus (CREATED, QUEUED, RUNNING, COMPLETED, FAILED, CANCELLED)
 *    Maps to: QDMI_JOB_STATUS_*
 * 
 * 3. getResults() → Parse S3 results or GetQuantumTask().GetResults()
 *    Format: measurement outcomes as histogram {"00": 45, "01": 23, "10": 18, "11": 14}
 * 
 * 4. cancel() → CancelQuantumTask(taskArn_)
 * 
 * Status Mapping:
 * - CREATED/QUEUED → QDMI_JOB_STATUS_SUBMITTED
 * - RUNNING → QDMI_JOB_STATUS_RUNNING
 * - COMPLETED → QDMI_JOB_STATUS_DONE
 * - FAILED → QDMI_JOB_STATUS_ERROR
 * - CANCELLED → QDMI_JOB_STATUS_CANCELLED
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
  std::map<std::string, size_t> counts_;
  std::vector<uint8_t> measurements_;

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
                  size_t* sizeRet) -> QDMI_STATUS;
};
