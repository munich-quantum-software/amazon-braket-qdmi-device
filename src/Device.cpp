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

/** @file
 * @brief QDMI device implementation for Amazon Braket
 *
 * This file implements the Quantum Device Management Interface (QDMI)
 * specification for Amazon Braket.
 *
 * QDMI Project: https://github.com/Munich-Quantum-Software-Stack/QDMI
 *
 * ============================================================================
 * Purpose: QDMI Adapter for Amazon Braket
 * ============================================================================
 *
 * You can target Amazon Braket devices by linking against this library
 * instead of another QDMI implementation. Your OpenQASM circuits will
 * execute on Amazon Braket.
 *
 * ============================================================================
 * QDMI to Amazon Braket Mapping
 * ============================================================================
 *
 * This implementation translates QDMI standard calls into Amazon Braket SDK
 * calls:
 *
 * QDMI Concept          | Amazon Braket Equivalent
 * ----------------------|--------------------------------------------------
 * Device                | BraketClient + GetDeviceRequest/Result
 * Session               | BraketClient instance with credentials
 *
 * Job                   | QuantumTask (single circuit execution)
 * Job Status            | QuantumTaskStatus (CREATED, QUEUED, RUNNING, etc.)
 * Job Submission        | BraketClient::CreateQuantumTask()
 * Job Cancellation      | BraketClient::CancelQuantumTask()
 *
 * Program               | Action field (OpenQASM string wrapped in JSON)
 * Shots                 | CreateQuantumTaskRequest::SetShots()
 *
 * Site (Qubit)          | Parsed from paradigm.qubitCount
 * Operation (Gate)      | Parsed from action.braket.ir.openqasm.program
 * Coupling Map          | Full connectivity (for simulators)
 *
 */

#include "amazon-braket-qdmi-device/Device.hpp"

#include "amazon-braket-qdmi-device/Constants.hpp"
#include "amazon-braket-qdmi-device/DeviceParser.hpp"
#include "amazon_braket_qdmi/constants.h"
#include "amazon_braket_qdmi/device.h"
#include "aws/core/utils/Array.h"

#include <algorithm>
#include <aws/braket/BraketClient.h>
#include <aws/braket/model/Association.h>
#include <aws/braket/model/AssociationType.h>
#include <aws/braket/model/CancelQuantumTaskRequest.h>
#include <aws/braket/model/CreateQuantumTaskRequest.h>
#include <aws/braket/model/DeviceStatus.h>
#include <aws/braket/model/DeviceType.h>
#include <aws/braket/model/GetDeviceRequest.h>
#include <aws/braket/model/GetDeviceResult.h>
#include <aws/braket/model/GetQuantumTaskRequest.h>
#include <aws/braket/model/QuantumTaskStatus.h>
#include <aws/braket/model/SearchDevicesFilter.h>
#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentials.h>
#include <aws/core/client/ClientConfiguration.h>
#include <aws/core/utils/json/JsonSerializer.h>
#include <aws/core/utils/memory/stl/AWSString.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/S3ClientConfiguration.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <ostream>
#include <sstream>
#include <string>
// NOLINTNEXTLINE(modernize-deprecated-headers) - strnlen is POSIX, not in
// <cstring>
#include <string.h>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

// NOLINTBEGIN(bugprone-macro-parentheses)
#define ADD_SINGLE_VALUE_PROPERTY(prop_name, prop_type, prop_value, prop,      \
                                  size, value, size_ret)                       \
  {                                                                            \
    if ((prop) == (prop_name)) {                                               \
      if ((value) != nullptr) {                                                \
        if ((size) < sizeof(prop_type)) {                                      \
          return QDMI_ERROR_INVALIDARGUMENT;                                   \
        }                                                                      \
        *static_cast<prop_type*>((value)) = (prop_value);                      \
      }                                                                        \
      if ((size_ret) != nullptr) {                                             \
        *(size_ret) = sizeof(prop_type);                                       \
      }                                                                        \
      return QDMI_SUCCESS;                                                     \
    }                                                                          \
  } // NOLINT(bugprone-macro-parentheses)

#define ADD_STRING_PROPERTY(prop_name, prop_value, prop, size, value,          \
                            size_ret)                                          \
  {                                                                            \
    if ((prop) == (prop_name)) {                                               \
      const size_t len = strlen((prop_value));                                 \
      if ((value) != nullptr) {                                                \
        if ((size) < len + 1) {                                                \
          return QDMI_ERROR_INVALIDARGUMENT;                                   \
        }                                                                      \
        memcpy(value, (prop_value), len);                                      \
        static_cast<char*>(value)[len] = '\0';                                 \
      }                                                                        \
      if ((size_ret) != nullptr) {                                             \
        *(size_ret) = len + 1;                                                 \
      }                                                                        \
      return QDMI_SUCCESS;                                                     \
    }                                                                          \
  }

#define ADD_LIST_PROPERTY(prop_name, prop_type, prop_values, prop, size,       \
                          value, size_ret)                                     \
  {                                                                            \
    if ((prop) == (prop_name)) {                                               \
      if ((value) != nullptr) {                                                \
        if ((size) < ((prop_values)).size() * sizeof(prop_type)) {             \
          return QDMI_ERROR_INVALIDARGUMENT;                                   \
        }                                                                      \
        memcpy(static_cast<void*>(value),                                      \
               static_cast<const void*>(((prop_values)).data()),               \
               ((prop_values)).size() * sizeof(prop_type));                    \
      }                                                                        \
      if ((size_ret) != nullptr) {                                             \
        *(size_ret) = ((prop_values)).size() * sizeof(prop_type);              \
      }                                                                        \
      return QDMI_SUCCESS;                                                     \
    }                                                                          \
  }
// NOLINTEND(bugprone-macro-parentheses)

namespace {
/**
 * @brief Parse AWS credentials from an INI-format credentials file.
 *
 * Reads the first profile section found in the credentials file.
 * Only one profile section should be present in the file.
 * Format:
 * [default]
 * aws_access_key_id=AKIA...
 * aws_secret_access_key=...
 * aws_session_token=... (optional)
 *
 * @param filePath Path to the credentials file
 * @param accessKeyId Output parameter for access key
 * @param secretAccessKey Output parameter for secret key
 * @param sessionToken Output parameter for session token (optional)
 * @return true if credentials were successfully parsed, false otherwise
 */
auto parseCredentialsFile(const std::string& filePath, std::string& accessKeyId,
                          std::string& secretAccessKey,
                          std::string& sessionToken) -> bool {
  std::ifstream file(filePath);
  if (!file.is_open()) {
    std::cerr << "ERROR: Failed to open credentials file: " << filePath << "\n";
    return false;
  }

  std::string line;
  std::string currentProfile;
  std::string firstProfile;     // Track the first profile name
  bool inTargetProfile = false; // Only parse after finding a profile header
  bool foundCredentials = false;

  while (std::getline(file, line)) {
    // Trim whitespace
    line.erase(0, line.find_first_not_of(" \t\r\n"));
    line.erase(line.find_last_not_of(" \t\r\n") + 1);

    // Skip empty lines and comments
    if (line.empty() || line[0] == '#' || line[0] == ';') {
      continue;
    }

    // Check for profile header [default] or [profile_name]
    if (line[0] == '[' && line[line.length() - 1] == ']') {
      currentProfile = line.substr(1, line.length() - 2);
      if (foundCredentials) {
        // Multiple profiles detected - warn and use the first one
        std::cerr << "WARNING: Multiple profiles detected in credentials file. "
                  << "Using first profile [" << firstProfile << "]\\n";
        break;
      }
      if (firstProfile.empty()) {
        firstProfile = currentProfile;
      }
      inTargetProfile = true;
      continue;
    }

    // Parse key=value pairs within the target profile
    if (inTargetProfile) {
      const size_t equalPos = line.find('=');
      if (equalPos != std::string::npos) {
        std::string key = line.substr(0, equalPos);
        std::string value = line.substr(equalPos + 1);

        // Trim whitespace from key and value
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);

        if (key == "aws_access_key_id") {
          accessKeyId = value;
          foundCredentials = true;
        } else if (key == "aws_secret_access_key") {
          secretAccessKey = value;
        } else if (key == "aws_session_token") {
          sessionToken = value;
        }
      }
    }
  }

  if (!foundCredentials || accessKeyId.empty() || secretAccessKey.empty()) {
    std::cerr << "ERROR: Invalid credentials file format or missing required "
                 "fields\n";
    std::cerr
        << "Expected format (only one profile section should be present):\n";
    std::cerr << "[default]\n";
    std::cerr << "aws_access_key_id=AKIA...\n";
    std::cerr << "aws_secret_access_key=...\n";
    std::cerr << "aws_session_token=... (optional)\n";
    return false;
  }

  return true;
}

/**
 * @brief Threshold for considering an ONLINE device as BUSY.
 *
 * If the total number of queued quantum tasks across all queue entries
 * is >= this value, the device is reported as QDMI_DEVICE_STATUS_BUSY.
 * Below this value, the device is reported as QDMI_DEVICE_STATUS_IDLE.
 *
 */
constexpr int QUEUE_BUSY_THRESHOLD = 5;

/**
 * @brief Compute total queue depth from a Braket GetDevice result.
 *
 * Sums the `queueSize` field across all entries in `deviceQueueInfo`.
 * AWS returns queue sizes as strings (e.g. "12" or ">50"); values that
 * cannot be parsed as integers are treated as QUEUE_BUSY_THRESHOLD to
 * indicate "definitely busy".
 *
 * @param result Result of a successful BraketClient::GetDevice() call
 * @return Total queue depth (>= 0)
 */
