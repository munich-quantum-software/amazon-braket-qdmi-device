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
 * Typical Usage Pattern:
 * A quantum software stack (compiler, orchestrator) initializes this library
 * once, then creates multiple sessions to address many AWS QPUs concurrently
 * (e.g., IQM Garnet, Lucy, sv1 simulator). Each session can have its own AWS
 * credentials specified via:
 * - Credentials file (AUTHFILE parameter with INI format)
 * - Direct parameters (AWS_ACCESS_KEY_ID, AWS_SECRET_ACCESS_KEY,
 * AWS_SESSION_TOKEN)
 *
 * Device (Singleton - ONE per process)
 *   ├─ Initializes AWS SDK once via QDMI device_initialize()
 *   ├─ Tracks all active sessions across all users and devices
 *   ├─ Caches device architecture (shared across sessions to same ARN)
 *   ├─ Generates unique job IDs across all sessions
 *   └─ Thread-safe coordination (sessionsMutex_, rngMutex_, deviceCacheMutex_)
 *
 * Device_Session (MANY - one per user+device combination)
 *   ├─ Connects to specific AWS Braket device (Garnet, sv1, etc.)
 *   ├─ References cached device architecture (shared if same ARN)
 *   ├─ Has own BraketClient instance with explicit credentials or defaults
 *   ├─ Fetches mutable device status (ONLINE/OFFLINE) per query
 *   ├─ Creates and manages jobs for that user+device
 *   └─ Thread-safe job management (jobsMutex_)
 *
 * Device_Job (MANY - one per quantum circuit submission)
 *   ├─ Represents a quantum task/circuit execution
 *   ├─ Handles async execution on AWS infrastructure
 *   ├─ Stores results (measurement outcomes from S3)
 *   ├─ Maps to Amazon Braket QuantumTask
 *   └─ Thread-safe result fetching (jobMutex_)
 *
 * Site
 *   └─ Represents a physical qubit with coherence times
 *
 * Operation
 *   └─ Represents a quantum gate with parameters and fidelity
 */

#pragma once

#include "amazon-braket-qdmi-device/Constants.hpp"
#include "amazon_braket_qdmi/device.h"

#include <atomic>
#include <aws/braket/BraketClient.h>
#include <aws/braket/model/DeviceType.h>
#include <aws/core/auth/AWSCredentials.h>
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
struct AMAZON_BRAKET_QDMI_Site_impl_d;
struct AMAZON_BRAKET_QDMI_Operation_impl_d;
struct AMAZON_BRAKET_QDMI_Device_Job_impl_d;

namespace amazon::braket::qdmi {

/**
 * @brief Cached device architecture data shared across sessions.
 *
 * Immutable device properties that don't change between queries.
 * Multiple sessions to the same device ARN share one cached instance.
 */
struct DeviceArchitecture {
  std::string name;     // Real device name from AWS (e.g., "Garnet")
  std::string provider; // Provider name (e.g., "IQM")
  Aws::Braket::Model::DeviceType deviceType; // QPU or SIMULATOR
  size_t qubitsNum = 0;                      // Number of qubits

  // Device topology
  std::vector<std::unique_ptr<AMAZON_BRAKET_QDMI_Site_impl_d>> sites;
  std::vector<AMAZON_BRAKET_QDMI_Site_impl_d*> sitesPtr;
  std::unordered_map<std::string, AMAZON_BRAKET_QDMI_Site_impl_d*> sitesMap;

  // Supported operations
  std::vector<std::unique_ptr<AMAZON_BRAKET_QDMI_Operation_impl_d>> operations;
  std::vector<AMAZON_BRAKET_QDMI_Operation_impl_d*> operationsPtr;
  std::unordered_map<std::string, AMAZON_BRAKET_QDMI_Operation_impl_d*>
      operationsMap;

