/*
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <https://www.gnu.org/licenses/>.
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
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <optional>
#include <slurm/slurm.h>
#include <slurm/slurm_errno.h>
#include <slurm/spank.h>
#include <string>
#include <syslog.h>

static_assert(SLURM_VERSION_NUMBER >= SLURM_VERSION_NUM(20, 2, 0),
              "The Amazon Braket SPANK plugin requires Slurm 20.02 or newer");

SPANK_PLUGIN(amazon_braket_qdmi, 1); // NOLINT

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

constexpr std::array<const char*, 4> SESSION_ENVIRONMENT = {
    AMAZON_BRAKET_QDMI_DEVICE_ENV_BASEURL,
    AMAZON_BRAKET_QDMI_DEVICE_ENV_REGION,
    AMAZON_BRAKET_QDMI_DEVICE_ENV_RESERVATION_ARN,
    AMAZON_BRAKET_QDMI_DEVICE_ENV_AUTHFILE,
};

struct ValidationState {
  bool active = false;
  bool complete = false;
  bool successful = false;
};

auto slurmText(const char* text) -> char* {
  // Slurm's option structure requires mutable pointers for static text.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
  return const_cast<char*>(text);
}

struct PluginState {
  std::mutex optionsMutex;
  std::array<std::string, 4> optionValues;
  std::mutex validationMutex;
  ValidationState validationState;
  std::array<spank_option, 4> options = {{
      {.name = slurmText("qdmi-device-session-parameter-baseurl"),
       .arginfo = slurmText("ARN"),
       .usage = slurmText("Amazon Braket device ARN"),
       .has_arg = 1,
       .val = static_cast<int>(OptionValue::DeviceArn),
       .cb = nullptr},
      {.name = slurmText("qdmi-device-session-parameter-region"),
       .arginfo = slurmText("REGION"),
       .usage = slurmText("AWS region"),
       .has_arg = 1,
       .val = static_cast<int>(OptionValue::Region),
       .cb = nullptr},
      {.name = slurmText("qdmi-device-session-parameter-reservation-arn"),
       .arginfo = slurmText("ARN"),
       .usage = slurmText("Amazon Braket reservation ARN"),
       .has_arg = 1,
       .val = static_cast<int>(OptionValue::ReservationArn),
       .cb = nullptr},
      {.name = slurmText("qdmi-device-session-parameter-authfile"),
       .arginfo = slurmText("PATH"),
       .usage = slurmText("AWS credentials file path"),
       .has_arg = 1,
       .val = static_cast<int>(OptionValue::AuthFile),
       .cb = nullptr},
  }};
};

auto pluginState() -> PluginState& {
  static PluginState state;
  return state;
}

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

  auto& state = pluginState();
  const std::scoped_lock lock(state.optionsMutex);
  state.optionValues[static_cast<size_t>(value) -
                     static_cast<size_t>(OptionValue::DeviceArn)] = argument;
  return 0;
}

auto snapshotOptions() -> std::array<std::string, 4> {
  auto& state = pluginState();
  const std::scoped_lock lock(state.optionsMutex);
  return state.optionValues;
}

auto getJobEnvironment(spank_t spank, const char* name)
    -> std::optional<std::string> {
  std::array<char, 4096> buffer{};
  if (spank_getenv(spank, name, buffer.data(),
                   static_cast<int>(buffer.size())) != ESPANK_SUCCESS ||
      !validOptionValue(buffer.data())) {
    return std::nullopt;
  }
  return std::string{buffer.data()};
}

auto collectRemoteOptions(spank_t spank) -> std::array<std::string, 4> {
  auto values = snapshotOptions();
  for (auto& option : pluginState().options) {
    // Slurm's getopt API writes the argument pointer through char**.
    // NOLINTNEXTLINE(misc-const-correctness)
    char* argument = nullptr;
    if (spank_option_getopt(spank, &option, &argument) == ESPANK_SUCCESS &&
        validOptionValue(argument)) {
      values[optionIndex(static_cast<OptionValue>(option.val))] = argument;
    }
  }
  for (size_t index = 0; index < values.size(); ++index) {
    if (values[index].empty()) {
      if (const auto jobValue =
              getJobEnvironment(spank, SESSION_ENVIRONMENT[index]);
          jobValue.has_value()) {
        values[index] = *jobValue;
      }
    }
  }
  return values;
}

/**
 * Temporarily mirror the exact SPANK job environment while QDMI initializes.
 *
 * The core library supports process-environment fallbacks, while SPANK keeps
 * job variables in a separate environment. Mirroring both present and absent
 * values prevents slurmstepd service variables from changing what is validated.
 */