auto getTotalQueueDepth(const Aws::Braket::Model::GetDeviceResult& result)
    -> int {
  int totalDepth = 0;
  for (const auto& queueItem : result.GetDeviceQueueInfo()) {
    const Aws::String& sizeStr = queueItem.GetQueueSize();
    try {
      totalDepth += std::stoi(std::string(sizeStr));
    } catch (...) {
      // Non-integer strings like ">50" indicate a busy queue; treat as
      // threshold to ensure the device is reported as BUSY.
      totalDepth += QUEUE_BUSY_THRESHOLD;
    }
  }
  return totalDepth;
}
} // anonymous namespace

namespace amazon::braket::qdmi {

/**
 * Device constructor - initializes the global Braket device singleton.
 *
 * This singleton manages all sessions and provides library-level properties.
 * In QDMI terminology:
 * - Braket device = this library singleton (manages sessions, caching)
 * - Session device = actual quantum backend from AWS Braket
 *
 * The Braket device maintains:
 * - Session registry for lifecycle management
 * - Session device architecture cache (shared across credentials)
 * - Random number generation for unique job IDs
 */
Device::Device() = default;

auto Device::sessionAlloc(AMAZON_BRAKET_QDMI_Device_Session* session)
    -> QDMI_STATUS {
  if (session == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  auto uniqueSession =
      std::make_unique<AMAZON_BRAKET_QDMI_Device_Session_impl_d>();
  const std::scoped_lock<std::mutex> lock(sessionsMutex_);
  const auto& it =
      sessions_.emplace(uniqueSession.get(), std::move(uniqueSession))
          .first; // NOLINT(misc-include-cleaner)
  *session = it->first;
  return QDMI_SUCCESS;
}

auto Device::sessionFree(AMAZON_BRAKET_QDMI_Device_Session session) -> void {
  if (session != nullptr) {
    const std::scoped_lock<std::mutex> lock(sessionsMutex_);
    if (const auto& it = sessions_.find(session); it != sessions_.end()) {
      sessions_.erase(it);
    }
  }
}

auto Device::queryProperty(const QDMI_Device_Property prop, const size_t size,
                           void* value, size_t* sizeRet) -> QDMI_STATUS {
  // Validate arguments and reject MAX sentinel value
  if ((value != nullptr && size == 0) || prop == QDMI_DEVICE_PROPERTY_MAX) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }

  // Braket device properties (library-level, not session device-specific)
  ADD_STRING_PROPERTY(QDMI_DEVICE_PROPERTY_LIBRARYVERSION, QDMI_VERSION, prop,
                      size, value, sizeRet)

  // Session device properties are handled by Session, not Braket device
  // singleton
  return QDMI_ERROR_NOTSUPPORTED;
}

auto Device::generateUniqueID() -> int {
  const std::scoped_lock<std::mutex> lock(rngMutex_);
  return dis_(rng_);
}

auto Device::getCachedArchitecture(const std::string& deviceArn) const
    -> std::shared_ptr<DeviceArchitecture> {
  const std::scoped_lock<std::mutex> lock(deviceCacheMutex_);
  auto it = deviceCache_.find(deviceArn);
  if (it != deviceCache_.end()) {
    return it->second;
  }
  return nullptr;
}

auto Device::setCachedArchitecture(
    const std::string& deviceArn,
    std::shared_ptr<DeviceArchitecture> architecture) -> void {
  const std::scoped_lock<std::mutex> lock(deviceCacheMutex_);
  deviceCache_[deviceArn] =
      std::move(architecture); // NOLINT(misc-include-cleaner)
}

} // namespace amazon::braket::qdmi

// ============================================================================
// Session Implementation
// ============================================================================

/**
 * Fetches the session device architecture from Amazon Braket.
 *
 * This function queries the session device properties:
 * - Number of qubits (sites)
 * - Qubit connectivity (which qubits can interact)
 * - Available quantum operations
 * - Device operational status (ONLINE/OFFLINE/RETIRED)
 *
 * Design: Uses Braket device singleton cache to avoid redundant fetches when
 * multiple sessions connect to the same device ARN (even with different
 * credentials). Immutable properties are cached; mutable status is always
 * re-fetched.
 *
 * @return QDMI_SUCCESS on successful fetch, error code otherwise
 */
auto AMAZON_BRAKET_QDMI_Device_Session_impl_d::fetchDeviceArchitecture() const
    -> QDMI_STATUS {
  if (deviceArn_.empty() || client_ == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }

  // Check if architecture is already cached
  cachedArchitecture_ =
      amazon::braket::qdmi::Device::get().getCachedArchitecture(deviceArn_);

  if (cachedArchitecture_ != nullptr) {
    // Cache hit: Only fetch mutable session device status
    Aws::Braket::Model::GetDeviceRequest request;
    request.SetDeviceArn(deviceArn_.c_str());
    auto outcome = client_->GetDevice(request);
    if (outcome.IsSuccess()) {
      const auto& device = outcome.GetResult();
      const auto braketStatus = device.GetDeviceStatus();
      switch (braketStatus) {
      case Aws::Braket::Model::DeviceStatus::ONLINE: {
        // Differentiate IDLE vs BUSY based on current queue depth
        const int queueDepth = getTotalQueueDepth(device);
        braketDeviceStatus_.store(queueDepth >= QUEUE_BUSY_THRESHOLD
                                      ? QDMI_DEVICE_STATUS_BUSY
                                      : QDMI_DEVICE_STATUS_IDLE);
        break;
      }
      case Aws::Braket::Model::DeviceStatus::OFFLINE:
        braketDeviceStatus_.store(QDMI_DEVICE_STATUS_MAINTENANCE);
        break;
      case Aws::Braket::Model::DeviceStatus::RETIRED:
        braketDeviceStatus_.store(QDMI_DEVICE_STATUS_OFFLINE);
        break;
      case Aws::Braket::Model::DeviceStatus::NOT_SET:
      default:
        std::cerr << "ERROR: Unknown device status (enum value: "
                  << static_cast<int>(braketStatus) << ")\n";
        return QDMI_ERROR_NOTSUPPORTED;
      }
    }
    return QDMI_SUCCESS;
  }

  // Cache miss: Request full device architecture.
  Aws::Braket::Model::GetDeviceRequest request;
  request.SetDeviceArn(deviceArn_.c_str());

  auto outcome = client_->GetDevice(request);
  if (!outcome.IsSuccess()) {
    std::cerr << "Failed to get device: " << outcome.GetError().GetMessage()
              << "\n";
    std::cerr << "Please ensure you have configured your AWS credentials "
                 "correctly.\n";
    std::cerr
        << "You can set them via environment variables (AWS_ACCESS_KEY_ID, "
           "AWS_SECRET_ACCESS_KEY) or in ~/.aws/credentials.\n";
    return QDMI_ERROR_NOTSUPPORTED;
  }

  const auto& device = outcome.GetResult();

  // Create new cached architecture
  auto architecture =
      std::make_shared<amazon::braket::qdmi::DeviceArchitecture>();
  architecture->name = device.GetDeviceName();
  architecture->provider = device.GetProviderName();
  architecture->deviceType = device.GetDeviceType();

  // Map Amazon Braket DeviceStatus to QDMI Device Status
  // Amazon Braket has three statuses: ONLINE, OFFLINE, RETIRED
  // QDMI has: OFFLINE, IDLE, BUSY, ERROR, MAINTENANCE, CALIBRATION
  const auto braketStatus = device.GetDeviceStatus();

  switch (braketStatus) {
  case Aws::Braket::Model::DeviceStatus::ONLINE: {
    // Differentiate IDLE vs BUSY based on current queue depth.
    // Device still accepts submissions in the BUSY state; this is
    // purely informational.
    const int queueDepth = getTotalQueueDepth(device);
    braketDeviceStatus_.store(queueDepth >= QUEUE_BUSY_THRESHOLD
                                  ? QDMI_DEVICE_STATUS_BUSY
                                  : QDMI_DEVICE_STATUS_IDLE);
    break;
  }

  case Aws::Braket::Model::DeviceStatus::OFFLINE:
    // Device is temporarily unavailable (maintenance/calibration),
    // tasks will queue until it returns
    braketDeviceStatus_.store(QDMI_DEVICE_STATUS_MAINTENANCE);
    break;

  case Aws::Braket::Model::DeviceStatus::RETIRED:
    // Device is permanently decommissioned
    braketDeviceStatus_.store(QDMI_DEVICE_STATUS_OFFLINE);
    std::cerr << "ERROR: Device " << device.GetDeviceName()
              << " is RETIRED and permanently unavailable.\n";
    std::cerr << "Please update your device ARN to a newer generation.\n";
    return QDMI_ERROR_NOTSUPPORTED;

  case Aws::Braket::Model::DeviceStatus::NOT_SET:
  default:
    std::cerr << "ERROR: Unknown device status (enum value: "
              << static_cast<int>(braketStatus) << ")\n";
    return QDMI_ERROR_NOTSUPPORTED;
  }

  // Parse session device properties JSON
  const auto& propertiesStr = device.GetDeviceCapabilities();

  Aws::Utils::Json::JsonValue const json(propertiesStr);
  if (!json.WasParseSuccessful()) {
    std::cerr << "Failed to parse session device properties JSON\n";
    return QDMI_ERROR_FATAL;
  }

  // Create Provider-Specific Parser
  auto parser =
      CreateDeviceParser(architecture->deviceType, architecture->provider);
  if (parser == nullptr) {
    const char* deviceTypeStr =
        (architecture->deviceType == Aws::Braket::Model::DeviceType::QPU)
            ? "QPU"
            : "Simulator";
    std::cerr << "Unsupported device type or provider: " << deviceTypeStr
              << " / " << architecture->provider << "\n";
    return QDMI_ERROR_NOTSUPPORTED;
  }

  // Parse Session Device Properties
  ParsedDeviceProperties properties;
  auto status = parser->ParseProperties(json.View(), properties);
  if (status != QDMI_SUCCESS) {
    std::cerr << "Failed to parse session device properties\n";
    return static_cast<QDMI_STATUS>(status);
  }

  // Transfer Parsed Data to Cached Architecture
  architecture->qubitsNum = properties.qubitCount;
  architecture->connectivity = std::move(properties.connectivity);

  architecture->sites = std::move(properties.sites);
  architecture->sitesPtr = std::move(properties.sitesPtr);
  architecture->sitesMap = std::move(properties.sitesMap);

  architecture->operations = std::move(properties.operations);
  architecture->operationsPtr = std::move(properties.operationsPtr);
  architecture->operationsMap = std::move(properties.operationsMap);

  // Store in singleton cache and use in this session
  amazon::braket::qdmi::Device::get().setCachedArchitecture(deviceArn_,
                                                            architecture);
  cachedArchitecture_ = architecture;
  return QDMI_SUCCESS;
}