  // Connectivity (coupling map)
  std::vector<AMAZON_BRAKET_QDMI_Site_impl_d*> connectivity;
};

/**
 * @brief Main device singleton managing Amazon Braket device access.
 *
 * Design Rationale:
 * This singleton enables a quantum software stack to initialize AWS SDK once
 * (expensive), then concurrently address multiple AWS QPUs through separate
 * sessions (e.g., simultaneously submit to Garnet, Lucy, and sv1).
 *
 * Multi-Credential Support:
 * When multiple users (with different AWS credentials) connect to the same
 * device ARN, the immutable device architecture is fetched once and cached.
 * Only mutable properties (status) and credentials differ per session.
 *
 * The singleton coordinates:
 * - AWS SDK initialization/shutdown lifecycle
 * - Session registry (one session per user+device combination)
 * - Device architecture cache (one entry per unique device ARN)
 * - Unique job ID generation across all devices
 * - Thread-safe access to shared resources
 */
class Device final {
  std::unordered_map<AMAZON_BRAKET_QDMI_Device_Session,
                     std::unique_ptr<AMAZON_BRAKET_QDMI_Device_Session_impl_d>>
      sessions_;
  mutable std::mutex sessionsMutex_;

  // Cache of device architecture keyed by device ARN
  std::unordered_map<std::string, std::shared_ptr<DeviceArchitecture>>
      deviceCache_;
  mutable std::mutex deviceCacheMutex_;

  std::mt19937_64 rng_{std::random_device{}()};
  mutable std::mutex rngMutex_;
  std::uniform_int_distribution<> dis_{0, std::numeric_limits<int>::max()};

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

  auto sessionAlloc(AMAZON_BRAKET_QDMI_Device_Session* session) -> QDMI_STATUS;
  auto sessionFree(AMAZON_BRAKET_QDMI_Device_Session session) -> void;
  static auto queryProperty(QDMI_Device_Property prop, size_t size, void* value,
                            size_t* sizeRet) -> QDMI_STATUS;
  auto generateUniqueID() -> int;

  // Device architecture cache access
  auto getCachedArchitecture(const std::string& deviceArn) const
      -> std::shared_ptr<DeviceArchitecture>;
  auto setCachedArchitecture(const std::string& deviceArn,
                             std::shared_ptr<DeviceArchitecture> architecture)
      -> void;
};

} // namespace amazon::braket::qdmi

/**
 * @brief Device session implementation - one instance per user+device
 * combination.
 *
 * Each session represents a connection to a specific AWS Braket device with
 * specific credentials. Multiple sessions can exist concurrently to address
 * different QPUs or the same QPU with different user credentials.
 */
struct AMAZON_BRAKET_QDMI_Device_Session_impl_d {
private:
  bool initialized_ = false;

  std::string region_;
  std::string deviceArn_;

  // AWS Credentials
  std::string credentialsFile_; // Path to AWS credentials file (INI format)
  std::string accessKeyId_;     // AWS Access Key ID (CUSTOM3)
  std::string secretAccessKey_; // AWS Secret Access Key (CUSTOM4)
  std::string sessionToken_;    // AWS Session Token (CUSTOM5)

  // Cached device architecture (shared across sessions to same device ARN)
  mutable std::shared_ptr<amazon::braket::qdmi::DeviceArchitecture>
      cachedArchitecture_;
  mutable std::mutex cachedArchitectureMutex_;

  // Mutable device status (re-fetched per query, can change over time)
  mutable std::atomic<QDMI_Device_Status> braketDeviceStatus_{
      QDMI_DEVICE_STATUS_OFFLINE};

  std::unordered_map<AMAZON_BRAKET_QDMI_Device_Job,
                     std::unique_ptr<AMAZON_BRAKET_QDMI_Device_Job_impl_d>>
      jobs_;
  mutable std::mutex jobsMutex_;

  std::unique_ptr<Aws::Braket::BraketClient> client_;

public:
  auto init() -> QDMI_STATUS;
  auto setParameter(QDMI_Device_Session_Parameter param, size_t size,
                    const void* value) -> QDMI_STATUS;
  auto createDeviceJob(AMAZON_BRAKET_QDMI_Device_Job* job) -> QDMI_STATUS;
  auto freeDeviceJob(AMAZON_BRAKET_QDMI_Device_Job job) -> void;
  auto queryDeviceProperty(QDMI_Device_Property prop, size_t size, void* value,
                           size_t* sizeRet) const -> QDMI_STATUS;
  static auto querySiteProperty(AMAZON_BRAKET_QDMI_Site_impl_d* site,
                                QDMI_Site_Property prop, size_t size,
                                void* value, size_t* sizeRet) -> QDMI_STATUS;
  static auto queryOperationProperty(
      AMAZON_BRAKET_QDMI_Operation_impl_d* operation, size_t numSites,
      const AMAZON_BRAKET_QDMI_Site_impl_d* const* sites, size_t numParams,
      const double* params, QDMI_Operation_Property prop, size_t size,
      void* value, size_t* sizeRet) -> QDMI_STATUS;

private:
  auto fetchDeviceArchitecture() const -> QDMI_STATUS;

