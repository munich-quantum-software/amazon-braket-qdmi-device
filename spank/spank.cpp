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

/**
 * @file spank.cpp
 * @brief Slurm SPANK integration for Amazon Braket QDMI.
 *
 * The plugin validates one requested device per remote job step. Incomplete
 * opt-in fails in the allocator; AWS/device failures are enforced at task
 * launch so they fail the job without draining the node.
 */

#include "amazon-braket-qdmi-device/constants.hpp"
#include "amazon_braket_qdmi/device.h"

#include <array>
#include <cstdint>
#include <mutex>
#include <slurm/spank.h>
#include <string>
#include <syslog.h>

SPANK_PLUGIN(amazon_braket_qdmi, 1);

namespace {

enum class OptionValue : std::uint8_t {
  DeviceArn = 1,
  Region,
  ReservationArn,
  AuthFile,
};

constexpr auto optionIndex(const OptionValue option) -> size_t {
  return static_cast<size_t>(option) -
         static_cast<size_t>(OptionValue::DeviceArn);
}

struct ValidationState {
  bool active = false;
  bool complete = false;
  bool successful = false;
};

std::mutex optionsMutex;
std::array<std::string, 4> optionValues;
std::mutex validationMutex;
ValidationState validationState;

spank_option options[] = {
    {const_cast<char*>("qdmi-device-session-parameter-baseurl"),
     const_cast<char*>("ARN"), const_cast<char*>("Amazon Braket device ARN"), 1,
     static_cast<int>(OptionValue::DeviceArn), nullptr},
    {const_cast<char*>("qdmi-device-session-parameter-region"),
     const_cast<char*>("REGION"), const_cast<char*>("AWS region"), 1,
     static_cast<int>(OptionValue::Region), nullptr},
    {const_cast<char*>("qdmi-device-session-parameter-reservation-arn"),
     const_cast<char*>("ARN"),
     const_cast<char*>("Amazon Braket reservation ARN"), 1,
     static_cast<int>(OptionValue::ReservationArn), nullptr},
    {const_cast<char*>("qdmi-device-session-parameter-authfile"),
     const_cast<char*>("PATH"), const_cast<char*>("AWS credentials file path"),
     1, static_cast<int>(OptionValue::AuthFile), nullptr},
};

auto validOptionValue(const char* value) -> bool {
  if (value == nullptr || value[0] == '\0') {
    return false;
  }
  for (const char* current = value; *current != '\0'; ++current) {
    if (*current == '\n' || *current == '\r') {
      return false;
    }
  }
  return true;
}

auto optionCallback(const int value, const char* argument, const int remote)
    -> int {
  (void)remote;
  // Reject empty or multiline values before they are forwarded by Slurm.
  if (value < static_cast<int>(OptionValue::DeviceArn) ||
      value > static_cast<int>(OptionValue::AuthFile) ||
      !validOptionValue(argument)) {
    return 1;
  }

  const std::scoped_lock lock(optionsMutex);
  optionValues[static_cast<size_t>(value) -
               static_cast<size_t>(OptionValue::DeviceArn)] = argument;
  return 0;
}

auto snapshotOptions() -> std::array<std::string, 4> {
  const std::scoped_lock lock(optionsMutex);
  return optionValues;
}

auto collectRemoteOptions(spank_t spank) -> std::array<std::string, 4> {
  auto values = snapshotOptions();
  for (auto& option : options) {
    char* argument = nullptr;
    if (spank_option_getopt(spank, &option, &argument) == ESPANK_SUCCESS &&
        validOptionValue(argument)) {
      values[optionIndex(static_cast<OptionValue>(option.val))] = argument;
    }
  }
  return values;
}

auto setValidationState(const bool successful) -> void {
  const std::scoped_lock lock(validationMutex);
  validationState = {
      .active = true, .complete = true, .successful = successful};
}

auto getValidationState() -> ValidationState {
  const std::scoped_lock lock(validationMutex);
  return validationState;
}

auto logFailure(const char* message) -> void {
  slurm_spank_log("amazon-braket-qdmi: %s", message);
}

auto validateRequiredOptions(const std::array<std::string, 4>& values) -> bool {
  const bool hasBaseUrl = !values[optionIndex(OptionValue::DeviceArn)].empty();
  const bool hasAuthFile = !values[optionIndex(OptionValue::AuthFile)].empty();

  // Neither option means inactive; providing only one is invalid.
  if (!hasBaseUrl && !hasAuthFile) {
    return true;
  }
  if (hasBaseUrl != hasAuthFile) {
    logFailure("--qdmi-device-session-parameter-baseurl and "
               "--qdmi-device-session-parameter-authfile are required");
    return false;
  }
  return true;
}

auto validateDevice(const std::array<std::string, 4>& values) -> bool {
  if (values[optionIndex(OptionValue::DeviceArn)].empty() ||
      values[optionIndex(OptionValue::AuthFile)].empty()) {
    logFailure("device ARN and credentials file are required");
    return false;
  }

  if (AMAZON_BRAKET_QDMI_device_initialize() != QDMI_SUCCESS) {
    logFailure("failed to initialize QDMI");
    return false;
  }

  AMAZON_BRAKET_QDMI_Device_Session session = nullptr;
  bool successful = false;
  do {
    if (AMAZON_BRAKET_QDMI_device_session_alloc(&session) != QDMI_SUCCESS) {
      logFailure("failed to allocate QDMI session");
      break;
    }

    const auto setString = [&](const QDMI_Device_Session_Parameter parameter,
                               const std::string& value) -> bool {
      return AMAZON_BRAKET_QDMI_device_session_set_parameter(
                 session, parameter, value.size() + 1, value.c_str()) ==
             QDMI_SUCCESS;
    };

    if (!setString(QDMI_DEVICE_SESSION_PARAMETER_BASEURL,
                   values[optionIndex(OptionValue::DeviceArn)]) ||
        !setString(QDMI_DEVICE_SESSION_PARAMETER_AUTHFILE,
                   values[optionIndex(OptionValue::AuthFile)])) {
      logFailure("invalid QDMI session parameters");
      break;
    }
    if (!values[optionIndex(OptionValue::Region)].empty() &&
        !setString(QDMI_DEVICE_SESSION_PARAMETER_REGION,
                   values[optionIndex(OptionValue::Region)])) {
      logFailure("invalid AWS region");
      break;
    }
    if (!values[optionIndex(OptionValue::ReservationArn)].empty() &&
        !setString(QDMI_DEVICE_SESSION_PARAMETER_RESERVATION_ARN,
                   values[optionIndex(OptionValue::ReservationArn)])) {
      logFailure("invalid reservation ARN");
      break;
    }

    if (AMAZON_BRAKET_QDMI_device_session_init(session) != QDMI_SUCCESS) {
      logFailure("could not initialize the Amazon Braket session");
      break;
    }

    QDMI_Device_Status status = QDMI_DEVICE_STATUS_OFFLINE;
    if (AMAZON_BRAKET_QDMI_device_session_query_device_property(
            session, QDMI_DEVICE_PROPERTY_STATUS, sizeof(status), &status,
            nullptr) != QDMI_SUCCESS) {
      logFailure("could not query Amazon Braket device status");
      break;
    }

    if (status != QDMI_DEVICE_STATUS_IDLE &&
        status != QDMI_DEVICE_STATUS_BUSY) {
      logFailure("Amazon Braket device is not available");
      break;
    }
    successful = true;
  } while (false);

  AMAZON_BRAKET_QDMI_device_session_free(session);
  AMAZON_BRAKET_QDMI_device_finalize();
  return successful;
}

auto injectEnvironment(spank_t spank, const std::array<std::string, 4>& values)
    -> bool {
  const auto setEnvironment = [&](const char* name,
                                  const std::string& value) -> bool {
    return spank_setenv(spank, name, value.c_str(), 1) == ESPANK_SUCCESS;
  };

  if (!setEnvironment(AMAZON_BRAKET_QDMI_DEVICE_ENV_BASEURL,
                      values[optionIndex(OptionValue::DeviceArn)]) ||
      !setEnvironment(AMAZON_BRAKET_QDMI_DEVICE_ENV_AUTHFILE,
                      values[optionIndex(OptionValue::AuthFile)])) {
    return false;
  }
  if (!values[optionIndex(OptionValue::Region)].empty() &&
      !setEnvironment(AMAZON_BRAKET_QDMI_DEVICE_ENV_REGION,
                      values[optionIndex(OptionValue::Region)])) {
    return false;
  }
  if (!values[optionIndex(OptionValue::ReservationArn)].empty() &&
      !setEnvironment(AMAZON_BRAKET_QDMI_DEVICE_ENV_RESERVATION_ARN,
                      values[optionIndex(OptionValue::ReservationArn)])) {
    return false;
  }
  return true;
}

} // namespace