/**
 * Initialize a device session.
 *
 * Session initialization involves:
 * 1. Validating required parameters (device ARN and credentials)
 * 2. Setting up the AWS SDK client with proper region configuration
 * 3. Transitioning the session to INITIALIZED state
 *
 * Configuration:
 * QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN
 * - Credentials: Must be set via one of:
 *   - QDMI_DEVICE_SESSION_PARAMETER_AUTHFILE (credentials file path)
 *   - QDMI_DEVICE_SESSION_PARAMETER_AWS_ACCESS_KEY_ID + AWS_SECRET_ACCESS_KEY +
 * QDMI_DEVICE_SESSION_PARAMETER_AWS_SESSION_TOKEN (optional)
 *  - QDMI_DEVICE_SESSION_PARAMETER_REGION (defaults to value from ARN or
 * us-east-1)
 *
 *
 * @return QDMI_SUCCESS on successful initialization
 * @return QDMI_ERROR_BADSTATE if session is not in ALLOCATED state
 * @return QDMI_ERROR_INVALIDARGUMENT if device ARN is not configured
 */
auto AMAZON_BRAKET_QDMI_Device_Session_impl_d::init() -> QDMI_STATUS {
  if (initialized_) {
    return QDMI_ERROR_BADSTATE;
  }

  // Check that required parameters are set
  if (deviceArn_.empty()) {
    std::cerr << "ERROR: Device ARN not configured. Set via:\n";
    std::cerr << "  AMAZON_BRAKET_QDMI_device_session_set_parameter() with "
                 "QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN\n";
    return QDMI_ERROR_INVALIDARGUMENT;
  }

  // Extract region from ARN if not explicitly set
  // ARN format: arn:aws:braket:<region>::device/... or
  // arn:aws:braket:::<device> (global)
  if (region_.empty() && !deviceArn_.empty()) {
    // Parse ARN: arn:aws:braket:REGION::device/...
    const size_t start = deviceArn_.find("braket:");
    if (start != std::string::npos) {
      size_t const startPos = start + 7; // Skip "braket:"
      const size_t end = deviceArn_.find(':', startPos);
      if (end != std::string::npos && end > startPos) {
        region_ = deviceArn_.substr(startPos, end - startPos);
      }
    }
    // If region is still empty (global ARN like simulators), default to
    // us-east-1
    if (region_.empty()) {
      region_ = "us-east-1";
    }
  }

  // Configure AWS client with region
  Aws::Client::ClientConfiguration config;
  if (!region_.empty()) {
    config.region = region_;
  }

  // Parse credentials file if provided (takes precedence over direct
  // parameters)
  if (!credentialsFile_.empty()) {
    std::string accessKeyId;
    std::string secretAccessKey;
    std::string sessionToken;
    if (parseCredentialsFile(credentialsFile_, accessKeyId, secretAccessKey,
                             sessionToken)) {
      accessKeyId_ = accessKeyId;
      secretAccessKey_ = secretAccessKey;
      sessionToken_ = sessionToken;
    } else {
      std::cerr << "ERROR: Failed to parse credentials file: "
                << credentialsFile_ << "\n";
      return QDMI_ERROR_INVALIDARGUMENT;
    }
  }

  // Create BraketClient with explicit credentials
  // Credentials MUST be provided via one of these methods:
  // 1. QDMI_DEVICE_SESSION_PARAMETER_AUTHFILE (credentials file)
  // 2. QDMI_DEVICE_SESSION_PARAMETER_AWS_ACCESS_KEY_ID + AWS_SECRET_ACCESS_KEY
  if (!accessKeyId_.empty() && !secretAccessKey_.empty()) {
    // Explicit credentials provided via setParameter() or credentials file
    const Aws::Auth::AWSCredentials credentials(
        accessKeyId_, secretAccessKey_,
        sessionToken_.empty() ? "" : sessionToken_);
    client_ = std::make_unique<Aws::Braket::BraketClient>(credentials, config);
  } else {
    // No credentials provided - return error
    std::cerr << "ERROR: AWS credentials required but not provided.\n";
    std::cerr << "Please provide credentials via one of these methods:\n";
    std::cerr << "  1. QDMI_DEVICE_SESSION_PARAMETER_AUTHFILE (path to "
                 "credentials file)\n";
    std::cerr << "  2. QDMI_DEVICE_SESSION_PARAMETER_AWS_ACCESS_KEY_ID + "
                 "AWS_SECRET_ACCESS_KEY\n";
    return QDMI_ERROR_INVALIDARGUMENT;
  }

  initialized_ = true;
  return QDMI_SUCCESS;
}

auto AMAZON_BRAKET_QDMI_Device_Session_impl_d::setParameter(
    const QDMI_Device_Session_Parameter param, const size_t size,
    const void* value) -> QDMI_STATUS {
  // Check for invalid arguments
  if (value == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }

  if (size == 0) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }

  // Validate parameter: must be standard QDMI param or one of the specifically
  // defined custom params (DEVICEARN, REGION, AWS_ACCESS_KEY_ID,
  // AWS_SECRET_ACCESS_KEY, AWS_SESSION_TOKEN)
  const bool isStandardParam = param < QDMI_DEVICE_SESSION_PARAMETER_MAX;
  const bool isDefinedCustomParam =
      (param == QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN ||
       param == QDMI_DEVICE_SESSION_PARAMETER_REGION ||
       param == QDMI_DEVICE_SESSION_PARAMETER_AWS_ACCESS_KEY_ID ||
       param == QDMI_DEVICE_SESSION_PARAMETER_AWS_SECRET_ACCESS_KEY ||
       param == QDMI_DEVICE_SESSION_PARAMETER_AWS_SESSION_TOKEN);

  if (!isStandardParam && !isDefinedCustomParam) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }

  // Parameters can only be set before initialization
  if (initialized_) {
    return QDMI_ERROR_BADSTATE;
  }

  // Handle Amazon Braket custom parameters
  switch (param) {
  case QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN: {
    // Device ARN (required)
    const auto* arnStr = static_cast<const char*>(value);
    if (strnlen(arnStr, size) >= size) {
      return QDMI_ERROR_INVALIDARGUMENT; // Not null-terminated
    }
    deviceArn_ = arnStr;
    return QDMI_SUCCESS;
  }

  case QDMI_DEVICE_SESSION_PARAMETER_REGION: {
    // AWS Region (optional - can be extracted from ARN)
    const auto* regionStr = static_cast<const char*>(value);
    if (strnlen(regionStr, size) >= size) {
      return QDMI_ERROR_INVALIDARGUMENT; // Not null-terminated
    }
    region_ = regionStr;
    return QDMI_SUCCESS;
  }

  // Method 1: AWS Credentials File (AUTHFILE)
  // Supports standard AWS credentials file format with profiles
  case QDMI_DEVICE_SESSION_PARAMETER_AUTHFILE: {
    const auto* filePath = static_cast<const char*>(value);
    if (strnlen(filePath, size) >= size) {
      return QDMI_ERROR_INVALIDARGUMENT; // Not null-terminated
    }
    credentialsFile_ = filePath;
    return QDMI_SUCCESS;
  }

  // Method 2: Direct Credential Parameters (CUSTOM3-5)
  // For programmatic credential specification
  case QDMI_DEVICE_SESSION_PARAMETER_CUSTOM3: { // AWS_ACCESS_KEY_ID
    const auto* accessKey = static_cast<const char*>(value);
    if (strnlen(accessKey, size) >= size) {
      return QDMI_ERROR_INVALIDARGUMENT; // Not null-terminated
    }
    accessKeyId_ = accessKey;
    return QDMI_SUCCESS;
  }

  case QDMI_DEVICE_SESSION_PARAMETER_CUSTOM4: { // AWS_SECRET_ACCESS_KEY
    const auto* secretKey = static_cast<const char*>(value);
    if (strnlen(secretKey, size) >= size) {
      return QDMI_ERROR_INVALIDARGUMENT; // Not null-terminated
    }
    secretAccessKey_ = secretKey;
    return QDMI_SUCCESS;
  }

  case QDMI_DEVICE_SESSION_PARAMETER_CUSTOM5: { // AWS_SESSION_TOKEN
    const auto* token = static_cast<const char*>(value);
    if (strnlen(token, size) >= size) {
      return QDMI_ERROR_INVALIDARGUMENT; // Not null-terminated
    }
    sessionToken_ = token;
    return QDMI_SUCCESS;
  }
  default:
    break;
  }
  return QDMI_ERROR_NOTSUPPORTED;
}