  [[nodiscard]] auto getClient() const -> Aws::Braket::BraketClient* {
    return client_.get();
  }
  [[nodiscard]] auto getDeviceArn() const -> const std::string& {
    return deviceArn_;
  }
  [[nodiscard]] auto getRegion() const -> const std::string& { return region_; }
  [[nodiscard]] auto getCredentials() const -> Aws::Auth::AWSCredentials {
    return Aws::Auth::AWSCredentials(accessKeyId_, secretAccessKey_,
                                     sessionToken_);
  }

  // Allow Job to access session internals
  friend struct AMAZON_BRAKET_QDMI_Device_Job_impl_d;
};

/**
 * @brief Site implementation structure.
 */
struct AMAZON_BRAKET_QDMI_Site_impl_d {
  std::string name_;
  size_t id_ = 0;
  uint64_t t1_ = 0; // T1 coherence time in microseconds
  uint64_t t2_ = 0; // T2 coherence time in microseconds
};

/**
 * @brief Operation implementation structure.
 */
struct AMAZON_BRAKET_QDMI_Operation_impl_d {
  std::string name_;
  size_t numQubits_ = 0;
  size_t numParams_ = 0;
  double fidelity_ = 0.0;
  std::vector<std::vector<AMAZON_BRAKET_QDMI_Site_impl_d*>> applicable_sites_;
};

/**
 * @brief Device job implementation.
 */
struct AMAZON_BRAKET_QDMI_Device_Job_impl_d {
private:
  AMAZON_BRAKET_QDMI_Device_Session_impl_d* session_;
  int id_ = 0;

  /// Quantum task execution status (lifecycle tracking)
  /// CREATED → QUEUED → RUNNING → DONE/CANCELED/FAILED
  /// - CREATED: Job object created, not yet submitted
  /// - QUEUED: Submitted to AWS, waiting to execute
  /// - RUNNING: Currently executing on quantum hardware
  /// - DONE: Execution completed successfully, results available
  /// - CANCELED: User canceled before completion
  /// - FAILED: Execution failed due to error
  mutable std::atomic<QDMI_Job_Status> status_{QDMI_JOB_STATUS_CREATED};

  QDMI_Program_Format format_ = QDMI_PROGRAM_FORMAT_QASM3;
  std::string program_;
  size_t shots_ = 100;
  std::string taskArn_;

  // Per-job S3 configuration (required)
  std::string jobS3Bucket_;    // Required - S3 bucket for results
  std::string jobS3Prefix_;    // Optional - S3 prefix, defaults to timestamp
  std::string reservationArn_; // Optional - dedicate task to a reserved window

  std::future<void> jobHandle_;
  mutable std::map<std::string, size_t> counts_;
  mutable std::string shotsString_; // Comma-separated shots: "00,11,00,..."
  mutable bool resultsFetched_ = false;
  mutable std::string outputS3Bucket_;
  mutable std::string outputS3Directory_;
  mutable std::mutex jobMutex_;

  // Helpers to fetch and parse results from S3
  auto fetchResults() const -> QDMI_STATUS;         // Locks and calls internal
  auto fetchResultsInternal() const -> QDMI_STATUS; // Assumes jobMutex_ is held

public:
  explicit AMAZON_BRAKET_QDMI_Device_Job_impl_d(
      AMAZON_BRAKET_QDMI_Device_Session_impl_d* session)
      : session_(session),
        id_(amazon::braket::qdmi::Device::get().generateUniqueID()) {}

  auto getSession() -> AMAZON_BRAKET_QDMI_Device_Session_impl_d* {
    return session_;
  }

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
