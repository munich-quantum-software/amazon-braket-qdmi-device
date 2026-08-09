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

#include "amazon-braket-qdmi-device/DeviceParser.hpp"
#include "amazon-braket-qdmi-device/Queue.hpp"
#include "amazon-braket-qdmi-device/Wait.hpp"
#include "amazon-braket-qdmi-device/constants.hpp"
#include "amazon_braket_qdmi/device.h"

#include <algorithm>
#include <array>
#include <aws/braket/BraketClient.h>
#include <aws/braket/BraketErrors.h>
#include <aws/braket/model/Association.h>
#include <aws/braket/model/AssociationType.h>
#include <aws/braket/model/CancelQuantumTaskRequest.h>
#include <aws/braket/model/CreateQuantumTaskRequest.h>
#include <aws/braket/model/DeviceStatus.h>
#include <aws/braket/model/DeviceType.h>
#include <aws/braket/model/GetDeviceRequest.h>
#include <aws/braket/model/GetDeviceResult.h>
#include <aws/braket/model/GetQuantumTaskRequest.h>
#include <aws/braket/model/GetQuantumTaskResult.h>
#include <aws/braket/model/QuantumTaskAdditionalAttributeName.h>
#include <aws/braket/model/QuantumTaskStatus.h>
#include <aws/core/Aws.h>
#include <aws/core/auth/AWSCredentials.h>
#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/core/auth/AWSCredentialsProviderChain.h>
#include <aws/core/client/ClientConfiguration.h>
#include <aws/core/utils/Array.h>
#include <aws/core/utils/json/JsonSerializer.h>
#include <aws/core/utils/memory/stl/AWSAllocator.h>
#include <aws/core/utils/memory/stl/AWSString.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/S3ClientConfiguration.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
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
  }

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

// Assigns a null-terminated string parameter.
// Validates that value contains a null byte within the given size, then
// assigns. Use as the body of a case label or an if block.
#define SET_STRING(size, value, member)                                        \
  {                                                                            \
    const auto* str_ = static_cast<const char*>((value));                      \
    if (memchr(str_, '\0', (size)) == nullptr) {                               \
      return QDMI_ERROR_INVALIDARGUMENT;                                       \
    }                                                                          \
    (member) = str_;                                                           \
    return QDMI_SUCCESS;                                                       \
  }

#define SET_STRING_IF(param_name, param, size, value, member)                  \
  if ((param) == (param_name))                                                 \
  SET_STRING(size, value, member)
// NOLINTEND(bugprone-macro-parentheses)

namespace {

constexpr std::array<QDMI_Program_Format, 2> SUPPORTED_PROGRAM_FORMATS = {
    QDMI_PROGRAM_FORMAT_QASM2, QDMI_PROGRAM_FORMAT_QASM3};

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
    // Trim leading whitespace
    line.erase(0, line.find_first_not_of(" \t\r\n"));
    // Trim trailing whitespace
    {
      const auto last = line.find_last_not_of(" \t\r\n");
      line.erase(last == std::string::npos ? 0 : last + 1);
    }
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
                  << "Using first profile [" << firstProfile << "]\n";
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
        {
          const auto last = key.find_last_not_of(" \t");
          key.erase(last == std::string::npos ? 0 : last + 1);
        }
        value.erase(0, value.find_first_not_of(" \t"));
        {
          const auto last = value.find_last_not_of(" \t");
          value.erase(last == std::string::npos ? 0 : last + 1);
        }

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

struct UtcTimeOfWeek {
  int dayOfWeek = 0; // 0 = Sunday, 1 = Monday, ..., 6 = Saturday
  int secondOfDay = 0;
};

/**
 * @brief Threshold for considering an ONLINE device as BUSY.
 *
 * If the total number of queued quantum tasks across all queue entries
 * is >= this value, the device is reported as QDMI_DEVICE_STATUS_BUSY.
 * Below this value (within an execution window) the device is reported as
 * QDMI_DEVICE_STATUS_IDLE.
 */
constexpr int QUEUE_BUSY_THRESHOLD = 5;

} // anonymous namespace