auto AMAZON_BRAKET_QDMI_Device_Session_impl_d::createDeviceJob(
    AMAZON_BRAKET_QDMI_Device_Job* job) -> QDMI_STATUS {
  if (job == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  if (!initialized_) {
    return QDMI_ERROR_BADSTATE;
  }

  // Amazon Braket validates device availability during CreateQuantumTask().
  auto uniqueJob = std::make_unique<AMAZON_BRAKET_QDMI_Device_Job_impl_d>(this);
  const std::scoped_lock<std::mutex> lock(jobsMutex_);
  *job = jobs_.emplace(uniqueJob.get(), std::move(uniqueJob)).first->first;
  return QDMI_SUCCESS;
}

auto AMAZON_BRAKET_QDMI_Device_Session_impl_d::freeDeviceJob(
    AMAZON_BRAKET_QDMI_Device_Job job) -> void {
  if (job != nullptr) {
    const std::scoped_lock<std::mutex> lock(jobsMutex_);
    jobs_.erase(job);
  }
}

auto AMAZON_BRAKET_QDMI_Device_Session_impl_d::queryDeviceProperty(
    const QDMI_Device_Property prop, const size_t size, void* value,
    size_t* sizeRet) const -> QDMI_STATUS {
  if (!initialized_) {
    return QDMI_ERROR_BADSTATE;
  }

  // Fetch session device architecture on first property query (uses cache if
  // available)
  if (cachedArchitecture_ == nullptr) {
    const auto ret = fetchDeviceArchitecture();
    if (ret != QDMI_SUCCESS) {
      return ret;
    }
  }

  // Session device architecture properties (from cache)
  ADD_LIST_PROPERTY(QDMI_DEVICE_PROPERTY_SITES, AMAZON_BRAKET_QDMI_Site,
                    cachedArchitecture_->sitesPtr, prop, size, value, sizeRet)
  ADD_LIST_PROPERTY(
      QDMI_DEVICE_PROPERTY_OPERATIONS, AMAZON_BRAKET_QDMI_Operation,
      cachedArchitecture_->operationsPtr, prop, size, value, sizeRet)
  ADD_LIST_PROPERTY(QDMI_DEVICE_PROPERTY_COUPLINGMAP, AMAZON_BRAKET_QDMI_Site,
                    cachedArchitecture_->connectivity, prop, size, value,
                    sizeRet)
  ADD_SINGLE_VALUE_PROPERTY(QDMI_DEVICE_PROPERTY_QUBITSNUM, size_t,
                            cachedArchitecture_->qubitsNum, prop, size, value,
                            sizeRet)
  ADD_STRING_PROPERTY(QDMI_DEVICE_PROPERTY_NAME,
                      cachedArchitecture_->name.c_str(), prop, size, value,
                      sizeRet)

  // Return device status from Amazon Braket (mutable, per-query)
  ADD_SINGLE_VALUE_PROPERTY(QDMI_DEVICE_PROPERTY_STATUS, QDMI_Device_Status,
                            braketDeviceStatus_.load(), prop, size, value,
                            sizeRet)

  // Delegate to Braket device singleton for library-level properties only
  // (LIBRARYVERSION, NEEDSCALIBRATION)
  return amazon::braket::qdmi::Device::queryProperty(prop, size, value,
                                                     sizeRet);
}

auto AMAZON_BRAKET_QDMI_Device_Session_impl_d::querySiteProperty(
    AMAZON_BRAKET_QDMI_Site_impl_d* site, const QDMI_Site_Property prop,
    const size_t size, void* value, size_t* sizeRet) -> QDMI_STATUS {
  if (site == nullptr || (value != nullptr && size == 0) ||
      prop == QDMI_SITE_PROPERTY_MAX) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }

  ADD_SINGLE_VALUE_PROPERTY(QDMI_SITE_PROPERTY_INDEX, uint64_t, site->id_, prop,
                            size, value, sizeRet)
  ADD_STRING_PROPERTY(QDMI_SITE_PROPERTY_NAME, site->name_.c_str(), prop, size,
                      value, sizeRet)
  if (site->t1_ > 0) {
    ADD_SINGLE_VALUE_PROPERTY(QDMI_SITE_PROPERTY_T1, uint64_t, site->t1_, prop,
                              size, value, sizeRet)
  }
  if (site->t2_ > 0) {
    ADD_SINGLE_VALUE_PROPERTY(QDMI_SITE_PROPERTY_T2, uint64_t, site->t2_, prop,
                              size, value, sizeRet)
  }

  return QDMI_ERROR_NOTSUPPORTED;
}

auto AMAZON_BRAKET_QDMI_Device_Session_impl_d::queryOperationProperty(
    AMAZON_BRAKET_QDMI_Operation_impl_d* operation, const size_t numSites,
    const AMAZON_BRAKET_QDMI_Site_impl_d* const* sites, const size_t numParams,
    const double* params, const QDMI_Operation_Property prop, const size_t size,
    void* value, size_t* sizeRet) -> QDMI_STATUS {
  if (operation == nullptr || (value != nullptr && size == 0) ||
      (sites != nullptr && numSites == 0) ||
      (params != nullptr && numParams == 0) ||
      prop == QDMI_OPERATION_PROPERTY_MAX) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }

  ADD_STRING_PROPERTY(QDMI_OPERATION_PROPERTY_NAME, operation->name_.c_str(),
                      prop, size, value, sizeRet)
  ADD_SINGLE_VALUE_PROPERTY(QDMI_OPERATION_PROPERTY_QUBITSNUM, size_t,
                            operation->numQubits_, prop, size, value, sizeRet)
  ADD_SINGLE_VALUE_PROPERTY(QDMI_OPERATION_PROPERTY_PARAMETERSNUM, size_t,
                            operation->numParams_, prop, size, value, sizeRet)
  ADD_SINGLE_VALUE_PROPERTY(QDMI_OPERATION_PROPERTY_FIDELITY, double,
                            operation->fidelity_, prop, size, value, sizeRet)

  return QDMI_ERROR_NOTSUPPORTED;
}

// Job implementation
auto AMAZON_BRAKET_QDMI_Device_Job_impl_d::setParameter(
    const QDMI_Device_Job_Parameter param, const size_t size, const void* value)
    -> QDMI_STATUS {
  // Validate parameter: must be standard QDMI param or one of the specifically
  // defined custom params (OUTPUTS3BUCKET, OUTPUTS3PREFIX)
  const bool isStandardParam = param < QDMI_DEVICE_JOB_PARAMETER_MAX;
  const bool isDefinedCustomParam =
      (param == QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3BUCKET ||
       param == QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3PREFIX ||
       param == QDMI_DEVICE_JOB_PARAMETER_RESERVATION_ARN);

  if ((value != nullptr && size == 0) ||
      (!isStandardParam && !isDefinedCustomParam)) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }

  const auto currentStatus = status_.load();
  if (currentStatus != QDMI_JOB_STATUS_CREATED) {
    return QDMI_ERROR_BADSTATE;
  }

  const std::scoped_lock<std::mutex> lock(jobMutex_);
  if (param == QDMI_DEVICE_JOB_PARAMETER_SHOTSNUM) {
    if (value == nullptr || size != sizeof(size_t)) {
      return QDMI_ERROR_INVALIDARGUMENT;
    }
    shots_ = *static_cast<const size_t*>(value);
    return QDMI_SUCCESS;
  }
  if (param == QDMI_DEVICE_JOB_PARAMETER_PROGRAM) {
    if (value == nullptr || size == 0) {
      return QDMI_ERROR_INVALIDARGUMENT;
    }
    program_ = std::string(static_cast<const char*>(value), size - 1);
    return QDMI_SUCCESS;
  }
  if (param == QDMI_DEVICE_JOB_PARAMETER_PROGRAMFORMAT) {
    if (value == nullptr || size != sizeof(QDMI_Program_Format)) {
      return QDMI_ERROR_INVALIDARGUMENT;
    }
    const auto fmt = *static_cast<const QDMI_Program_Format*>(value);

    // Only OpenQASM 2.0 and 3.0 are currently supported
    if (fmt != QDMI_PROGRAM_FORMAT_QASM2 && fmt != QDMI_PROGRAM_FORMAT_QASM3) {
      return QDMI_ERROR_NOTSUPPORTED;
    }

    format_ = fmt;
    return QDMI_SUCCESS;
  }

  // Per-job S3 bucket configuration (required)
  if (param == QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3BUCKET) {
    if (value == nullptr || size == 0) {
      return QDMI_ERROR_INVALIDARGUMENT;
    }
    const char* bucketStr = static_cast<const char*>(value);
    if (bucketStr[size - 1] != '\0') {
      return QDMI_ERROR_INVALIDARGUMENT;
    }
    jobS3Bucket_ = bucketStr;
    return QDMI_SUCCESS;
  }

  // Per-job S3 prefix configuration (optional, defaults to timestamp)
  if (param == QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3PREFIX) {
    if (value == nullptr || size == 0) {
      return QDMI_ERROR_INVALIDARGUMENT;
    }
    const char* prefixStr = static_cast<const char*>(value);
    if (prefixStr[size - 1] != '\0') {
      return QDMI_ERROR_INVALIDARGUMENT;
    }
    jobS3Prefix_ = prefixStr;
    return QDMI_SUCCESS;
  }

  // Braket reservation ARN (optional, routes the task into a reserved window)
  if (param == QDMI_DEVICE_JOB_PARAMETER_RESERVATION_ARN) {
    if (value == nullptr || size == 0) {
      return QDMI_ERROR_INVALIDARGUMENT;
    }
    const char* arnStr = static_cast<const char*>(value);
    if (arnStr[size - 1] != '\0') {
      return QDMI_ERROR_INVALIDARGUMENT;
    }
    reservationArn_ = arnStr;
    return QDMI_SUCCESS;
  }

  return QDMI_ERROR_NOTSUPPORTED;
}

