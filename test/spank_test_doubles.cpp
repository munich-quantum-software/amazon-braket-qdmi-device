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

#include "spank_test_doubles.hpp"

#include "amazon-braket-qdmi-device/constants.hpp"
#include "amazon_braket_qdmi/device.h"

#include <cstdarg>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <slurm/slurm_errno.h>
#include <slurm/spank.h>
#include <string>
#include <utility>

namespace {
auto sessionStorage() -> char* {
  static char storage;
  return &storage;
}
} // namespace

namespace spank_test {

auto state() -> State& {
  static State testState;
  return testState;
}

auto reset() -> void { state() = {}; }

auto registeredOption(const std::string& name) -> spank_option* {
  for (auto* option : state().registeredOptions) {
    if (name == option->name) {
      return option;
    }
  }
  return nullptr;
}

auto configureOptIn() -> void {
  registeredOption("qdmi-device-session-parameter-baseurl")
      ->cb(registeredOption("qdmi-device-session-parameter-baseurl")->val,
           "arn:aws:braket:::device/quantum-simulator/amazon/sv1", 0);
  registeredOption("qdmi-device-session-parameter-authfile")
      ->cb(registeredOption("qdmi-device-session-parameter-authfile")->val,
           "/tmp/credentials", 0);
}

auto beginRemote() -> spank_t {
  state().remote = true;
  state().context = S_CTX_REMOTE;
  return reinterpret_cast<spank_t>(&state());
}

auto beginAllocator() -> spank_t {
  state().remote = false;
  state().context = S_CTX_ALLOCATOR;
  return reinterpret_cast<spank_t>(&state());
}

} // namespace spank_test

extern "C" {

int spank_remote(spank_t /*spank*/) {
  return static_cast<int>(spank_test::state().remote);
}

spank_context_t spank_context(void) { return spank_test::state().context; }

int spank_option_register(spank_t /*spank*/, spank_option* option) {
  spank_test::state().registeredOptions.push_back(option);
  return ESPANK_SUCCESS;
}

int spank_option_getopt(spank_t /*spank*/, spank_option* option,
                        char** argument) {
  auto found = spank_test::state().forwardedOptions.find(option->name);
  if (found == spank_test::state().forwardedOptions.end()) {
    return ESPANK_ERROR;
  }
  *argument = found->second.data();
  return ESPANK_SUCCESS;
}

int spank_getenv(spank_t /*spank*/, const char* name, char* buffer,
                 const int length) {
  const auto found = spank_test::state().jobEnvironment.find(name);
  if (found == spank_test::state().jobEnvironment.end() || length <= 0 ||
      static_cast<size_t>(length) <= found->second.size()) {
    return ESPANK_ERROR;
  }
  std::memcpy(buffer, found->second.c_str(), found->second.size() + 1);
  return ESPANK_SUCCESS;
}

// This test double must mirror Slurm's variadic C ABI.
// NOLINTBEGIN(cppcoreguidelines-pro-type-vararg)
int spank_get_item(spank_t /*spank*/, const spank_item_t item, ...) {
  if (item != S_TASK_ID) {
    return ESPANK_ERROR;
  }
  va_list arguments;
  va_start(arguments, item);
  auto* taskId = va_arg(arguments, int*);
  va_end(arguments);
  if (taskId == nullptr) {
    return ESPANK_ERROR;
  }
  *taskId = spank_test::state().taskId;
  return ESPANK_SUCCESS;
}
// NOLINTEND(cppcoreguidelines-pro-type-vararg)

int spank_setenv(spank_t /*spank*/, const char* name, const char* value,
                 int overwrite) {
  spank_test::state().environmentOverwrites.push_back(overwrite);
  if (spank_test::state().environmentResult != ESPANK_SUCCESS) {
    return spank_test::state().environmentResult;
  }
  spank_test::state().environment[name] = value;
  return ESPANK_SUCCESS;
}

// This test double must mirror Slurm's variadic C ABI.
// NOLINTBEGIN(cppcoreguidelines-pro-type-vararg)
void slurm_spank_log(const char* format, ...) {
  std::string message{format};
  va_list arguments;
  va_start(arguments, format);
  if (message == "amazon-braket-qdmi: %s") {
    message = "amazon-braket-qdmi: ";
    message += va_arg(arguments, const char*);
  }
  va_end(arguments);
  spank_test::state().logs.emplace_back(std::move(message));
}
// NOLINTEND(cppcoreguidelines-pro-type-vararg)

int AMAZON_BRAKET_QDMI_device_initialize(void) {
  ++spank_test::state().initializeCalls;
  return spank_test::state().deviceInitializeResult;
}

int AMAZON_BRAKET_QDMI_device_finalize(void) {
  ++spank_test::state().finalizeCalls;
  return QDMI_SUCCESS;
}

int AMAZON_BRAKET_QDMI_device_session_alloc(
    AMAZON_BRAKET_QDMI_Device_Session* session) {
  ++spank_test::state().sessionAllocCalls;
  if (spank_test::state().sessionAllocResult == QDMI_SUCCESS) {
    *session =
        reinterpret_cast<AMAZON_BRAKET_QDMI_Device_Session>(sessionStorage());
  }
  return spank_test::state().sessionAllocResult;
}

int AMAZON_BRAKET_QDMI_device_session_set_parameter(
    AMAZON_BRAKET_QDMI_Device_Session /*session*/,
    QDMI_Device_Session_Parameter parameter, size_t size, const void* value) {
  spank_test::state().parameters.push_back(parameter);
  if (spank_test::state().setParameterResult != QDMI_SUCCESS) {
    return spank_test::state().setParameterResult;
  }
  spank_test::state().parameterValues[parameter] =
      std::string(static_cast<const char*>(value), size - 1);
  return QDMI_SUCCESS;
}

int AMAZON_BRAKET_QDMI_device_session_init(
    AMAZON_BRAKET_QDMI_Device_Session /*session*/) {
  ++spank_test::state().sessionInitCalls;
  for (const char* name : {AMAZON_BRAKET_QDMI_DEVICE_ENV_BASEURL,
                           AMAZON_BRAKET_QDMI_DEVICE_ENV_REGION,
                           AMAZON_BRAKET_QDMI_DEVICE_ENV_RESERVATION_ARN,
                           AMAZON_BRAKET_QDMI_DEVICE_ENV_AUTHFILE}) {
    if (const char* value = std::getenv(name); value != nullptr) {
      spank_test::state().sessionEnvironmentAtInit[name] = value;
    }
  }
  return spank_test::state().sessionInitResult;
}

void AMAZON_BRAKET_QDMI_device_session_free(
    AMAZON_BRAKET_QDMI_Device_Session /*session*/) {
  ++spank_test::state().sessionFreeCalls;
}

int AMAZON_BRAKET_QDMI_device_session_query_device_property(
    AMAZON_BRAKET_QDMI_Device_Session /*session*/,
    QDMI_Device_Property property, size_t size, void* value,
    size_t* /*sizeRet*/) {
  ++spank_test::state().queryStatusCalls;
  if (spank_test::state().queryStatusResult != QDMI_SUCCESS) {
    return spank_test::state().queryStatusResult;
  }
  if (property == QDMI_DEVICE_PROPERTY_STATUS &&
      size >= sizeof(QDMI_Device_Status)) {
    *static_cast<QDMI_Device_Status*>(value) =
        static_cast<QDMI_Device_Status>(spank_test::state().deviceStatus);
  }
  return QDMI_SUCCESS;
}

} // extern "C"