namespace {

/**
 * @brief Compute total queue length from a Braket GetDevice result.
 *
 * Sums the `queueSize` field across all entries in `deviceQueueInfo`.
 * AWS returns queue sizes as strings (e.g. "12" or ">50"). A lower bound is
 * represented by its numeric component. If any value cannot be interpreted
 * reliably, no queue length is returned.
 *
 * @param result Result of a successful BraketClient::GetDevice() call
 * @return Total queue length (>= 0)
 */
auto getTotalQueueLength(const Aws::Braket::Model::GetDeviceResult& result)
    -> std::optional<size_t> {
  size_t totalLength = 0;
  for (const auto& queueItem : result.GetDeviceQueueInfo()) {
    const auto length =
        amazon::braket::qdmi::detail::parseQueueValue(queueItem.GetQueueSize());
    if (!length.has_value() ||
        *length > std::numeric_limits<size_t>::max() - totalLength) {
      return std::nullopt;
    }
    totalLength += *length;
  }
  return totalLength;
}

auto parseExecutionWindowTime(const std::string& timeStr, int& secondsOfDay)
    -> bool {
  if (timeStr.size() != 5 && timeStr.size() != 8) {
    return false;
  }
  if (timeStr[2] != ':' || (timeStr.size() == 8 && timeStr[5] != ':')) {
    return false;
  }

  try {
    const int hours = std::stoi(timeStr.substr(0, 2));
    const int minutes = std::stoi(timeStr.substr(3, 2));
    const int seconds =
        timeStr.size() == 8 ? std::stoi(timeStr.substr(6, 2)) : 0;
    if (hours < 0 || hours > 23 || minutes < 0 || minutes > 59 || seconds < 0 ||
        seconds > 59) {
      return false;
    }
    secondsOfDay = (hours * 60 * 60) + (minutes * 60) + seconds;
  } catch (...) {
    return false;
  }
  return true;
}

auto executionDayMatches(const std::string& executionDay, const int dayOfWeek)
    -> bool {
  if (executionDay == "Everyday") {
    return true;
  }
  if (executionDay == "Weekdays") {
    return dayOfWeek >= 1 && dayOfWeek <= 5;
  }
  if (executionDay == "Weekend") {
    return dayOfWeek == 0 || dayOfWeek == 6;
  }
  static const std::array<const char*, 7> DAY_NAMES = {
      "Sunday",   "Monday", "Tuesday", "Wednesday",
      "Thursday", "Friday", "Saturday"};
  if (dayOfWeek < 0) {
    return false;
  }
  const auto dayIndex = static_cast<size_t>(dayOfWeek);
  return dayIndex < DAY_NAMES.size() && executionDay == DAY_NAMES[dayIndex];
}

auto getCurrentUtcTimeOfWeek() -> UtcTimeOfWeek {
  const auto now = std::chrono::system_clock::now();
  const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
  std::tm utcTime{};
#ifdef _WIN32
  gmtime_s(&utcTime, &nowTime);
#else
  gmtime_r(&nowTime, &utcTime);
#endif
  return {.dayOfWeek = utcTime.tm_wday,
          .secondOfDay = (utcTime.tm_hour * 60 * 60) + (utcTime.tm_min * 60) +
                         utcTime.tm_sec};
}

auto isTimeInExecutionWindow(const std::string& executionDay,
                             const int windowStart, const int windowEnd,
                             const UtcTimeOfWeek& time) -> bool {
  if (windowStart == windowEnd) {
    // Braket execution windows with equal start/end times are interpreted
    // as an all-day window for the matching executionDay
    return executionDayMatches(executionDay, time.dayOfWeek);
  }

  if (windowStart < windowEnd) {
    return executionDayMatches(executionDay, time.dayOfWeek) &&
           time.secondOfDay >= windowStart && time.secondOfDay <= windowEnd;
  }

  const int previousDayOfWeek = (time.dayOfWeek + 6) % 7;
  return (executionDayMatches(executionDay, time.dayOfWeek) &&
          time.secondOfDay >= windowStart) ||
         (executionDayMatches(executionDay, previousDayOfWeek) &&
          time.secondOfDay <= windowEnd);
}

auto getCurrentExecutionWindowAvailability(
    const Aws::String& deviceCapabilities) -> std::optional<bool> {
  Aws::Utils::Json::JsonValue const json(deviceCapabilities);
  if (!json.WasParseSuccessful()) {
    return std::nullopt;
  }

  const auto propertiesJson = json.View();
  if (!propertiesJson.ValueExists("service")) {
    return std::nullopt;
  }

  const auto service = propertiesJson.GetObject("service");
  if (!service.ValueExists("executionWindows")) {
    return std::nullopt;
  }

  const auto executionWindows = service.GetArray("executionWindows");
  if (executionWindows.GetLength() == 0) {
    return std::nullopt;
  }

  const auto now = getCurrentUtcTimeOfWeek();
  bool anyWindowParsed = false;
  for (size_t i = 0; i < executionWindows.GetLength(); ++i) {
    const auto window = executionWindows[i].AsObject();
    if (!window.ValueExists("executionDay") ||
        !window.ValueExists("windowStartHour") ||
        !window.ValueExists("windowEndHour")) {
      continue;
    }

    int windowStart = 0;
    int windowEnd = 0;
    if (!parseExecutionWindowTime(window.GetString("windowStartHour"),
                                  windowStart) ||
        !parseExecutionWindowTime(window.GetString("windowEndHour"),
                                  windowEnd)) {
      continue;
    }
    anyWindowParsed = true;
    if (isTimeInExecutionWindow(window.GetString("executionDay"), windowStart,
                                windowEnd, now)) {
      return true;
    }
  }
  return anyWindowParsed ? std::optional<bool>{false} : std::nullopt;
}

auto getQDMIStatusForBraketDevice(
    const Aws::Braket::Model::DeviceStatus braketStatus,
    const std::optional<bool>& executionWindowAvailability,
    const size_t queueLength) -> std::optional<QDMI_Device_Status> {
  switch (braketStatus) {
  case Aws::Braket::Model::DeviceStatus::ONLINE: {
    const bool isOutsideExecutionWindow =
        executionWindowAvailability.has_value() &&
        !*executionWindowAvailability;
    if (isOutsideExecutionWindow || queueLength >= QUEUE_BUSY_THRESHOLD) {
      return QDMI_DEVICE_STATUS_BUSY;
    }
    return QDMI_DEVICE_STATUS_IDLE;
  }
  case Aws::Braket::Model::DeviceStatus::OFFLINE:
    return QDMI_DEVICE_STATUS_MAINTENANCE;
  case Aws::Braket::Model::DeviceStatus::RETIRED:
    return QDMI_DEVICE_STATUS_OFFLINE;
  case Aws::Braket::Model::DeviceStatus::NOT_SET:
  default:
    return std::nullopt;
  }
}

auto computeQDMIStatusFromDevice(
    const Aws::Braket::Model::GetDeviceResult& device,
    const bool hasReservationArn, const std::optional<size_t> queueLength)
    -> std::optional<QDMI_Device_Status> {
  std::optional<bool> executionWindowAvailability;
  if (!hasReservationArn) {
    executionWindowAvailability =
        getCurrentExecutionWindowAvailability(device.GetDeviceCapabilities());
  }
  return getQDMIStatusForBraketDevice(
      device.GetDeviceStatus(), executionWindowAvailability,
      queueLength.value_or(QUEUE_BUSY_THRESHOLD));
}

auto isPermissionError(const Aws::Braket::BraketErrors error) -> bool {
  switch (error) {
  case Aws::Braket::BraketErrors::ACCESS_DENIED:
  case Aws::Braket::BraketErrors::INVALID_ACCESS_KEY_ID:
  case Aws::Braket::BraketErrors::INVALID_CLIENT_TOKEN_ID:
  case Aws::Braket::BraketErrors::INVALID_SIGNATURE:
  case Aws::Braket::BraketErrors::MISSING_AUTHENTICATION_TOKEN:
  case Aws::Braket::BraketErrors::SIGNATURE_DOES_NOT_MATCH:
  case Aws::Braket::BraketErrors::UNRECOGNIZED_CLIENT:
    return true;
  default:
    return false;
  }
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
      sessions_.emplace(uniqueSession.get(), std::move(uniqueSession)).first;
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

auto Device::clear() -> void {
  {
    const std::scoped_lock<std::mutex> lock(sessionsMutex_);
    sessions_.clear();
  }
  {
    const std::scoped_lock<std::mutex> lock(deviceCacheMutex_);
    deviceCache_.clear();
  }
}

auto Device::queryProperty(const QDMI_Device_Property prop, const size_t size,
                           void* value, size_t* sizeRet) -> QDMI_STATUS {
  // Validate arguments and reject MAX sentinel value
  if ((value != nullptr && size == 0) || prop == QDMI_DEVICE_PROPERTY_MAX) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  ADD_STRING_PROPERTY(QDMI_DEVICE_PROPERTY_NAME, "Amazon Braket QDMI Device",
                      prop, size, value, sizeRet)
  ADD_STRING_PROPERTY(QDMI_DEVICE_PROPERTY_VERSION,
                      AMAZON_BRAKET_QDMI_DEVICE_VERSION, prop, size, value,
                      sizeRet)
  // Braket device properties (library-level, not session device-specific)
  ADD_STRING_PROPERTY(QDMI_DEVICE_PROPERTY_LIBRARYVERSION, QDMI_VERSION, prop,
                      size, value, sizeRet)
  // There is no notion of calibration in this context.
  ADD_SINGLE_VALUE_PROPERTY(QDMI_DEVICE_PROPERTY_NEEDSCALIBRATION, size_t, 0,
                            prop, size, value, sizeRet)

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
 * Caching strategy:
 * - Simulators: full architecture is cached globally.
 *   Cache hits only re-fetch the mutable device status.
 * - QPUs: only name/provider/deviceType are cached globally. Sites,
 *   operations, and connectivity data may change between calibration cycles
 *   and are re-fetched from the API on every query.
 *
 * @return QDMI_SUCCESS on successful fetch, error code otherwise
 */
auto AMAZON_BRAKET_QDMI_Device_Session_impl_d::fetchDeviceArchitecture() const
    -> QDMI_STATUS {
  if (deviceArn_.empty() || client_ == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }

  // Check if architecture is already cached
  // lock the shared_ptr when queryDeviceProperty() is called from multiple
  // threads
  std::shared_ptr<amazon::braket::qdmi::DeviceArchitecture> localArch;
  {
    const std::scoped_lock lock(cachedArchitectureMutex_);
    cachedArchitecture_ =
        amazon::braket::qdmi::Device::get().getCachedArchitecture(deviceArn_);
    localArch = cachedArchitecture_;
  }

  if (localArch != nullptr &&
      localArch->deviceType != Aws::Braket::Model::DeviceType::QPU) {
    // Simulator cache hit: only re-fetch the mutable device status.
    // QPUs fall through to a full re-fetch every time because their sites,
    // operations, and connectivity may change between calibration cycles.
    Aws::Braket::Model::GetDeviceRequest request;
    request.SetDeviceArn(deviceArn_.c_str());
    auto outcome = client_->GetDevice(request);
    if (!outcome.IsSuccess()) {
      std::cerr << "Failed to get device: " << outcome.GetError().GetMessage()
                << "\n";
      std::cerr
          << "Ensure AWS credentials are available through the SDK default "
             "provider chain or supplied explicitly via AUTHFILE or complete "
             "USERNAME/PASSWORD parameters (with optional TOKEN).\n";
      return QDMI_ERROR_NOTSUPPORTED;
    }
    const auto& device = outcome.GetResult();
    const auto queueLength = getTotalQueueLength(device);
    const auto qdmiStatus = computeQDMIStatusFromDevice(
        device, !reservationArn_.empty(), queueLength);
    if (!qdmiStatus.has_value()) {
      const auto braketStatus = device.GetDeviceStatus();
      std::cerr << "ERROR: Unknown device status (enum value: "
                << static_cast<int>(braketStatus) << ")\n";
      return QDMI_ERROR_NOTSUPPORTED;
    }
    if (*qdmiStatus == QDMI_DEVICE_STATUS_OFFLINE) {
      std::cerr << "ERROR: Device " << device.GetDeviceName()
                << " is RETIRED and permanently unavailable.\n";
      std::cerr << "Please update your device ARN to a newer generation.\n";
      return QDMI_ERROR_NOTSUPPORTED;
    }
    braketDeviceStatus_.store(*qdmiStatus);
    {
      const std::scoped_lock lock(cachedArchitectureMutex_);
      queueLength_ = queueLength;
    }
    return QDMI_SUCCESS;
  }

  // Cache miss (any device type) or QPU cache hit: request full device
  // architecture. QPUs always re-fetch because calibration data is mutable.
  Aws::Braket::Model::GetDeviceRequest request;
  request.SetDeviceArn(deviceArn_.c_str());

  auto outcome = client_->GetDevice(request);
  if (!outcome.IsSuccess()) {
    std::cerr << "Failed to get device: " << outcome.GetError().GetMessage()
              << "\n";
    std::cerr << "Ensure AWS credentials are available through the SDK default "
                 "provider chain or supplied explicitly via AUTHFILE or "
                 "complete USERNAME/PASSWORD parameters (with optional "
                 "TOKEN).\n";
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

  const auto queueLength = getTotalQueueLength(device);
  const auto qdmiStatus = computeQDMIStatusFromDevice(
      device, !reservationArn_.empty(), queueLength);
  if (!qdmiStatus.has_value()) {
    std::cerr << "ERROR: Unknown device status (enum value: "
              << static_cast<int>(braketStatus) << ")\n";
    return QDMI_ERROR_NOTSUPPORTED;
  }
  braketDeviceStatus_.store(*qdmiStatus);

  if (braketStatus == Aws::Braket::Model::DeviceStatus::RETIRED) {
    // Device is permanently decommissioned
    std::cerr << "ERROR: Device " << device.GetDeviceName()
              << " is RETIRED and permanently unavailable.\n";
    std::cerr << "Please update your device ARN to a newer generation.\n";
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
  std::unique_ptr<IDeviceParser> parser;
  if (architecture->deviceType == Aws::Braket::Model::DeviceType::SIMULATOR) {
    parser = std::make_unique<SimulatorPropertiesParser>();
  } else if (architecture->deviceType == Aws::Braket::Model::DeviceType::QPU) {
    if (architecture->provider == "IQM") {
      parser = std::make_unique<IQMDeviceParser>();
    }
  }
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
  auto status = parser->ParseProperties(propertiesStr, properties);
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

  // Store in global singleton cache and assign to session.
  // QPUs: only a stub {name, provider, deviceType} goes into the global cache
  // so that future calls know the device type and provider (for parser
  // selection) without caching the mutable sites, operations, and connectivity.
  // Simulators: the full architecture is cached (sites and operations are
  // static).
  {
    const std::scoped_lock lock(cachedArchitectureMutex_);
    if (architecture->deviceType == Aws::Braket::Model::DeviceType::QPU) {
      auto stub = std::make_shared<amazon::braket::qdmi::DeviceArchitecture>();
      stub->name = architecture->name;
      stub->provider = architecture->provider;
      stub->deviceType = architecture->deviceType;
      amazon::braket::qdmi::Device::get().setCachedArchitecture(deviceArn_,
                                                                stub);
    } else {
      amazon::braket::qdmi::Device::get().setCachedArchitecture(deviceArn_,
                                                                architecture);
    }
    cachedArchitecture_ = architecture;
    queueLength_ = queueLength;
  }
  return QDMI_SUCCESS;
}

/**
 * Initialize a device session.
 *
 * Session initialization involves:
 * 1. Validating the required device ARN and any explicit credentials
 * 2. Setting up the AWS SDK client with proper region configuration
 * 3. Transitioning the session to INITIALIZED state
 *
 * Configuration:
 * AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN
 * - Credentials: Optional. Explicit credentials can be set via one of:
 *   - QDMI_DEVICE_SESSION_PARAMETER_AUTHFILE (credentials file path)
 *   - QDMI_DEVICE_SESSION_PARAMETER_AWS_ACCESS_KEY_ID + AWS_SECRET_ACCESS_KEY +
 * QDMI_DEVICE_SESSION_PARAMETER_AWS_SESSION_TOKEN (optional). Without explicit
 * credentials, the AWS SDK default credential provider chain is used.
 *  - AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_REGION (defaults to the value
 *    from the ARN or us-east-1)
 *  - AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_RESERVATION_ARN (optional
 *    session default; skips public execution-window status checks and is
 *    inherited by jobs)
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

  // Slurm/SPANK can provide session metadata through the environment. These
  // fallbacks are deliberately applied only when the application did not set
  // the corresponding API parameter, so explicit application configuration
  // takes precedence.
  const auto applyEnvironmentFallback = [](std::string& destination,
                                           const char* variable) {
    if (destination.empty()) {
      if (const char* value = std::getenv(variable);
          value != nullptr && value[0] != '\0') {
        destination = value;
      }
    }
  };
  applyEnvironmentFallback(deviceArn_,
                           AMAZON_BRAKET_QDMI_DEVICE_ENV_DEVICE_ARN);
  applyEnvironmentFallback(region_, AMAZON_BRAKET_QDMI_DEVICE_ENV_REGION);
  applyEnvironmentFallback(reservationArn_,
                           AMAZON_BRAKET_QDMI_DEVICE_ENV_RESERVATION_ARN);
  if (credentialsFile_.empty() && accessKeyId_.empty() &&
      secretAccessKey_.empty() && sessionToken_.empty()) {
    applyEnvironmentFallback(credentialsFile_,
                             AMAZON_BRAKET_QDMI_DEVICE_ENV_AUTHFILE);
  }

  // Check that required parameters are set
  if (deviceArn_.empty()) {
    std::cerr << "ERROR: Device ARN not configured. Set via:\n";
    std::cerr << "  AMAZON_BRAKET_QDMI_device_session_set_parameter() with "
                 "AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN\n";
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

  const bool hasAccessKey = !accessKeyId_.empty();
  const bool hasSecretKey = !secretAccessKey_.empty();
  const bool hasSessionToken = !sessionToken_.empty();
  if (credentialsFile_.empty() &&
      (hasAccessKey != hasSecretKey ||
       (hasSessionToken && !(hasAccessKey && hasSecretKey)))) {
    std::cerr << "ERROR: Incomplete direct AWS credentials.\n";
    std::cerr << "Provide both QDMI_DEVICE_SESSION_PARAMETER_USERNAME and "
                 "QDMI_DEVICE_SESSION_PARAMETER_PASSWORD; "
                 "QDMI_DEVICE_SESSION_PARAMETER_TOKEN is only valid with a "
                 "complete access/secret key pair.\n";
    std::cerr << "To use the AWS SDK default credential provider chain, do not "
                 "set any direct credential parameters.\n";
    return QDMI_ERROR_INVALIDARGUMENT;
  }

  if (hasAccessKey && hasSecretKey) {
    const Aws::Auth::AWSCredentials credentials(
        accessKeyId_, secretAccessKey_,
        sessionToken_.empty() ? "" : sessionToken_);
    credentialsProvider_ =
        Aws::MakeShared<Aws::Auth::SimpleAWSCredentialsProvider>(
            "AmazonBraketQDMICredentialsProvider", credentials);
  } else {
    credentialsProvider_ =
        Aws::MakeShared<Aws::Auth::DefaultAWSCredentialsProviderChain>(
            "AmazonBraketQDMICredentialsProvider",
            config.ResolveCredentialProviderConfig());
  }
  client_ =
      std::make_unique<Aws::Braket::BraketClient>(credentialsProvider_, config);

  initialized_ = true;
  return QDMI_SUCCESS;
}

auto AMAZON_BRAKET_QDMI_Device_Session_impl_d::setParameter(
    const QDMI_Device_Session_Parameter param, const size_t size,
    const void* value) -> QDMI_STATUS {
  // Check for invalid arguments (value must never be null for a setter)
  if (value == nullptr || size == 0) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }

  // Validate parameter: must be standard QDMI param or one of the specifically
  // defined custom params (REGION, RESERVATION_ARN)
  const bool isStandardParam = param < QDMI_DEVICE_SESSION_PARAMETER_MAX;
  const bool isDefinedCustomParam =
      (param == AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_REGION ||
       param == AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_RESERVATION_ARN);

  if (!isStandardParam && !isDefinedCustomParam) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }

  // Parameters can only be set before initialization
  if (initialized_) {
    return QDMI_ERROR_BADSTATE;
  }

  // Handle Amazon Braket parameters
  switch (param) {
  case AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN: // Device ARN
    SET_STRING(size, value, deviceArn_)
  case AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_REGION: // AWS Region
    SET_STRING(size, value, region_)
  case AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_RESERVATION_ARN:
    SET_STRING(size, value, reservationArn_)
  // Method 1: AWS Credentials File (AUTHFILE)
  // Supports standard AWS credentials file format with profiles
  case QDMI_DEVICE_SESSION_PARAMETER_AUTHFILE:
    SET_STRING(size, value, credentialsFile_)
  // Method 2: Direct Credential Parameters
  // For programmatic credential specification
  case QDMI_DEVICE_SESSION_PARAMETER_USERNAME: // AWS_ACCESS_KEY_ID
    SET_STRING(size, value, accessKeyId_)
  case QDMI_DEVICE_SESSION_PARAMETER_PASSWORD: // AWS_SECRET_ACCESS_KEY
    SET_STRING(size, value, secretAccessKey_)
  case QDMI_DEVICE_SESSION_PARAMETER_TOKEN: // AWS_SESSION_TOKEN
    SET_STRING(size, value, sessionToken_)
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

auto AMAZON_BRAKET_QDMI_Device_Session_impl_d::openDeviceJob(
    const char* jobId, AMAZON_BRAKET_QDMI_Device_Job* job) -> QDMI_STATUS {
  if (jobId == nullptr || *jobId == '\0' || job == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  if (!initialized_) {
    return QDMI_ERROR_BADSTATE;
  }

  Aws::Braket::Model::GetQuantumTaskRequest request;
  request.SetQuantumTaskArn(jobId);
  const auto outcome = client_->GetQuantumTask(request);
  if (!outcome.IsSuccess()) {
    const auto error = outcome.GetError().GetErrorType();
    switch (error) {
    case Aws::Braket::BraketErrors::RESOURCE_NOT_FOUND:
    case Aws::Braket::BraketErrors::VALIDATION:
      return QDMI_ERROR_NOTFOUND;
    default:
      if (isPermissionError(error)) {
        return QDMI_ERROR_PERMISSIONDENIED;
      }
      std::cerr << "Failed to open task: " << outcome.GetError().GetMessage()
                << "\n";
      return QDMI_ERROR_FATAL;
    }
  }

  const auto& task = outcome.GetResult();
  if (task.GetDeviceArn() != deviceArn_) {
    return QDMI_ERROR_NOTFOUND;
  }
  if (!std::in_range<size_t>(task.GetShots())) {
    return QDMI_ERROR_FATAL;
  }

  auto uniqueJob = std::make_unique<AMAZON_BRAKET_QDMI_Device_Job_impl_d>(this);
  uniqueJob->retrieved_ = true;
  uniqueJob->taskArn_ = jobId;
  uniqueJob->shots_ = static_cast<size_t>(task.GetShots());
  uniqueJob->status_.store(QDMI_JOB_STATUS_SUBMITTED);
  QDMI_Job_Status status = QDMI_JOB_STATUS_SUBMITTED;
  if (const auto result = uniqueJob->updateFromTask(task, &status);
      result != QDMI_SUCCESS) {
    return QDMI_ERROR_FATAL;
  }

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

  // fetchDeviceArchitecture() uses the internal cache
  if (const auto ret = fetchDeviceArchitecture(); ret != QDMI_SUCCESS) {
    return ret;
  }

  // Snapshot the shared_ptr under the lock
  std::shared_ptr<amazon::braket::qdmi::DeviceArchitecture> arch;
  std::optional<size_t> queueLength;
  {
    const std::scoped_lock lock(cachedArchitectureMutex_);
    arch = cachedArchitecture_;
    queueLength = queueLength_;
  }

  // Session device architecture properties (from cache)
  ADD_LIST_PROPERTY(QDMI_DEVICE_PROPERTY_SITES, AMAZON_BRAKET_QDMI_Site,
                    arch->sitesPtr, prop, size, value, sizeRet)
  ADD_LIST_PROPERTY(QDMI_DEVICE_PROPERTY_OPERATIONS,
                    AMAZON_BRAKET_QDMI_Operation, arch->operationsPtr, prop,
                    size, value, sizeRet)
  ADD_LIST_PROPERTY(QDMI_DEVICE_PROPERTY_COUPLINGMAP, AMAZON_BRAKET_QDMI_Site,
                    arch->connectivity, prop, size, value, sizeRet)
  ADD_SINGLE_VALUE_PROPERTY(QDMI_DEVICE_PROPERTY_QUBITSNUM, size_t,
                            arch->qubitsNum, prop, size, value, sizeRet)
  ADD_STRING_PROPERTY(QDMI_DEVICE_PROPERTY_NAME, arch->name.c_str(), prop, size,
                      value, sizeRet)

  // Return device status from Amazon Braket (mutable, per-query)
  ADD_SINGLE_VALUE_PROPERTY(QDMI_DEVICE_PROPERTY_STATUS, QDMI_Device_Status,
                            braketDeviceStatus_.load(), prop, size, value,
                            sizeRet)
  if (prop == QDMI_DEVICE_PROPERTY_QUEUELENGTH) {
    if (!queueLength.has_value()) {
      return QDMI_ERROR_NOTSUPPORTED;
    }
    ADD_SINGLE_VALUE_PROPERTY(QDMI_DEVICE_PROPERTY_QUEUELENGTH, size_t,
                              *queueLength, prop, size, value, sizeRet)
  }
  ADD_LIST_PROPERTY(QDMI_DEVICE_PROPERTY_SUPPORTEDPROGRAMFORMATS,
                    QDMI_Program_Format, SUPPORTED_PROGRAM_FORMATS, prop, size,
                    value, sizeRet)

  // Delegate to Braket device singleton for library-level properties only
  // (LIBRARYVERSION, NEEDSCALIBRATION)
  return amazon::braket::qdmi::Device::queryProperty(prop, size, value,
                                                     sizeRet);
}

auto AMAZON_BRAKET_QDMI_Device_Session_impl_d::querySiteProperty(
    AMAZON_BRAKET_QDMI_Site_impl_d* site, const QDMI_Site_Property prop,
    const size_t size, void* value, size_t* sizeRet) const -> QDMI_STATUS {
  if (!initialized_) {
    return QDMI_ERROR_BADSTATE;
  }
  if (site == nullptr || (value != nullptr && size == 0) ||
      prop == QDMI_SITE_PROPERTY_MAX) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }

  std::shared_ptr<amazon::braket::qdmi::DeviceArchitecture> architecture;
  {
    const std::scoped_lock lock(cachedArchitectureMutex_);
    architecture = cachedArchitecture_;
  }
  if (architecture == nullptr ||
      std::ranges::find(architecture->sitesPtr, site) ==
          architecture->sitesPtr.end()) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }

  ADD_SINGLE_VALUE_PROPERTY(QDMI_SITE_PROPERTY_INDEX, uint64_t, site->id_, prop,
                            size, value, sizeRet)
  ADD_STRING_PROPERTY(QDMI_SITE_PROPERTY_NAME, site->name_.c_str(), prop, size,
                      value, sizeRet)
  if (site->t1_ > 0.0) {
    ADD_SINGLE_VALUE_PROPERTY(QDMI_SITE_PROPERTY_T1, double, site->t1_, prop,
                              size, value, sizeRet)
  }
  if (site->t2_ > 0.0) {
    ADD_SINGLE_VALUE_PROPERTY(QDMI_SITE_PROPERTY_T2, double, site->t2_, prop,
                              size, value, sizeRet)
  }

  return QDMI_ERROR_NOTSUPPORTED;
}

auto AMAZON_BRAKET_QDMI_Device_Session_impl_d::queryOperationProperty(
    AMAZON_BRAKET_QDMI_Operation_impl_d* operation, const size_t numSites,
    const AMAZON_BRAKET_QDMI_Site_impl_d* const* sites, const size_t numParams,
    const double* params, const QDMI_Operation_Property prop, const size_t size,
    void* value, size_t* sizeRet) const -> QDMI_STATUS {
  if (!initialized_) {
    return QDMI_ERROR_BADSTATE;
  }
  if (operation == nullptr || (value != nullptr && size == 0) ||
      (sites != nullptr && numSites == 0) ||
      (params != nullptr && numParams == 0) ||
      prop == QDMI_OPERATION_PROPERTY_MAX) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }

  std::shared_ptr<amazon::braket::qdmi::DeviceArchitecture> architecture;
  {
    const std::scoped_lock lock(cachedArchitectureMutex_);
    architecture = cachedArchitecture_;
  }
  if (architecture == nullptr ||
      std::ranges::find(architecture->operationsPtr, operation) ==
          architecture->operationsPtr.end() ||
      (sites != nullptr &&
       std::ranges::any_of(
           std::span{sites, numSites}, [&architecture](const auto* site) {
             return std::ranges::find(architecture->sitesPtr, site) ==
                    architecture->sitesPtr.end();
           }))) {
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
  // defined custom params (OUTPUTS3BUCKET, OUTPUTS3PREFIX, RESERVATION_ARN)
  const bool isStandardParam = param < QDMI_DEVICE_JOB_PARAMETER_MAX;
  const bool isDefinedCustomParam =
      (param == AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3BUCKET ||
       param == AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3PREFIX ||
       param == AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_RESERVATION_ARN);

  if ((value != nullptr && size == 0) ||
      (!isStandardParam && !isDefinedCustomParam)) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }

  const std::scoped_lock<std::mutex> lock(jobMutex_);
  if (retrieved_) {
    return QDMI_ERROR_BADSTATE;
  }
  if (status_.load() != QDMI_JOB_STATUS_CREATED) {
    return QDMI_ERROR_BADSTATE;
  }
  if (param == QDMI_DEVICE_JOB_PARAMETER_SHOTSNUM) {
    if (value == nullptr || size != sizeof(size_t)) {
      return QDMI_ERROR_INVALIDARGUMENT;
    }
    shots_ = *static_cast<const size_t*>(value);
    return QDMI_SUCCESS;
  }
  SET_STRING_IF(QDMI_DEVICE_JOB_PARAMETER_PROGRAM, param, size, value, program_)
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
  SET_STRING_IF(AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3BUCKET, param,
                size, value, jobS3Bucket_)
  // Per-job S3 prefix configuration (optional, defaults to timestamp)
  SET_STRING_IF(AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3PREFIX, param,
                size, value, jobS3Prefix_)
  // Braket reservation ARN (optional, routes the task into a reserved window)
  SET_STRING_IF(AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_RESERVATION_ARN, param,
                size, value, reservationArn_)

  return QDMI_ERROR_NOTSUPPORTED;
}

auto AMAZON_BRAKET_QDMI_Device_Job_impl_d::queryProperty(
    const QDMI_Device_Job_Property prop, const size_t size, void* value,
    size_t* sizeRet) const -> QDMI_STATUS {
  if ((value != nullptr && size == 0) || prop == QDMI_DEVICE_JOB_PROPERTY_MAX) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }

  if (prop == QDMI_DEVICE_JOB_PROPERTY_QUEUEPOSITION) {
    QDMI_Job_Status refreshedStatus = QDMI_JOB_STATUS_CREATED;
    if (const auto result = check(&refreshedStatus); result != QDMI_SUCCESS) {
      return result;
    }
    if (refreshedStatus != QDMI_JOB_STATUS_QUEUED) {
      return QDMI_ERROR_BADSTATE;
    }

    std::optional<size_t> queuePosition;
    {
      const std::scoped_lock<std::mutex> lock(jobMutex_);
      queuePosition = queuePosition_;
    }
    if (!queuePosition.has_value()) {
      return QDMI_ERROR_NOTSUPPORTED;
    }
    ADD_SINGLE_VALUE_PROPERTY(QDMI_DEVICE_JOB_PROPERTY_QUEUEPOSITION, size_t,
                              *queuePosition, prop, size, value, sizeRet)
  }

  const std::scoped_lock<std::mutex> lock(jobMutex_);
  if (!taskArn_.empty()) {
    ADD_STRING_PROPERTY(QDMI_DEVICE_JOB_PROPERTY_ID, taskArn_.c_str(), prop,
                        size, value, sizeRet)
  }
  if (retrieved_) {
    ADD_SINGLE_VALUE_PROPERTY(QDMI_DEVICE_JOB_PROPERTY_SHOTSNUM, size_t, shots_,
                              prop, size, value, sizeRet)
    return QDMI_ERROR_NOTSUPPORTED;
  }
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

  // Capture all shared fields under jobMutex_ to prevent data races with
  // concurrent setParameter() calls
  std::string localProgram;
  std::string localS3Bucket;
  std::string localS3Prefix;
  std::string localReservationArn;
  size_t localShots = 0;
  {
    const std::scoped_lock<std::mutex> lock(jobMutex_);
    if (retrieved_) {
      return QDMI_ERROR_BADSTATE;
    }
    if (status_.load() != QDMI_JOB_STATUS_CREATED) {
      return QDMI_ERROR_BADSTATE;
    }
    if (program_.empty() || session_->getDeviceArn().empty()) {
      return QDMI_ERROR_INVALIDARGUMENT;
    }
    status_.store(QDMI_JOB_STATUS_QUEUED);
    localProgram = program_;
    localS3Bucket = jobS3Bucket_;
    localS3Prefix = jobS3Prefix_;
    localReservationArn = reservationArn_.empty()
                              ? session_->getReservationArn()
                              : reservationArn_;
    localShots = shots_;
  }

  Aws::Braket::Model::CreateQuantumTaskRequest request;
  request.SetDeviceArn(session_->getDeviceArn());
  request.SetShots(static_cast<int64_t>(localShots));

  // Construct the Action JSON
  Aws::Utils::Json::JsonValue actionJson;
  Aws::Utils::Json::JsonValue header;
  header.WithString("name", "braket.ir.openqasm.program");
  header.WithString("version", "1");
  actionJson.WithObject("braketSchemaHeader", header);
  actionJson.WithString("source", localProgram);

  request.SetAction(actionJson.View().WriteCompact());

  // Configure Output S3 Bucket and Prefix
  // Bucket is required per job (no session-level fallback in AWS Braket)
  if (localS3Bucket.empty()) {
    std::cerr << "Error: S3 bucket must be configured per job to store task "
                 "results.\n";
    const std::scoped_lock<std::mutex> lock(jobMutex_);
    status_.store(QDMI_JOB_STATUS_FAILED);
    return QDMI_ERROR_INVALIDARGUMENT;
  }

  request.SetOutputS3Bucket(localS3Bucket);

  // Use provided prefix or generate timestamp-based prefix
  std::string effectivePrefix;
  if (!localS3Prefix.empty()) {
    effectivePrefix = localS3Prefix;
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

  // Attach reservation ARN when provided (routes task into a reserved window).
  // A job-level reservation overrides the session default.
  if (!localReservationArn.empty()) {
    Aws::Braket::Model::Association reservation;
    reservation.SetArn(localReservationArn);
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

  {
    const std::scoped_lock<std::mutex> lock(jobMutex_);
    taskArn_ = outcome.GetResult().GetQuantumTaskArn();
  }
  status_.store(QDMI_JOB_STATUS_SUBMITTED);
  return QDMI_SUCCESS;
}

auto AMAZON_BRAKET_QDMI_Device_Job_impl_d::cancel() -> QDMI_STATUS {
  const auto currentStatus = status_.load();

  if (currentStatus == QDMI_JOB_STATUS_CREATED) {
    status_.store(QDMI_JOB_STATUS_CANCELED);
    return QDMI_SUCCESS;
  }

  if (currentStatus != QDMI_JOB_STATUS_SUBMITTED &&
      currentStatus != QDMI_JOB_STATUS_QUEUED &&
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
  // - AWS accepts cancellation while a task is CREATED, QUEUED, or RUNNING
  // - Remote AWS CREATED maps to QDMI SUBMITTED because the task is no longer
  //   locally configurable
  // - Cannot cancel COMPLETED or FAILED tasks
  // - After successful cancel request, task enters CANCELLING state
  // - Final outcome depends on race conditions:
  //   * CANCELLED: Cancellation succeeded (standard path)
  //   * COMPLETED: Task finished before cancellation took effect
  //   * FAILED: Task failed while cancellation was in flight
  // - Some devices may not support mid-execution cancellation
  //
  // After cancellation request:
  // - Call check() repeatedly; it returns the last known status while
  //   CANCELLING and updates to the terminal state once AWS resolves it

  std::string localTaskArn;
  {
    const std::scoped_lock<std::mutex> lock(jobMutex_);
    localTaskArn = taskArn_;
  }
  if (localTaskArn.empty()) {
    return QDMI_ERROR_BADSTATE;
  }

  Aws::Braket::Model::CancelQuantumTaskRequest request;
  request.SetQuantumTaskArn(localTaskArn);

  auto outcome = session_->getClient()->CancelQuantumTask(request);
  if (!outcome.IsSuccess()) {
    std::cerr << "Failed to cancel task: " << outcome.GetError().GetMessage()
              << "\n";
    return isPermissionError(outcome.GetError().GetErrorType())
               ? QDMI_ERROR_PERMISSIONDENIED
               : QDMI_ERROR_FATAL;
  }

  // Cancellation request succeeded - task is now in CANCELLING state.
  // Do NOT set status to CANCELED here; check() will report the last known
  // status while CANCELLING and update to the terminal state once resolved.

  return QDMI_SUCCESS;
}

auto AMAZON_BRAKET_QDMI_Device_Job_impl_d::updateFromTask(
    const Aws::Braket::Model::GetQuantumTaskResult& task,
    QDMI_Job_Status* status) const -> QDMI_STATUS {
  const std::scoped_lock<std::mutex> lock(jobMutex_);
  queuePosition_.reset();
  switch (task.GetStatus()) {
  case Aws::Braket::Model::QuantumTaskStatus::CREATED:
    *status = QDMI_JOB_STATUS_SUBMITTED;
    break;
  case Aws::Braket::Model::QuantumTaskStatus::QUEUED:
    *status = QDMI_JOB_STATUS_QUEUED;
    if (const auto& queueInfo = task.GetQueueInfo();
        queueInfo.PositionHasBeenSet()) {
      queuePosition_ = amazon::braket::qdmi::detail::parseQueueValue(
          queueInfo.GetPosition());
    }
    break;
  case Aws::Braket::Model::QuantumTaskStatus::RUNNING:
    *status = QDMI_JOB_STATUS_RUNNING;
    break;
  case Aws::Braket::Model::QuantumTaskStatus::COMPLETED:
    *status = QDMI_JOB_STATUS_DONE;
    outputS3Bucket_ = task.GetOutputS3Bucket();
    outputS3Directory_ = task.GetOutputS3Directory();
    break;
  case Aws::Braket::Model::QuantumTaskStatus::FAILED:
    *status = QDMI_JOB_STATUS_FAILED;
    break;
  case Aws::Braket::Model::QuantumTaskStatus::CANCELLED:
    *status = QDMI_JOB_STATUS_CANCELED;
    break;
  case Aws::Braket::Model::QuantumTaskStatus::CANCELLING:
    *status = status_.load();
    return QDMI_SUCCESS;
  case Aws::Braket::Model::QuantumTaskStatus::NOT_SET:
  default:
    return QDMI_ERROR_FATAL;
  }
  status_.store(*status);
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
  // - CANCELLING: Transitional state after cancel request (returns last known
  // status)
  // - CANCELLED: User cancelled the task
  // - NOT_SET: Uninitialized/unknown state, should not occur with IsSuccess()
  //
  // Status Mapping to QDMI:
  // AWS CREATED    → QDMI_JOB_STATUS_SUBMITTED
  // AWS QUEUED     → QDMI_JOB_STATUS_QUEUED
  // AWS RUNNING    → QDMI_JOB_STATUS_RUNNING
  // AWS COMPLETED  → QDMI_JOB_STATUS_DONE
  // AWS FAILED     → QDMI_JOB_STATUS_FAILED
  // AWS CANCELLING → last known status
  // AWS CANCELLED  → QDMI_JOB_STATUS_CANCELED
  // AWS NOT_SET    → QDMI_ERROR_FATAL
  // AWS unknown    → QDMI_ERROR_FATAL
  //
  std::string localTaskArn;
  {
    const std::scoped_lock<std::mutex> lock(jobMutex_);
    localTaskArn = taskArn_;
  }
  if (localTaskArn.empty()) {
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
  request.SetQuantumTaskArn(localTaskArn);
  request.AddAdditionalAttributeNames(
      Aws::Braket::Model::QuantumTaskAdditionalAttributeName::QueueInfo);

  auto outcome = session_->getClient()->GetQuantumTask(request);
  if (!outcome.IsSuccess()) {
    std::cerr << "Failed to check task: " << outcome.GetError().GetMessage()
              << "\n";
    return isPermissionError(outcome.GetError().GetErrorType())
               ? QDMI_ERROR_PERMISSIONDENIED
               : QDMI_ERROR_FATAL;
  }

  return updateFromTask(outcome.GetResult(), status);
}

auto AMAZON_BRAKET_QDMI_Device_Job_impl_d::wait(const size_t timeout) const
    -> QDMI_STATUS {
  struct WaitContext {
    const AMAZON_BRAKET_QDMI_Device_Job_impl_d* job;
  } context{this};
  const amazon::braket::qdmi::detail::JobWaitFunctions functions{
      .context = &context,
      .checkStatus =
          [](void* rawContext, QDMI_Job_Status* status) {
            const auto* waitContext =
                static_cast<const WaitContext*>(rawContext);
            return waitContext->job->check(status);
          },
      .now = [](void*) { return std::chrono::steady_clock::now(); },
      .sleepFor =
          [](void*, const std::chrono::steady_clock::duration duration) {
            std::this_thread::sleep_for(duration);
          }};
  return wait(timeout, functions);
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

  // Create the S3 client with the same refreshable credentials provider and
  // region as the Braket client.
  Aws::S3::S3ClientConfiguration s3Config;
  s3Config.region = session_->getRegion();
  Aws::S3::S3Client const s3Client(session_->getCredentialsProvider(), nullptr,
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
 * - HIST_KEYS: Comma-separated unique outcomes "00,11"
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
    // Return a null-terminated, comma-separated list matching QDMI's string
    // representation and the order of HIST_VALUES.
    std::string keysStr;
    for (const auto& [key, count] : counts_) {
      if (!keysStr.empty()) {
        keysStr += ',';
      }
      keysStr += key;
    }
    const size_t totalSize = keysStr.size() + 1;

    if (data != nullptr) {
      if (size < totalSize) {
        return QDMI_ERROR_INVALIDARGUMENT;
      }
      memcpy(data, keysStr.c_str(), totalSize);
    }
    if (sizeRet != nullptr) {
      *sizeRet = totalSize;
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
    amazon::braket::qdmi::Device::get().clear();
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
 * Establishes connection to Amazon Braket and fetches device properties.
 * The session must be configured with a device ARN using
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
 * - AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN: Device ARN (string)
 *   **REQUIRED**
 *   Format: arn:aws:braket:<region>::device/<type>/<provider>/<device-name>
 *   Example: "arn:aws:braket:::device/quantum-simulator/amazon/<sim-name>"
 *            "arn:aws:braket:eu-north-1::device/qpu/<vendor>/<device-name>"
 *
 * AWS Authentication (optional explicit configuration):
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
 * If no explicit credential parameters are set, the AWS SDK default credential
 * provider chain is used.
 *
 * Optional Parameters:
 * - AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_REGION: AWS region override
 *   (string)
 *   If not set, region is extracted from ARN. Example: "us-east-1",
 * "eu-north-1"
 *
 * Unsupported QDMI Authentication Parameters:
 * - QDMI_DEVICE_SESSION_PARAMETER_AUTHURL
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

int AMAZON_BRAKET_QDMI_device_session_retrieve_device_job_by_id(
    AMAZON_BRAKET_QDMI_Device_Session session, const char* jobId,
    AMAZON_BRAKET_QDMI_Device_Job* job) {
  if (session == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  return session->openDeviceJob(jobId, job);
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
 * - AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3BUCKET: S3 bucket for
 *   results (string)
 *   Example: "amazon-braket-my-bucket"
 *
 * Optional parameters:
 * - QDMI_DEVICE_JOB_PARAMETER_SHOTSNUM: Number of measurement shots (default:
 * 100)
 * - AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3PREFIX: S3 prefix for
 *   results (string)
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
 * Queries Amazon Braket for the current quantum task status.
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
 * @param timeout Maximum time to wait in seconds (0 = infinite)
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