auto AMAZON_BRAKET_QDMI_Device_Job_impl_d::queryProperty(
    const QDMI_Device_Job_Property prop, const size_t size, void* value,
    size_t* sizeRet) const -> QDMI_STATUS {
  if ((value != nullptr && size == 0) || prop == QDMI_DEVICE_JOB_PROPERTY_MAX) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }

  const std::scoped_lock<std::mutex> lock(jobMutex_);
  ADD_SINGLE_VALUE_PROPERTY(QDMI_DEVICE_JOB_PROPERTY_PROGRAMFORMAT,
                            QDMI_Program_Format, format_, prop, size, value,
                            sizeRet)
  ADD_STRING_PROPERTY(QDMI_DEVICE_JOB_PROPERTY_PROGRAM, program_.c_str(), prop,
                      size, value, sizeRet)
  ADD_SINGLE_VALUE_PROPERTY(QDMI_DEVICE_JOB_PROPERTY_SHOTSNUM, size_t, shots_,
                            prop, size, value, sizeRet)

  return QDMI_ERROR_NOTSUPPORTED;
}

auto AMAZON_BRAKET_QDMI_Device_Job_impl_d::submit() -> QDMI_STATUS {
  const auto currentStatus = status_.load();
  if (currentStatus != QDMI_JOB_STATUS_CREATED) {
    return QDMI_ERROR_BADSTATE;
  }

  // Amazon Braket CreateQuantumTask API Call

  // Purpose: Submit a quantum circuit for execution on the target device
  //
  // Required Parameters:
  // - deviceArn: Target device ARN string
  // - action: OpenQASM 2.0/3.0 circuit string WRAPPED in Braket JSON schema
  // - shots: Number of circuit executions (measurements)
  // - outputS3Bucket: S3 location for storing results
  //
  // AWS SDK Usage:
  // 1. Create CreateQuantumTaskRequest
  // 2. Set device ARN, shots
  // 3. Construct Action JSON:
  //    {
  //      "braketSchemaHeader": {
  //        "name": "braket.ir.openqasm.program",
  //        "version": "1"
  //      },
  //      "source": "OPENQASM 3.0; ..."
  //    }
  // 4. Set Output S3 Bucket and Prefix
  // 5. Call BraketClient::CreateQuantumTask()

  if (program_.empty() || session_->getDeviceArn().empty()) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }

  {
    const std::scoped_lock<std::mutex> lock(jobMutex_);
    status_.store(QDMI_JOB_STATUS_QUEUED);
  }

  Aws::Braket::Model::CreateQuantumTaskRequest request;
  request.SetDeviceArn(session_->getDeviceArn());
  request.SetShots(static_cast<int64_t>(shots_));

  // Construct the Action JSON
  Aws::Utils::Json::JsonValue actionJson;
  Aws::Utils::Json::JsonValue header;
  header.WithString("name", "braket.ir.openqasm.program");
  header.WithString("version", "1");
  actionJson.WithObject("braketSchemaHeader", header);
  actionJson.WithString("source", program_);

  request.SetAction(actionJson.View().WriteCompact());

  // Configure Output S3 Bucket and Prefix
  // Bucket is required per job (no session-level fallback in AWS Braket)
  if (jobS3Bucket_.empty()) {
    std::cerr << "Error: S3 bucket must be configured per job to store task "
                 "results.\n";
    const std::scoped_lock<std::mutex> lock(jobMutex_);
    status_.store(QDMI_JOB_STATUS_FAILED);
    return QDMI_ERROR_INVALIDARGUMENT;
  }

  request.SetOutputS3Bucket(jobS3Bucket_);

  // Use provided prefix or generate timestamp-based prefix
  std::string effectivePrefix;
  if (!jobS3Prefix_.empty()) {
    effectivePrefix = jobS3Prefix_;
  } else {
    // Generate timestamp-based prefix
    const auto now = std::chrono::system_clock::now();
    const auto timestamp =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch())
            .count();
    effectivePrefix = std::to_string(timestamp);
  }
  request.SetOutputS3KeyPrefix(effectivePrefix);

  // Attach reservation ARN when provided (routes task into dedicated window)
  if (!reservationArn_.empty()) {
    Aws::Braket::Model::Association reservation;
    reservation.SetArn(reservationArn_);
    reservation.SetType(
        Aws::Braket::Model::AssociationType::RESERVATION_TIME_WINDOW_ARN);
    request.AddAssociations(reservation);
  }

  auto outcome = session_->getClient()->CreateQuantumTask(request);
  if (!outcome.IsSuccess()) {
    std::cerr << "Failed to submit task: " << outcome.GetError().GetMessage()
              << "\n";
    const std::scoped_lock<std::mutex> lock(jobMutex_);
    status_.store(QDMI_JOB_STATUS_FAILED);
    return QDMI_ERROR_NOTSUPPORTED;
  }

  taskArn_ = outcome.GetResult().GetQuantumTaskArn();
  status_.store(QDMI_JOB_STATUS_RUNNING);
  return QDMI_SUCCESS;
}

auto AMAZON_BRAKET_QDMI_Device_Job_impl_d::cancel() -> QDMI_STATUS {
  const auto currentStatus = status_.load();

  if (currentStatus == QDMI_JOB_STATUS_CREATED) {
    status_.store(QDMI_JOB_STATUS_CANCELED);
    return QDMI_SUCCESS;
  }

  if (currentStatus != QDMI_JOB_STATUS_QUEUED &&
      currentStatus != QDMI_JOB_STATUS_RUNNING) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }

  // Amazon Braket CancelQuantumTask API Call

  //
  // AWS SDK Usage:
  // 1. Create CancelQuantumTaskRequest with taskArn
  // 2. Call BraketClient::CancelQuantumTask()
  // 3. Task transitions to CANCELLING (transitional state)
  // 4. Task eventually reaches terminal state: CANCELLED, COMPLETED, or FAILED
  //
  // Important Notes:
  // - Can only cancel tasks in CREATED, QUEUED, or RUNNING state
  // - Cannot cancel COMPLETED or FAILED tasks
  // - After successful cancel request, task enters CANCELLING state
  // - Final outcome depends on race conditions:
  //   * CANCELLED: Cancellation succeeded (standard path)
  //   * COMPLETED: Task finished before cancellation took effect
  //   * FAILED: Task failed while cancellation was in flight
  // - Some devices may not support mid-execution cancellation
  //
  // After cancellation request:
  // - Call check() to poll for the final terminal status
  // - check() handles the CANCELLING→terminal state transition automatically

  if (taskArn_.empty()) {
    return QDMI_ERROR_BADSTATE;
  }

  Aws::Braket::Model::CancelQuantumTaskRequest request;
  request.SetQuantumTaskArn(taskArn_);

  auto outcome = session_->getClient()->CancelQuantumTask(request);
  if (!outcome.IsSuccess()) {
    std::cerr << "Failed to cancel task: " << outcome.GetError().GetMessage()
              << "\n";
    return QDMI_ERROR_NOTSUPPORTED;
  }

  // Cancellation request succeeded - task is now in CANCELLING state
  // Do NOT set status to CANCELED here - the actual terminal state will be
  // determined by the next check() call, which handles the CANCELLING
  // transition The final state could be CANCELLED, COMPLETED, or FAILED
  // depending on timing

  return QDMI_SUCCESS;
}