class ScopedSessionEnvironment final {
public:
  explicit ScopedSessionEnvironment(
      const std::array<std::string, 4>& jobValues) {
    for (size_t index = 0; index < SESSION_ENVIRONMENT.size(); ++index) {
      if (const char* value = std::getenv(SESSION_ENVIRONMENT[index]);
          value != nullptr) {
        originalValues[index] = value;
      }
      if (setValue(SESSION_ENVIRONMENT[index], jobValues[index]) != 0) {
        validState = false;
        break;
      }
      ++configuredValues;
    }
  }

  ScopedSessionEnvironment(const ScopedSessionEnvironment&) = delete;
  auto operator=(const ScopedSessionEnvironment&)
      -> ScopedSessionEnvironment& = delete;
  ScopedSessionEnvironment(ScopedSessionEnvironment&&) = delete;
  auto operator=(ScopedSessionEnvironment&&)
      -> ScopedSessionEnvironment& = delete;

  ~ScopedSessionEnvironment() {
    for (size_t index = 0; index < configuredValues; ++index) {
      if (setValue(SESSION_ENVIRONMENT[index],
                   originalValues[index].value_or("")) != 0) {
        slurm_spank_log(
            "amazon-braket-qdmi: failed to restore QDMI process environment");
      }
    }
  }

  [[nodiscard]] auto valid() const -> bool { return validState; }

private:
  static auto setValue(const char* name, const std::string& value) -> int {
    if (value.empty()) {
      return unsetenv(name);
    }
    return setenv(name, value.c_str(), 1);
  }

  std::array<std::optional<std::string>, 4> originalValues;
  size_t configuredValues = 0;
  bool validState = true;
};

auto setValidationState(const bool successful) -> void {
  auto& state = pluginState();
  const std::scoped_lock lock(state.validationMutex);
  state.validationState = {
      .active = true, .complete = true, .successful = successful};
}

auto getValidationState() -> ValidationState {
  auto& state = pluginState();
  const std::scoped_lock lock(state.validationMutex);
  return state.validationState;
}

