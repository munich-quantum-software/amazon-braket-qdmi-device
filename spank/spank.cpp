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
#include <mutex>
#include <slurm/spank.h>
#include <string>
#include <syslog.h>

SPANK_PLUGIN(amazon_braket_qdmi, 1);

namespace {

enum OptionValue : int {
  OPTION_DEVICE_ARN = 1,
  OPTION_REGION,
  OPTION_RESERVATION_ARN,
  OPTION_AUTHFILE,
};

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
     OPTION_DEVICE_ARN, nullptr},
    {const_cast<char*>("qdmi-device-session-parameter-region"),
     const_cast<char*>("REGION"), const_cast<char*>("AWS region"), 1,
     OPTION_REGION, nullptr},
    {const_cast<char*>("qdmi-device-session-parameter-reservation-arn"),
     const_cast<char*>("ARN"),
     const_cast<char*>("Amazon Braket reservation ARN"), 1,
     OPTION_RESERVATION_ARN, nullptr},
    {const_cast<char*>("qdmi-device-session-parameter-authfile"),
     const_cast<char*>("PATH"), const_cast<char*>("AWS credentials file path"),
     1, OPTION_AUTHFILE, nullptr},
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
  if (value < OPTION_DEVICE_ARN || value > OPTION_AUTHFILE ||
      !validOptionValue(argument)) {
    return 1;
  }

  const std::scoped_lock lock(optionsMutex);
  optionValues[static_cast<size_t>(value - OPTION_DEVICE_ARN)] = argument;
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
      values[static_cast<size_t>(option.val - OPTION_DEVICE_ARN)] = argument;
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

auto logFailure(spank_t spank, const char* message) -> void {
  slurm_spank_log(spank, LOG_ERR, "amazon-braket-qdmi: %s", message);
}

auto validateRequiredOptions(spank_t spank,
                             const std::array<std::string, 4>& values) -> bool {
  const bool hasBaseUrl =
      !values[OPTION_DEVICE_ARN - OPTION_DEVICE_ARN].empty();
  const bool hasAuthFile = !values[OPTION_AUTHFILE - OPTION_DEVICE_ARN].empty();

  // Neither option means inactive; providing only one is invalid.
  if (!hasBaseUrl && !hasAuthFile) {
    return true;
  }
  if (hasBaseUrl != hasAuthFile) {
    logFailure(spank, "--qdmi-device-session-parameter-baseurl and "
                      "--qdmi-device-session-parameter-authfile are required");
    return false;
  }
  return true;
}

auto validateDevice(spank_t spank, const std::array<std::string, 4>& values)
    -> bool {
  if (values[OPTION_DEVICE_ARN - OPTION_DEVICE_ARN].empty() ||
      values[OPTION_AUTHFILE - OPTION_DEVICE_ARN].empty()) {
    logFailure(spank, "device ARN and credentials file are required");
    return false;
  }

  if (AMAZON_BRAKET_QDMI_device_initialize() != QDMI_SUCCESS) {
    logFailure(spank, "failed to initialize QDMI");
    return false;
  }

  AMAZON_BRAKET_QDMI_Device_Session session = nullptr;
  bool successful = false;
  do {
    if (AMAZON_BRAKET_QDMI_device_session_alloc(&session) != QDMI_SUCCESS) {
      logFailure(spank, "failed to allocate QDMI session");
      break;
    }

    const auto setString = [&](const QDMI_Device_Session_Parameter parameter,
                               const std::string& value) -> bool {
      return AMAZON_BRAKET_QDMI_device_session_set_parameter(
                 session, parameter, value.size() + 1, value.c_str()) ==
             QDMI_SUCCESS;
    };

    if (!setString(QDMI_DEVICE_SESSION_PARAMETER_BASEURL,
                   values[OPTION_DEVICE_ARN - OPTION_DEVICE_ARN]) ||
        !setString(QDMI_DEVICE_SESSION_PARAMETER_AUTHFILE,
                   values[OPTION_AUTHFILE - OPTION_DEVICE_ARN])) {
      logFailure(spank, "invalid QDMI session parameters");
      break;
    }
    if (!values[OPTION_REGION - OPTION_DEVICE_ARN].empty() &&
        !setString(QDMI_DEVICE_SESSION_PARAMETER_REGION,
                   values[OPTION_REGION - OPTION_DEVICE_ARN])) {
      logFailure(spank, "invalid AWS region");
      break;
    }
    if (!values[OPTION_RESERVATION_ARN - OPTION_DEVICE_ARN].empty() &&
        !setString(QDMI_DEVICE_SESSION_PARAMETER_RESERVATION_ARN,
                   values[OPTION_RESERVATION_ARN - OPTION_DEVICE_ARN])) {
      logFailure(spank, "invalid reservation ARN");
      break;
    }

    if (AMAZON_BRAKET_QDMI_device_session_init(session) != QDMI_SUCCESS) {
      logFailure(spank, "could not initialize the Amazon Braket session");
      break;
    }

    QDMI_Device_Status status = QDMI_DEVICE_STATUS_OFFLINE;
    if (AMAZON_BRAKET_QDMI_device_session_query_device_property(
            session, QDMI_DEVICE_PROPERTY_STATUS, sizeof(status), &status,
            nullptr) != QDMI_SUCCESS) {
      logFailure(spank, "could not query Amazon Braket device status");
      break;
    }

    if (status != QDMI_DEVICE_STATUS_IDLE &&
        status != QDMI_DEVICE_STATUS_BUSY) {
      logFailure(spank, "Amazon Braket device is not available");
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
                      values[OPTION_DEVICE_ARN - OPTION_DEVICE_ARN]) ||
      !setEnvironment(AMAZON_BRAKET_QDMI_DEVICE_ENV_AUTHFILE,
                      values[OPTION_AUTHFILE - OPTION_DEVICE_ARN])) {
    return false;
  }
  if (!values[OPTION_REGION - OPTION_DEVICE_ARN].empty() &&
      !setEnvironment(AMAZON_BRAKET_QDMI_DEVICE_ENV_REGION,
                      values[OPTION_REGION - OPTION_DEVICE_ARN])) {
    return false;
  }
  if (!values[OPTION_RESERVATION_ARN - OPTION_DEVICE_ARN].empty() &&
      !setEnvironment(AMAZON_BRAKET_QDMI_DEVICE_ENV_RESERVATION_ARN,
                      values[OPTION_RESERVATION_ARN - OPTION_DEVICE_ARN])) {
    return false;
  }
  return true;
}

} // namespace

extern "C" {

int slurm_spank_init(spank_t spank, int /*ac*/, char* /*argv*/[]) {
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
    return validateRequiredOptions(spank, values) ? 0 : 1;
  }

  if (!spank_remote(spank)) {
    return 0;
  }

  if (values[OPTION_DEVICE_ARN - OPTION_DEVICE_ARN].empty() &&
      values[OPTION_AUTHFILE - OPTION_DEVICE_ARN].empty()) {
    const std::scoped_lock lock(validationMutex);
    validationState = {.active = false, .complete = true, .successful = true};
    return 0;
  }
  if (!validateRequiredOptions(spank, values)) {
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
  const bool successful = validateDevice(spank, values);
  // spank_setenv propagates values into the task environment.
  if (successful && !injectEnvironment(spank, values)) {
    logFailure(spank, "failed to inject QDMI environment variables");
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
    logFailure(spank, "job rejected because QDMI validation failed");
    return 1;
  }
  return 0;
}

} // extern "C"