auto AMAZON_BRAKET_QDMI_Device_Job_impl_d::check(QDMI_Job_Status* status) const
    -> QDMI_STATUS {
  if (status == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }

  // Amazon Braket GetQuantumTask API Call

  // Purpose: Poll the current status of a submitted quantum task
  //
  // AWS SDK Usage:
  // 1. Create GetQuantumTaskRequest with taskArn
  // 2. Call BraketClient::GetQuantumTask()
  // 3. Extract QuantumTaskStatus from GetQuantumTaskResult
  // 4. Map AWS status to QDMI status enum
  //
  // AWS QuantumTaskStatus Enum Values:
  // - CREATED: Task created but not yet queued
  // - QUEUED: Waiting in device queue
  // - RUNNING: Actively executing on device
  // - COMPLETED: Finished successfully, results available
  // - FAILED: Execution failed (circuit error, device error, etc.)
  // - CANCELLING: Transitional state after cancel request (poll until terminal)
  // - CANCELLED: User cancelled the task
  // - NOT_SET: Uninitialized/unknown state, should not occur with IsSuccess()
  //
  // Status Mapping to QDMI:
  // AWS CREATED    → QDMI_JOB_STATUS_CREATED
  // AWS QUEUED     → QDMI_JOB_STATUS_QUEUED
  // AWS RUNNING    → QDMI_JOB_STATUS_RUNNING
  // AWS COMPLETED  → QDMI_JOB_STATUS_DONE
  // AWS FAILED     → QDMI_JOB_STATUS_FAILED
  // AWS CANCELLING → Poll until terminal state (CANCELLED/COMPLETED/FAILED)
  // AWS CANCELLED  → QDMI_JOB_STATUS_CANCELED
  // AWS NOT_SET    → QDMI_ERROR_NOTSUPPORTED
  // AWS unknown    → QDMI_ERROR_NOTSUPPORTED
  //
  if (taskArn_.empty()) {
    *status = status_.load();
    return QDMI_SUCCESS;
  }

  // If already terminal, don't poll again
  {
    const std::scoped_lock<std::mutex> lock(jobMutex_);
    const auto current = status_.load();
    if (current == QDMI_JOB_STATUS_DONE || current == QDMI_JOB_STATUS_FAILED ||
        current == QDMI_JOB_STATUS_CANCELED) {
      *status = current;
      return QDMI_SUCCESS;
    }
  }

  Aws::Braket::Model::GetQuantumTaskRequest request;
  request.SetQuantumTaskArn(taskArn_);

  // Poll until we get a status with a QDMI equivalent (handle CANCELLING)
  constexpr int maxPolls = 60;     // Maximum number of polling attempts
  constexpr int baseDelayMs = 100; // Base delay between polls (ms)
  constexpr int maxDelayMs = 2000; // Maximum delay between polls (ms)
  int pollCount = 0;
  int delayMs = baseDelayMs;

  while (pollCount < maxPolls) {
    auto outcome = session_->getClient()->GetQuantumTask(request);
    if (!outcome.IsSuccess()) {
      std::cerr << "Failed to check task: " << outcome.GetError().GetMessage()
                << "\n";
      return QDMI_ERROR_NOTSUPPORTED;
    }

    const auto& taskStatus = outcome.GetResult().GetStatus();

    // Handle CANCELLING: transitional state, poll until terminal
    if (taskStatus == Aws::Braket::Model::QuantumTaskStatus::CANCELLING) {
      pollCount++;
      if (pollCount >= maxPolls) {
        std::cerr << "WARNING: Task stuck in CANCELLING state after "
                  << maxPolls << " polls\n";
        // Return current cached status as fallback
        *status = status_.load();
        return QDMI_SUCCESS;
      }
      // Exponential backoff with cap
      std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
      delayMs = std::min(delayMs * 2, maxDelayMs);
      continue; // Poll again
    }

    // Map AWS status to QDMI status using switch for comprehensive handling
    QDMI_Job_Status newStatus = QDMI_JOB_STATUS_CREATED;

    {
      const std::scoped_lock<std::mutex> lock(jobMutex_);
      switch (taskStatus) {
      case Aws::Braket::Model::QuantumTaskStatus::CREATED:
        newStatus = QDMI_JOB_STATUS_CREATED;
        break;

      case Aws::Braket::Model::QuantumTaskStatus::QUEUED:
        newStatus = QDMI_JOB_STATUS_QUEUED;
        break;

      case Aws::Braket::Model::QuantumTaskStatus::RUNNING:
        newStatus = QDMI_JOB_STATUS_RUNNING;
        break;

      case Aws::Braket::Model::QuantumTaskStatus::COMPLETED:
        newStatus = QDMI_JOB_STATUS_DONE;
        // Store S3 location for result retrieval
        outputS3Bucket_ = outcome.GetResult().GetOutputS3Bucket();
        outputS3Directory_ = outcome.GetResult().GetOutputS3Directory();
        break;

      case Aws::Braket::Model::QuantumTaskStatus::FAILED:
        newStatus = QDMI_JOB_STATUS_FAILED;
        break;

      case Aws::Braket::Model::QuantumTaskStatus::CANCELLED:
        newStatus = QDMI_JOB_STATUS_CANCELED;
        break;

      case Aws::Braket::Model::QuantumTaskStatus::NOT_SET:
      default:
        std::cerr << "ERROR: Unknown task status (enum value: "
                  << static_cast<int>(taskStatus) << ")\n";
        return QDMI_ERROR_NOTSUPPORTED;
      }

      status_.store(newStatus);
    }

    *status = newStatus;
    return QDMI_SUCCESS;
  }

  // Should never reach here (loop always returns within iteration)
  *status = status_.load();
  return QDMI_SUCCESS;
}

auto AMAZON_BRAKET_QDMI_Device_Job_impl_d::wait(const size_t timeout) const
    -> QDMI_STATUS {
  const auto currentStatus = status_.load();
  if (currentStatus == QDMI_JOB_STATUS_CREATED) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }

  if (currentStatus == QDMI_JOB_STATUS_DONE ||
      currentStatus == QDMI_JOB_STATUS_FAILED ||
      currentStatus == QDMI_JOB_STATUS_CANCELED) {
    return QDMI_SUCCESS;
  }

  const auto startTime = std::chrono::steady_clock::now();
  QDMI_Job_Status checkedStatus = QDMI_JOB_STATUS_CREATED;

  while (true) {
    const QDMI_STATUS result = check(&checkedStatus);
    if (result != QDMI_SUCCESS) {
      return result;
    }

    if (checkedStatus == QDMI_JOB_STATUS_DONE ||
        checkedStatus == QDMI_JOB_STATUS_FAILED ||
        checkedStatus == QDMI_JOB_STATUS_CANCELED) {
      return QDMI_SUCCESS;
    }

    if (timeout > 0U) {
      const auto elapsed =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::steady_clock::now() - startTime)
              .count();
      if (std::cmp_greater_equal(elapsed, timeout)) {
        return QDMI_ERROR_TIMEOUT;
      }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

/**
 * Fetch results from S3 and parse into QDMI format.
 *
 * Downloads results.json from S3 and parses the measurements array.
 * Amazon Braket stores results in format:
 * {
 *   "measurements": [[0,0], [1,1], [0,0], ...],
 *   "measuredQubits": [0, 1],
 *   "taskMetadata": { "shots": 100, ... }
 * }
 *
 * Converts to:
 * - shotsString_: "00,11,00,..." (comma-separated bitstrings)
 * - counts_: {"00": 52, "11": 48} (histogram)
 */
auto AMAZON_BRAKET_QDMI_Device_Job_impl_d::fetchResults() const -> QDMI_STATUS {
  const std::scoped_lock<std::mutex> lock(jobMutex_);
  return fetchResultsInternal();
}

auto AMAZON_BRAKET_QDMI_Device_Job_impl_d::fetchResultsInternal() const
    -> QDMI_STATUS {
  if (resultsFetched_) {
    return QDMI_SUCCESS;
  }

  if (outputS3Bucket_.empty() || outputS3Directory_.empty()) {
    std::cerr << "S3 output location not available\n";
    return QDMI_ERROR_FATAL;
  }

  // Create S3 client with the same credentials and region as the Braket client.
  // Uses Aws::S3::S3ClientConfiguration (new SDK API) and passes explicit
  // credentials so the client works regardless of env-var credential setup.
  Aws::S3::S3ClientConfiguration s3Config;
  s3Config.region = session_->getRegion();
  Aws::S3::S3Client const s3Client(session_->getCredentials(), nullptr,
                                   s3Config);

  // Download results.json from S3
  // Key format: {outputS3Directory}/results.json
  const std::string objectKey = outputS3Directory_ + "/results.json";

  Aws::S3::Model::GetObjectRequest getRequest;
  getRequest.SetBucket(outputS3Bucket_);
  getRequest.SetKey(objectKey);

  auto outcome = s3Client.GetObject(getRequest);
  if (!outcome.IsSuccess()) {
    std::cerr << "Failed to download results from S3: "
              << outcome.GetError().GetMessage() << "\n";
    return QDMI_ERROR_FATAL;
  }

  // Read response body into string
  std::stringstream ss;
  ss << outcome.GetResult().GetBody().rdbuf();
  const std::string jsonStr = ss.str();

  // Parse JSON
  const Aws::Utils::Json::JsonValue json(jsonStr);
  if (!json.WasParseSuccessful()) {
    std::cerr << "Failed to parse results JSON\n";
    return QDMI_ERROR_FATAL;
  }

  auto root = json.View();

  // Parse measurements array: [[0,0], [1,1], [0,0], ...]
  if (!root.KeyExists("measurements")) {
    std::cerr << "No measurements in results\n";
    return QDMI_ERROR_FATAL;
  }

  auto measurements = root.GetArray("measurements");
  std::vector<std::string> shotsList;
  shotsList.reserve(measurements.GetLength());

  for (size_t i = 0; i < measurements.GetLength(); ++i) {
    auto shot = measurements[i].AsArray();
    std::string bitstring;

    // Each shot is an array of qubit values: [0, 0] or [1, 1]
    for (size_t q = 0; q < shot.GetLength(); ++q) {
      bitstring += std::to_string(shot[q].AsInteger());
    }

    shotsList.push_back(bitstring);
    counts_[bitstring]++;
  }

  // Build comma-separated shots string for QDMI_JOB_RESULT_SHOTS
  for (size_t i = 0; i < shotsList.size(); ++i) {
    if (i > 0) {
      shotsString_ += ',';
    }
    shotsString_ += shotsList[i];
  }

  resultsFetched_ = true;
  return QDMI_SUCCESS;
}

/**
 * Retrieve results from a completed QDMI job (Amazon Braket quantum task).
 *
 * Result Types:
 * - SHOTS: Comma-separated bitstrings "00,11,00,11,..."
 * - HIST_KEYS: Null-terminated unique outcomes "00\011\0"
 * - HIST_VALUES: Count for each outcome [52, 48]
 * - STATEVECTOR/PROBABILITIES: Only from simulators (not supported yet)
 */
auto AMAZON_BRAKET_QDMI_Device_Job_impl_d::getResults(
    const QDMI_Job_Result result, const size_t size, void* data,
    size_t* sizeRet) const -> QDMI_STATUS {
  if ((data != nullptr && size == 0) || result >= QDMI_JOB_RESULT_MAX) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }

  const auto currentStatus = status_.load();
  if (currentStatus != QDMI_JOB_STATUS_DONE) {
    return QDMI_ERROR_BADSTATE;
  }

  const std::scoped_lock<std::mutex> lock(jobMutex_);

  // Fetch results from S3 if not already done
  QDMI_STATUS const fetchStatus = fetchResultsInternal();
  if (fetchStatus != QDMI_SUCCESS) {
    return fetchStatus;
  }

  if (result == QDMI_JOB_RESULT_SHOTS) {
    // Return comma-separated shot results: "00,11,00,11,..."
    // Size includes null terminator
    const size_t totalSize = shotsString_.size() + 1;

    if (data != nullptr) {
      if (size < totalSize) {
        return QDMI_ERROR_INVALIDARGUMENT;
      }
      memcpy(data, shotsString_.c_str(), totalSize);
    }
    if (sizeRet != nullptr) {
      *sizeRet = totalSize;
    }
    return QDMI_SUCCESS;
  }

  if (result == QDMI_JOB_RESULT_HIST_KEYS) {
    // Return concatenated null-terminated bitstrings: "00\011\0"
    std::string keysStr;
    for (const auto& [key, count] : counts_) {
      keysStr += key;
      keysStr += '\0';
    }

    if (data != nullptr) {
      if (size < keysStr.size()) {
        return QDMI_ERROR_INVALIDARGUMENT;
      }
      memcpy(data, keysStr.data(), keysStr.size());
    }
    if (sizeRet != nullptr) {
      *sizeRet = keysStr.size();
    }
    return QDMI_SUCCESS;
  }

  if (result == QDMI_JOB_RESULT_HIST_VALUES) {
    // Return array of counts corresponding to HIST_KEYS
    std::vector<size_t> values;
    values.reserve(counts_.size());
    for (const auto& [key, count] : counts_) {
      values.push_back(count);
    }

    if (data != nullptr) {
      if (size < values.size() * sizeof(size_t)) {
        return QDMI_ERROR_INVALIDARGUMENT;
      }
      memcpy(data, values.data(), values.size() * sizeof(size_t));
    }
    if (sizeRet != nullptr) {
      *sizeRet = values.size() * sizeof(size_t);
    }
    return QDMI_SUCCESS;
  }

  if (result == QDMI_JOB_RESULT_STATEVECTOR_DENSE ||
      result == QDMI_JOB_RESULT_PROBABILITIES_DENSE ||
      result == QDMI_JOB_RESULT_STATEVECTOR_SPARSE_KEYS ||
      result == QDMI_JOB_RESULT_STATEVECTOR_SPARSE_VALUES ||
      result == QDMI_JOB_RESULT_PROBABILITIES_SPARSE_KEYS ||
      result == QDMI_JOB_RESULT_PROBABILITIES_SPARSE_VALUES) {
    // Statevector and probability results are not currently supported
    if (sizeRet != nullptr) {
      *sizeRet = 0;
    }
    return QDMI_ERROR_NOTSUPPORTED;
  }

  return QDMI_ERROR_NOTSUPPORTED;
}