auto logFailure(const char* message) -> void {
  slurm_spank_log("amazon-braket-qdmi: %s", message); // NOLINT
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

  const ScopedSessionEnvironment environmentScope{values};
  if (!environmentScope.valid()) {
    logFailure("failed to isolate the QDMI job environment");
    return false;
  }

  if (AMAZON_BRAKET_QDMI_device_initialize() != QDMI_SUCCESS) {
    logFailure("failed to initialize QDMI");
    return false;
  }

  struct QdmiGuard {
    AMAZON_BRAKET_QDMI_Device_Session session = nullptr;

    ~QdmiGuard() {
      if (session != nullptr) {
        AMAZON_BRAKET_QDMI_device_session_free(session);
      }
      AMAZON_BRAKET_QDMI_device_finalize();
    }
  } guard;

  if (AMAZON_BRAKET_QDMI_device_session_alloc(&guard.session) != QDMI_SUCCESS) {
    logFailure("failed to allocate QDMI session");
    return false;
  }

  const auto setString = [&](const QDMI_Device_Session_Parameter parameter,
                             const std::string& value) -> bool {
    return AMAZON_BRAKET_QDMI_device_session_set_parameter(
               guard.session, parameter, value.size() + 1, value.c_str()) ==
           QDMI_SUCCESS;
  };

  if (!setString(QDMI_DEVICE_SESSION_PARAMETER_BASEURL,
                 values[optionIndex(OptionValue::DeviceArn)]) ||
      !setString(QDMI_DEVICE_SESSION_PARAMETER_AUTHFILE,
                 values[optionIndex(OptionValue::AuthFile)])) {
    logFailure("invalid QDMI session parameters");
    return false;
  }
  if (!values[optionIndex(OptionValue::Region)].empty() &&
      !setString(QDMI_DEVICE_SESSION_PARAMETER_REGION,
                 values[optionIndex(OptionValue::Region)])) {
    logFailure("invalid AWS region");
    return false;
  }
  if (!values[optionIndex(OptionValue::ReservationArn)].empty() &&
      !setString(QDMI_DEVICE_SESSION_PARAMETER_RESERVATION_ARN,
                 values[optionIndex(OptionValue::ReservationArn)])) {
    logFailure("invalid reservation ARN");
    return false;
  }

  if (AMAZON_BRAKET_QDMI_device_session_init(guard.session) != QDMI_SUCCESS) {
    logFailure("could not initialize the Amazon Braket session");
    return false;
  }

  QDMI_Device_Status status = QDMI_DEVICE_STATUS_OFFLINE;
  if (AMAZON_BRAKET_QDMI_device_session_query_device_property(
          guard.session, QDMI_DEVICE_PROPERTY_STATUS, sizeof(status), &status,
          nullptr) != QDMI_SUCCESS) {
    logFailure("could not query Amazon Braket device status");
    return false;
  }

  if (status != QDMI_DEVICE_STATUS_IDLE && status != QDMI_DEVICE_STATUS_BUSY) {
    logFailure("Amazon Braket device is not available");
    return false;
  }
  return true;
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

// These names and signatures are required by the Slurm SPANK ABI.
// NOLINTBEGIN(misc-use-internal-linkage, readability-identifier-naming,
//             cppcoreguidelines-avoid-c-arrays)
int slurm_spank_init(spank_t spank, int /*ac*/, char* /*argv*/[]) {
  auto& state = pluginState();
  {
    const std::scoped_lock lock(state.optionsMutex);
    state.optionValues = {};
  }
  for (auto& option : state.options) {
    option.cb = optionCallback;
    if (spank_option_register(spank, &option) != ESPANK_SUCCESS) {
      return 1;
    }
  }
  if (spank_remote(spank) == 1) {
    const std::scoped_lock lock(state.validationMutex);
    state.validationState = {};
  }
  return 0;
}

int slurm_spank_init_post_opt(spank_t spank, int /*ac*/, char* /*argv*/[]) {
  const auto values = snapshotOptions();

  // Check the opt-in pair before sbatch schedules the job.
  if (spank_context() == S_CTX_ALLOCATOR) {
    return validateRequiredOptions(values) ? 0 : 1;
  }

  if (spank_remote(spank) != 1) {
    return 0;
  }

  if (values[optionIndex(OptionValue::DeviceArn)].empty() &&
      values[optionIndex(OptionValue::AuthFile)].empty()) {
    auto& state = pluginState();
    const std::scoped_lock lock(state.validationMutex);
    state.validationState = {
        .active = false, .complete = true, .successful = true};
    return 0;
  }
  if (!validateRequiredOptions(values)) {
    setValidationState(false);
  }
  return 0;
}

int slurm_spank_user_init(spank_t spank, int /*ac*/, char* /*argv*/[]) {
  if (spank_remote(spank) != 1) {
    return 0;
  }

  // Run AWS/device checks once, after privileges are dropped.
  auto& state = pluginState();
  {
    const std::scoped_lock validationLock(state.validationMutex);
    if (state.validationState.complete) {
      return 0;
    }
  }

  // Read options forwarded to the remote context.
  const auto values = collectRemoteOptions(spank);
  const bool successful = validateDevice(values);
  // spank_setenv propagates values into the task environment.
  if (successful && !injectEnvironment(spank, values)) {
    logFailure("failed to inject QDMI environment variables");
    const std::scoped_lock validationLock(state.validationMutex);
    state.validationState = {
        .active = true, .complete = true, .successful = false};
    return 0;
  }
  const std::scoped_lock validationLock(state.validationMutex);
  state.validationState = {
      .active = true, .complete = true, .successful = successful};
  return 0;
}

int slurm_spank_task_init(spank_t spank, int /*ac*/, char* /*argv*/[]) {
  if (spank_remote(spank) != 1) {
    return 0;
  }
  // Enforce remote failures just before execve(); fail the job, not the node.
  const auto state = getValidationState();
  if (!state.active) {
    return 0;
  }
  if (!state.complete || !state.successful) {
    logFailure("job rejected because QDMI validation failed");
    return -1;
  }
  return 0;
}

// NOLINTEND(misc-use-internal-linkage, readability-identifier-naming,
//           cppcoreguidelines-avoid-c-arrays)