int slurm_spank_init(spank_t spank, int /*ac*/, char* /*argv*/[]) {
  {
    const std::scoped_lock lock(optionsMutex);
    optionValues = {};
  }
  for (auto& option : options) {
    option.cb = optionCallback;
    if (spank_option_register(spank, &option) != ESPANK_SUCCESS) {
      return 1;
    }
  }
  if (spank_remote(spank)) {
    const std::scoped_lock lock(validationMutex);
    validationState = {};
  }
  return 0;
}

int slurm_spank_init_post_opt(spank_t spank, int /*ac*/, char* /*argv*/[]) {
  const auto values = snapshotOptions();

  // Check the opt-in pair before sbatch schedules the job.
  if (spank_context() == S_CTX_ALLOCATOR) {
    return validateRequiredOptions(values) ? 0 : 1;
  }

  if (!spank_remote(spank)) {
    return 0;
  }

  if (values[optionIndex(OptionValue::DeviceArn)].empty() &&
      values[optionIndex(OptionValue::AuthFile)].empty()) {
    const std::scoped_lock lock(validationMutex);
    validationState = {.active = false, .complete = true, .successful = true};
    return 0;
  }
  if (!validateRequiredOptions(values)) {
    setValidationState(false);
  }
  return 0;
}

int slurm_spank_user_init(spank_t spank, int /*ac*/, char* /*argv*/[]) {
  if (!spank_remote(spank)) {
    return 0;
  }

  // Run AWS/device checks once, after privileges are dropped.
  std::unique_lock validationLock(validationMutex);
  if (validationState.complete) {
    return 0;
  }

  // Read options forwarded to the remote context.
  const auto values = collectRemoteOptions(spank);
  const bool successful = validateDevice(values);
  // spank_setenv propagates values into the task environment.
  if (successful && !injectEnvironment(spank, values)) {
    logFailure("failed to inject QDMI environment variables");
    validationState = {.active = true, .complete = true, .successful = false};
    return 0;
  }
  validationState = {
      .active = true, .complete = true, .successful = successful};
  return 0;
}

int slurm_spank_task_init(spank_t spank, int /*ac*/, char* /*argv*/[]) {
  if (!spank_remote(spank)) {
    return 0;
  }
  // Enforce remote failures just before execve(); fail the job, not the node.
  const auto state = getValidationState();
  if (!state.active) {
    return 0;
  }
  if (!state.complete || !state.successful) {
    logFailure("job rejected because QDMI validation failed");
    return 1;
  }
  return 0;
}