// ============================================================================
// C API Implementation
// ============================================================================
// These functions provide the C interface required by QDMI specification.
// They wrap the C++ implementation classes above.

// Global SDK state - must be in same translation unit for safe init/shutdown
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
namespace {
Aws::SDKOptions gAWSOptions;
bool gAWSInitialized = false;
std::mutex gAWSInitMutex;
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)
} // namespace

/**
 * Initialize the QDMI device library.
 *
 * Must be called once at program startup before any other QDMI functions.
 * Initializes the AWS SDK and sets up the Braket device singleton.
 *
 * Thread-safe: Can be called multiple times, only first call initializes.
 *
 * @return QDMI_SUCCESS on successful initialization
 */
int AMAZON_BRAKET_QDMI_device_initialize() {
  const std::scoped_lock lock(gAWSInitMutex);
  if (!gAWSInitialized) {
    Aws::InitAPI(gAWSOptions);
    gAWSInitialized = true;
  }
  std::ignore = amazon::braket::qdmi::Device::get();
  return QDMI_SUCCESS;
}

/**
 * Finalize the QDMI device.
 *
 * Should be called once at program exit after all QDMI resources are freed.
 * Shuts down the AWS SDK and releases global resources.
 *
 * Thread-safe: Can be called multiple times, only first call shuts down.
 *
 * WARNING: After calling finalize(), no other AWS SDK calls should be made.
 * Any sessions or jobs should be freed BEFORE calling this function.
 *
 * @return QDMI_SUCCESS on successful finalization
 */
int AMAZON_BRAKET_QDMI_device_finalize() {
  const std::scoped_lock lock(gAWSInitMutex);
  if (gAWSInitialized) {
    Aws::ShutdownAPI(gAWSOptions);
    gAWSInitialized = false;
  }
  return QDMI_SUCCESS;
}

/**
 * Allocate a new device session.
 *
 * Creates a new session object that can be used to interact with an Amazon
 * Braket device. The session must be initialized with
 * AMAZON_BRAKET_QDMI_device_session_init() before use.
 *
 * @param session Pointer to receive the allocated session handle
 * @return QDMI_SUCCESS on successful allocation, error code otherwise
 */
int AMAZON_BRAKET_QDMI_device_session_alloc(
    AMAZON_BRAKET_QDMI_Device_Session* session) {
  return amazon::braket::qdmi::Device::get().sessionAlloc(session);
}

/**
 * Initialize a device session.
 *
 * Establishes connection to Amazon Braket and fetches device properties
 * including topology, supported gates, and qubit count. The session must be
 * configured with a device ARN using
 * AMAZON_BRAKET_QDMI_device_session_set_parameter() before initialization.
 *
 * @param session The session handle to initialize
 * @return QDMI_SUCCESS on successful initialization, error code otherwise
 */
int AMAZON_BRAKET_QDMI_device_session_init(
    AMAZON_BRAKET_QDMI_Device_Session session) {
  if (session == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  return session->init();
}

/**
 * Free a device session.
 *
 * Releases local resources associated with the session (memory, BraketClient).
 * All jobs created from this session should be freed before freeing the
 * session.
 *
 * AWS Braket Note: This only frees local C++ objects. AWS SDK clients and
 * their auth connections are cleaned up, but no AWS infrastructure cleanup
 * is needed (Braket is fully managed/serverless).
 *
 * @param session The session handle to free
 */
void AMAZON_BRAKET_QDMI_device_session_free(
    AMAZON_BRAKET_QDMI_Device_Session session) {
  amazon::braket::qdmi::Device::get().sessionFree(session);
}

/**
 * Set a session parameter.
 *
 * Configures session-level parameters before initialization. All parameters
 * must be set BEFORE calling AMAZON_BRAKET_QDMI_device_session_init().
 *
 * Required Parameters:
 * - QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN: Device ARN (string) **REQUIRED**
 *   Format: arn:aws:braket:<region>::device/<type>/<provider>/<device-name>
 *   Example: "arn:aws:braket:::device/quantum-simulator/amazon/<sim-name>"
 *            "arn:aws:braket:eu-north-1::device/qpu/<vendor>/<device-name>"
 *
 * AWS Authentication (one method required):
 * - QDMI_DEVICE_SESSION_PARAMETER_AUTHFILE: Path to credentials file (INI
 * format) Example: "/path/to/credentials"
 *
 * OR
 *
 * - QDMI_DEVICE_SESSION_PARAMETER_AWS_ACCESS_KEY_ID: AWS Access Key ID (string)
 * - QDMI_DEVICE_SESSION_PARAMETER_AWS_SECRET_ACCESS_KEY: AWS Secret Access Key
 * (string)
 * - QDMI_DEVICE_SESSION_PARAMETER_AWS_SESSION_TOKEN: AWS Session Token (string,
 * optional) For temporary credentials from STS or SSO
 *
 * Optional Parameters:
 * - QDMI_DEVICE_SESSION_PARAMETER_REGION: AWS region override (string)
 *   If not set, region is extracted from ARN. Example: "us-east-1",
 * "eu-north-1"
 *
 * Unsupported QDMI Authentication Parameters:
 * The following standard QDMI authentication parameters return
 * QDMI_ERROR_NOTSUPPORTED:
 * - USERNAME, PASSWORD, TOKEN, AUTHURL, BASEURL
 * Use the AWS-specific parameters above instead.
 *
 * @param session The session handle
 * @param param The parameter to set
 * @param size Size of the value in bytes (must include null terminator for
 * strings)
 * @param value Pointer to the parameter value (must be null-terminated for
 * strings)
 * @return QDMI_SUCCESS on success
 * @return QDMI_ERROR_INVALIDARGUMENT if value is NULL, size is 0, or string not
 * null-terminated
 * @return QDMI_ERROR_BADSTATE if session is already initialized
 * @return QDMI_ERROR_NOTSUPPORTED if parameter is not supported
 */
int AMAZON_BRAKET_QDMI_device_session_set_parameter(
    AMAZON_BRAKET_QDMI_Device_Session session,
    QDMI_Device_Session_Parameter param, const size_t size, const void* value) {
  if (session == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  return session->setParameter(param, size, value);
}

/**
 * Query a device property.
 *
 * Retrieves device-level properties such as qubit count, connectivity,
 * available gates, and device name. The session must be initialized before
 * querying properties.
 *
 * The first property query fetches device capabilities from Amazon Braket
 * via GetDevice(). Results are cached for subsequent queries.
 *
 * @param session The session handle
 * @param prop The property to query
 * @param size Size of the output buffer in bytes
 * @param value Pointer to output buffer (can be NULL to query size)
 * @param sizeRet Pointer to receive required size (can be NULL)
 * @return QDMI_SUCCESS on success, error code otherwise
 */
int AMAZON_BRAKET_QDMI_device_session_query_device_property(
    AMAZON_BRAKET_QDMI_Device_Session session, const QDMI_Device_Property prop,
    const size_t size, void* value, size_t* sizeRet) {
  if (session == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  return session->queryDeviceProperty(prop, size, value, sizeRet);
}

/**
 * Create a new device job.
 *
 * Allocates a new job object that can be configured with quantum circuit
 * and execution parameters before submission to Amazon Braket.
 *
 * Device availability is validated by Amazon Braket during task submission.
 *
 * @param session The session handle
 * @param job Pointer to receive the allocated job handle
 * @return QDMI_SUCCESS on successful creation, error code otherwise
 */
int AMAZON_BRAKET_QDMI_device_session_create_device_job(
    AMAZON_BRAKET_QDMI_Device_Session session,
    AMAZON_BRAKET_QDMI_Device_Job* job) {
  if (session == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  return session->createDeviceJob(job);
}

/**
 * Free a device job.
 *
 * Releases local resources associated with the job (memory, cached results).
 *
 * AWS Braket Note: This only frees local C++ objects. AWS automatically
 * releases compute resources when the quantum task completes. Task metadata
 * remains in AWS history and cannot be manually deleted.
 *
 * To stop a running task, call AMAZON_BRAKET_QDMI_device_job_cancel() before
 * freeing.
 *
 * @param job The job handle to free
 */
void AMAZON_BRAKET_QDMI_device_job_free(AMAZON_BRAKET_QDMI_Device_Job job) {
  if (job != nullptr) {
    job->getSession()->freeDeviceJob(job);
  }
}

/**
 * Set a job parameter.
 *
 * Configures job-level parameters.
 *
 * Required parameters:
 * - QDMI_DEVICE_JOB_PARAMETER_PROGRAM: OpenQASM circuit string
 * - QDMI_DEVICE_JOB_PARAMETER_PROGRAMFORMAT: Format (QASM2 or QASM3)
 * - QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3BUCKET: S3 bucket for results (string)
 *   Example: "amazon-braket-my-bucket"
 *
 * Optional parameters:
 * - QDMI_DEVICE_JOB_PARAMETER_SHOTSNUM: Number of measurement shots (default:
 * 100)
 * - QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3PREFIX: S3 prefix for results (string)
 *   Optional. If not set, uses timestamp (epoch milliseconds): "1234567890123"
 *   Example: "my-experiment/1234567890123/"
 *
 * @param job The job handle
 * @param param The parameter to set
 * @param size Size of the value in bytes
 * @param value Pointer to the parameter value
 * @return QDMI_SUCCESS on success, error code otherwise
 */
int AMAZON_BRAKET_QDMI_device_job_set_parameter(
    AMAZON_BRAKET_QDMI_Device_Job job, const QDMI_Device_Job_Parameter param,
    const size_t size, const void* value) {
  if (job == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  return job->setParameter(param, size, value);
}

/**
 * Query a job property.
 *
 * Retrieves job-level properties such as shot count, job ID, and status.
 *
 * @param job The job handle
 * @param prop The property to query
 * @param size Size of the output buffer in bytes
 * @param value Pointer to output buffer (can be NULL to query size)
 * @param sizeRet Pointer to receive required size (can be NULL)
 * @return QDMI_SUCCESS on success, error code otherwise
 */
int AMAZON_BRAKET_QDMI_device_job_query_property(
    AMAZON_BRAKET_QDMI_Device_Job job, const QDMI_Device_Job_Property prop,
    const size_t size, void* value, size_t* sizeRet) {
  if (job == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  return job->queryProperty(prop, size, value, sizeRet);
}

/**
 * Submit a QDMI job to Amazon Braket.
 *
 * Creates and submits a quantum task (single circuit execution) to Amazon
 * Braket. The QDMI job must have all required parameters set before submission.
 *
 * Note: This submits a QuantumTask, not a hybrid Job. Amazon Braket "Jobs"
 * (hybrid classical-quantum workflows) are not supported by this library.
 *
 * @param job The QDMI job handle to submit
 * @return QDMI_SUCCESS on successful submission, error code otherwise
 */
int AMAZON_BRAKET_QDMI_device_job_submit(AMAZON_BRAKET_QDMI_Device_Job job) {
  if (job == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  return job->submit();
}

/**
 * Cancel a submitted QDMI job.
 *
 * Attempts to cancel the underlying quantum task on Amazon Braket.
 * Quantum tasks that have already completed cannot be cancelled.
 *
 * @param job The QDMI job handle to cancel
 * @return QDMI_SUCCESS on successful cancellation, error code otherwise
 */
int AMAZON_BRAKET_QDMI_device_job_cancel(AMAZON_BRAKET_QDMI_Device_Job job) {
  if (job == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  return job->cancel();
}

/**
 * Check the status of a QDMI job.
 *
 * Queries Amazon Braket for the current quantum task status without blocking.
 * Status values include: CREATED, QUEUED, RUNNING, DONE, FAILED, CANCELLED.
 *
 * @param job The QDMI job handle
 * @param status Pointer to receive the current job status
 * @return QDMI_SUCCESS on successful check, error code otherwise
 */
int AMAZON_BRAKET_QDMI_device_job_check(AMAZON_BRAKET_QDMI_Device_Job job,
                                        QDMI_Job_Status* status) {
  if (job == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  return job->check(status);
}

/**
 * Wait for a QDMI job to complete.
 *
 * Blocks until the underlying quantum task completes or the timeout expires.
 * Polls the quantum task status periodically using exponential backoff.
 *
 * @param job The QDMI job handle
 * @param timeout Maximum time to wait in milliseconds (0 = infinite)
 * @return QDMI_SUCCESS when quantum task completes, QDMI_ERROR_TIMEOUT on
 * timeout
 */
int AMAZON_BRAKET_QDMI_device_job_wait(AMAZON_BRAKET_QDMI_Device_Job job,
                                       const size_t timeout) {
  if (job == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  return job->wait(timeout);
}

/**
 * Retrieve results from a completed QDMI job.
 *
 * Downloads quantum task results from S3 and returns them in the requested
 * format. Result types:
 * - QDMI_JOB_RESULT_SHOTS: Comma-separated bitstrings
 * - QDMI_JOB_RESULT_HIST_KEYS: Unique measurement outcomes
 * - QDMI_JOB_RESULT_HIST_VALUES: Counts for each outcome
 *
 * @param job The QDMI job handle
 * @param result The type of result to retrieve
 * @param size Size of the output buffer in bytes
 * @param data Pointer to output buffer (can be NULL to query size)
 * @param sizeRet Pointer to receive required size (can be NULL)
 * @return QDMI_SUCCESS on success, error code otherwise
 */
int AMAZON_BRAKET_QDMI_device_job_get_results(AMAZON_BRAKET_QDMI_Device_Job job,
                                              QDMI_Job_Result result,
                                              const size_t size, void* data,
                                              size_t* sizeRet) {
  if (job == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  return job->getResults(result, size, data, sizeRet);
}

/**
 * Query properties of a device site (qubit).
 *
 * Retrieves site-specific properties such as qubit index, connectivity,
 * and calibration data.
 *
 * @param session The session handle
 * @param site The site handle
 * @param prop The property to query
 * @param size Size of the output buffer in bytes
 * @param value Pointer to output buffer (can be NULL to query size)
 * @param sizeRet Pointer to receive required size (can be NULL)
 * @return QDMI_SUCCESS on success, error code otherwise
 */
int AMAZON_BRAKET_QDMI_device_session_query_site_property(
    AMAZON_BRAKET_QDMI_Device_Session session, AMAZON_BRAKET_QDMI_Site site,
    QDMI_Site_Property prop, const size_t size, void* value, size_t* sizeRet) {
  if (session == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  return session // NOLINT(readability-static-accessed-through-instance)
      ->querySiteProperty(site, prop, size, value, sizeRet);
}

/**
 * Query properties of a quantum operation (gate).
 *
 * Retrieves operation-specific properties such as gate name, qubit count,
 * parameter count, and fidelity for a specific instantiation on given sites.
 *
 * @param session The session handle
 * @param operation The operation handle
 * @param numSites Number of sites (qubits) the operation acts on
 * @param sites Array of site handles
 * @param numParams Number of gate parameters
 * @param params Array of gate parameter values
 * @param prop The property to query
 * @param size Size of the output buffer in bytes
 * @param value Pointer to output buffer (can be NULL to query size)
 * @param sizeRet Pointer to receive required size (can be NULL)
 * @return QDMI_SUCCESS on success, error code otherwise
 */
int AMAZON_BRAKET_QDMI_device_session_query_operation_property(
    AMAZON_BRAKET_QDMI_Device_Session session,
    AMAZON_BRAKET_QDMI_Operation operation, size_t numSites,
    const AMAZON_BRAKET_QDMI_Site* sites, size_t numParams,
    const double* params, QDMI_Operation_Property prop, size_t size,
    void* value, size_t* sizeRet) {
  if (session == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  return session // NOLINT(readability-static-accessed-through-instance)
      ->queryOperationProperty(
          operation, numSites,
          reinterpret_cast<const AMAZON_BRAKET_QDMI_Site_impl_d* const*>(sites),
          numParams, params, prop, size, value, sizeRet);
}
