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
 * Configuration precedence is:
 * 1. `srun`/`sbatch` options,
 * 2. the submitted job environment,
 * 3. administrator defaults in `plugstack.conf`.
 *
 * A device ARN opts a job into validation. Explicit credentials are optional;
 * when no credentials file is configured, the Amazon Braket QDMI device uses
 * the AWS SDK default credential provider chain.
 */

#include "amazon-braket-qdmi-device/constants.hpp"
#include "amazon_braket_qdmi/device.h"

#include <array>
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <optional>
#include <string>
#include <string_view>

extern "C" {
#include <slurm/slurm_errno.h>
#include <slurm/slurm_version.h>
#include <slurm/spank.h>
}

// POSIX setenv and unsetenv are declared by stdlib.h.
#include <stdlib.h> // NOLINT(modernize-deprecated-headers)

static_assert(SLURM_VERSION_NUMBER >= SLURM_VERSION_NUM(20, 2, 0),
              "The Amazon Braket SPANK plugin requires Slurm 20.02 or newer");

// These names are fixed by the Slurm plugin ABI.
// NOLINTBEGIN(readability-identifier-naming)
extern "C" {
extern const char plugin_name[] = "amazon_braket_qdmi";
extern const char plugin_type[] = "spank";
extern const unsigned int plugin_version = SLURM_VERSION_NUMBER;
extern const unsigned int spank_plugin_version = 1;
}
// NOLINTEND(readability-identifier-naming)

namespace {

struct ConfigMapping {
  std::string_view plugstackKey;
  const char* environment;
  const char* optionName;
  const char* argumentName;
  const char* usage;
  QDMI_Device_Session_Parameter parameter;
};

struct SessionParameterMapping {
  const char* environment;
  QDMI_Device_Session_Parameter parameter;
};

constexpr std::array<ConfigMapping, 4> CONFIG_MAPPINGS = {{
    {.plugstackKey = "amazon_braket_device_arn",
     .environment = AMAZON_BRAKET_QDMI_DEVICE_ENV_DEVICE_ARN,
     .optionName = "amazon-braket-device-arn",
     .argumentName = "ARN",
     .usage = "Amazon Braket device ARN",
     .parameter = AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN},
    {.plugstackKey = "amazon_braket_region",
     .environment = AMAZON_BRAKET_QDMI_DEVICE_ENV_REGION,
     .optionName = "amazon-braket-region",
     .argumentName = "REGION",
     .usage = "AWS region",
     .parameter = AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_REGION},
    {.plugstackKey = "amazon_braket_reservation_arn",
     .environment = AMAZON_BRAKET_QDMI_DEVICE_ENV_RESERVATION_ARN,
     .optionName = "amazon-braket-reservation-arn",
     .argumentName = "ARN",
     .usage = "Amazon Braket reservation ARN",
     .parameter = AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_RESERVATION_ARN},
    {.plugstackKey = "amazon_braket_credentials_file",
     .environment = AMAZON_BRAKET_QDMI_DEVICE_ENV_AUTHFILE,
     .optionName = "amazon-braket-credentials-file",
     .argumentName = "PATH",
     .usage = "AWS shared credentials file",
     .parameter = QDMI_DEVICE_SESSION_PARAMETER_AUTHFILE},
}};

constexpr std::array<SessionParameterMapping, 3> AWS_CREDENTIAL_MAPPINGS = {{
    {.environment = "AWS_ACCESS_KEY_ID",
     .parameter = QDMI_DEVICE_SESSION_PARAMETER_USERNAME},
    {.environment = "AWS_SECRET_ACCESS_KEY",
     .parameter = QDMI_DEVICE_SESSION_PARAMETER_PASSWORD},
    {.environment = "AWS_SESSION_TOKEN",
     .parameter = QDMI_DEVICE_SESSION_PARAMETER_TOKEN},
}};

/**
 * Process-environment inputs consumed by the AWS SDK credential and endpoint
 * provider chains.
 *
 * A remote SPANK hook runs in slurmstepd, whose process environment is not the
 * submitted job environment exposed by spank_getenv(). Mirror these values
 * while validating so the SDK sees the job's configuration and never inherits
 * credentials from the daemon.
 */
constexpr std::array<const char*, 29> AWS_PROVIDER_ENVIRONMENT = {{
    "AWS_ACCESS_KEY_ID",
    "AWS_SECRET_ACCESS_KEY",
    "AWS_SESSION_TOKEN",
    "AWS_ACCOUNT_ID",
    "AWS_PROFILE",
    "AWS_DEFAULT_PROFILE",
    "AWS_CONFIG_FILE",
    "AWS_DEFAULT_REGION",
    "AWS_ROLE_ARN",
    "AWS_IAM_ROLE_ARN",
    "AWS_ROLE_SESSION_NAME",
    "AWS_IAM_ROLE_SESSION_NAME",
    "AWS_WEB_IDENTITY_TOKEN_FILE",
    "AWS_LOGIN_CACHE_DIRECTORY",
    "AWS_CONTAINER_CREDENTIALS_RELATIVE_URI",
    "AWS_CONTAINER_CREDENTIALS_FULL_URI",
    "AWS_CONTAINER_AUTHORIZATION_TOKEN",
    "AWS_CONTAINER_AUTHORIZATION_TOKEN_FILE",
    "AWS_EC2_METADATA_DISABLED",
    "AWS_EC2_METADATA_V1_DISABLED",
    "AWS_EC2_METADATA_SERVICE_ENDPOINT",
    "AWS_EC2_METADATA_SERVICE_ENDPOINT_MODE",
    "AWS_METADATA_SERVICE_TIMEOUT",
    "AWS_METADATA_SERVICE_NUM_ATTEMPTS",
    "AWS_DEFAULTS_MODE",
    "AWS_ENDPOINT_URL",
    "AWS_ENDPOINT_URL_BRAKET",
    "AWS_IGNORE_CONFIGURED_ENDPOINT_URLS",
    "HOME",
}};

struct ValidationState {
  bool active = false;
  bool complete = false;
  bool successful = false;
};

auto mutableText(const char* text) -> char* {
  // Slurm's option structure predates const-correct string fields.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
  return const_cast<char*>(text);
}

auto validValue(const char* value) -> bool {
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

auto getJobEnvironment(spank_t spank, const char* name)
    -> std::optional<std::string> {
  std::array<char, 4096> buffer{};
  if (spank_getenv(spank, name, buffer.data(),
                   static_cast<int>(buffer.size())) != ESPANK_SUCCESS ||
      !validValue(buffer.data())) {
    return std::nullopt;
  }
  return std::string{buffer.data()};
}

auto logFailure(const char* message) -> void {
  // Slurm exposes logging through a variadic C ABI.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
  slurm_spank_log("amazon-braket-qdmi: %s", message);
}

auto logHook(const char* hook) -> void {
  // Slurm exposes logging through a variadic C ABI.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
  slurm_spank_log("amazon-braket-qdmi: hook=%s", hook);
}

/**
 * Isolate the daemon environment while validating the submitted job.
 *
 * Effective plugin configuration is passed directly to the QDMI session.
 * Those fallback variables are hidden, while AWS SDK provider inputs mirror
 * the values exposed by spank_getenv(). This prevents slurmstepd service
 * credentials from leaking into a job and preserves job-scoped provider-chain
 * sources such as web identity and container credentials.
 */
class ScopedSessionEnvironment final {
public:
  explicit ScopedSessionEnvironment(spank_t spank) {
    try {
      for (const auto& mapping : CONFIG_MAPPINGS) {
        if (!replace(mapping.environment, std::nullopt)) {
          valid_ = false;
          return;
        }
      }
      for (const auto* name : AWS_PROVIDER_ENVIRONMENT) {
        if (!replace(name, getJobEnvironment(spank, name))) {
          valid_ = false;
          return;
        }
      }
    } catch (...) {
      restoreChanges();
      throw;
    }
  }

  ScopedSessionEnvironment(const ScopedSessionEnvironment&) = delete;
  auto operator=(const ScopedSessionEnvironment&)
      -> ScopedSessionEnvironment& = delete;
  ScopedSessionEnvironment(ScopedSessionEnvironment&&) = delete;
  auto operator=(ScopedSessionEnvironment&&)
      -> ScopedSessionEnvironment& = delete;

  ~ScopedSessionEnvironment() { restoreChanges(); }

  [[nodiscard]] auto valid() const -> bool { return valid_; }

private:
  void restoreChanges() noexcept {
    while (changedValues_ > 0) {
      --changedValues_;
      const auto& change = changes_[changedValues_];
      if (restore(change.name, change.originalValue) != 0) {
        logFailure("failed to restore the process environment");
      }
    }
  }

  struct EnvironmentChange {
    const char* name = nullptr;
    std::optional<std::string> originalValue;
  };

  static auto restore(const char* name, const std::optional<std::string>& value)
      -> int {
    if (value.has_value()) {
      return setenv(name, value->c_str(), 1);
    }
    return unsetenv(name);
  }

  auto replace(const char* name, const std::optional<std::string>& replacement)
      -> bool {
    auto& change = changes_[changedValues_];
    change.name = name;
    if (const char* value = std::getenv(name); value != nullptr) {
      change.originalValue = value;
    }
    if (restore(name, replacement) != 0) {
      return false;
    }
    ++changedValues_;
    return true;
  }

  std::array<EnvironmentChange,
             CONFIG_MAPPINGS.size() + AWS_PROVIDER_ENVIRONMENT.size()>
      changes_{};
  size_t changedValues_ = 0;
  bool valid_ = true;
};

struct QdmiGuard {
  AMAZON_BRAKET_QDMI_Device_Session session = nullptr;
  bool finalizeDevice = false;

  QdmiGuard() = default;
  QdmiGuard(const QdmiGuard&) = delete;
  auto operator=(const QdmiGuard&) -> QdmiGuard& = delete;
  QdmiGuard(QdmiGuard&&) = delete;
  auto operator=(QdmiGuard&&) -> QdmiGuard& = delete;

  ~QdmiGuard() {
    if (session != nullptr) {
      AMAZON_BRAKET_QDMI_device_session_free(session);
    }
    if (finalizeDevice) {
      AMAZON_BRAKET_QDMI_device_finalize();
    }
  }
};

class BraketSpankConfig final {
public:
  void parsePlugstackArguments(const int count, char** arguments) {
    plugstackValues_ = {};
    optionValues_ = {};
    for (int index = 0; index < count; ++index) {
      if (arguments[index] == nullptr) {
        continue;
      }
      if (!validValue(arguments[index])) {
        logMalformedArgument(arguments[index]);
        continue;
      }
      const std::string_view argument{arguments[index]};
      const auto separator = argument.find('=');
      if (separator == std::string_view::npos || separator == 0 ||
          separator + 1 >= argument.size()) {
        logMalformedArgument(arguments[index]);
        continue;
      }

      const auto key = argument.substr(0, separator);
      const auto value = argument.substr(separator + 1);
      bool matched = false;
      for (size_t mappingIndex = 0; mappingIndex < CONFIG_MAPPINGS.size();
           ++mappingIndex) {
        if (key == CONFIG_MAPPINGS[mappingIndex].plugstackKey) {
          plugstackValues_[mappingIndex] = std::string{value};
          matched = true;
          break;
        }
      }
      if (!matched) {
        // Slurm exposes logging through a variadic C ABI.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
        slurm_spank_log(
            "amazon-braket-qdmi: ignoring unknown plugstack argument '%.*s'",
            static_cast<int>(key.size()), key.data());
      }
    }
  }

  auto registerOptions(spank_t spank) -> int {
    for (size_t index = 0; index < CONFIG_MAPPINGS.size(); ++index) {
      const auto& mapping = CONFIG_MAPPINGS[index];
      optionDefinitions_[index] = {
          .name = mutableText(mapping.optionName),
          .arginfo = mutableText(mapping.argumentName),
          .usage = mutableText(mapping.usage),
          .has_arg = 1,
          .val = static_cast<int>(index),
          .cb = optionCallback,
      };
      if (spank_option_register(spank, &optionDefinitions_[index]) !=
          ESPANK_SUCCESS) {
        logFailure("failed to register an Amazon Braket option");
        return ESPANK_ERROR;
      }
    }
    return ESPANK_SUCCESS;
  }

  auto injectEnvironment(spank_t spank) const -> bool {
    for (size_t index = 0; index < CONFIG_MAPPINGS.size(); ++index) {
      const std::string* value = nullptr;
      int overwrite = 0;
      if (optionValues_[index].has_value()) {
        value = &optionValues_[index].value();
        overwrite = 1;
      } else if (plugstackValues_[index].has_value()) {
        value = &plugstackValues_[index].value();
      }
      if (value == nullptr) {
        continue;
      }

      const auto result = spank_setenv(
          spank, CONFIG_MAPPINGS[index].environment, value->c_str(), overwrite);
      if (result != ESPANK_SUCCESS && result != ESPANK_ENV_EXISTS) {
        logFailure("failed to inject the Amazon Braket job environment");
        return false;
      }
    }
    return true;
  }

  [[nodiscard]] static auto isActive(spank_t spank) -> bool {
    return getJobEnvironment(spank, CONFIG_MAPPINGS.front().environment)
        .has_value();
  }

  [[nodiscard]] static auto validateBackend(spank_t spank) noexcept -> bool {
    try {
      const ScopedSessionEnvironment environmentScope{spank};
      if (!environmentScope.valid()) {
        logFailure("failed to isolate the QDMI job environment");
        return false;
      }

      QdmiGuard guard;
      guard.finalizeDevice = true;
      if (AMAZON_BRAKET_QDMI_device_initialize() != QDMI_SUCCESS) {
        guard.finalizeDevice = false;
        logFailure("failed to initialize the Amazon Braket QDMI device");
        return false;
      }

      if (AMAZON_BRAKET_QDMI_device_session_alloc(&guard.session) !=
          QDMI_SUCCESS) {
        logFailure("failed to allocate an Amazon Braket QDMI session");
        return false;
      }

      for (const auto& mapping : CONFIG_MAPPINGS) {
        const auto value = getJobEnvironment(spank, mapping.environment);
        if (!value.has_value()) {
          continue;
        }
        if (AMAZON_BRAKET_QDMI_device_session_set_parameter(
                guard.session, mapping.parameter, value->size() + 1,
                value->c_str()) != QDMI_SUCCESS) {
          logFailure("invalid Amazon Braket session parameter");
          return false;
        }
      }
      for (const auto& mapping : AWS_CREDENTIAL_MAPPINGS) {
        const auto value = getJobEnvironment(spank, mapping.environment);
        if (!value.has_value()) {
          continue;
        }
        if (AMAZON_BRAKET_QDMI_device_session_set_parameter(
                guard.session, mapping.parameter, value->size() + 1,
                value->c_str()) != QDMI_SUCCESS) {
          logFailure("invalid AWS credential environment");
          return false;
        }
      }

      if (AMAZON_BRAKET_QDMI_device_session_init(guard.session) !=
          QDMI_SUCCESS) {
        logFailure("Amazon Braket session validation failed");
        return false;
      }

      QDMI_Device_Status status = QDMI_DEVICE_STATUS_OFFLINE;
      if (AMAZON_BRAKET_QDMI_device_session_query_device_property(
              guard.session, QDMI_DEVICE_PROPERTY_STATUS, sizeof(status),
              &status, nullptr) != QDMI_SUCCESS ||
          (status != QDMI_DEVICE_STATUS_IDLE &&
           status != QDMI_DEVICE_STATUS_BUSY)) {
        logFailure("Amazon Braket device is not available");
        return false;
      }
    } catch (const std::exception& error) {
      // Slurm exposes logging through a variadic C ABI.
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
      slurm_spank_log("amazon-braket-qdmi: validation raised an exception: %s",
                      error.what());
      return false;
    } catch (...) {
      logFailure("validation raised an unknown exception");
      return false;
    }
    return true;
  }

  void resetValidation() { validation_ = {}; }

  void skipValidation() {
    validation_ = {.active = false, .complete = true, .successful = true};
  }

  void finishValidation(const bool successful) {
    validation_ = {.active = true, .complete = true, .successful = successful};
  }

  [[nodiscard]] auto validation() const -> ValidationState {
    return validation_;
  }

private:
  static auto optionCallback(int value, const char* argument, int remote)
      -> int;

  static void logMalformedArgument(const char* argument) {
    // Slurm exposes logging through a variadic C ABI.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    slurm_spank_log(
        "amazon-braket-qdmi: ignoring malformed plugstack argument '%s'",
        argument);
  }

  std::array<std::optional<std::string>, CONFIG_MAPPINGS.size()>
      plugstackValues_{};
  std::array<std::optional<std::string>, CONFIG_MAPPINGS.size()>
      optionValues_{};
  std::array<spank_option, CONFIG_MAPPINGS.size()> optionDefinitions_{};
  ValidationState validation_{};
};

BraketSpankConfig
    config; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

auto BraketSpankConfig::optionCallback(const int value, const char* argument,
                                       const int /*remote*/) -> int {
  if (value < 0 || static_cast<size_t>(value) >= CONFIG_MAPPINGS.size() ||
      !validValue(argument)) {
    logFailure("invalid Amazon Braket option value");
    return -1;
  }
  try {
    config.optionValues_[static_cast<size_t>(value)] = argument;
  } catch (const std::exception& error) {
    // Slurm exposes logging through a variadic C ABI.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    slurm_spank_log(
        "amazon-braket-qdmi: option processing raised an exception: %s",
        error.what());
    return -1;
  } catch (...) {
    logFailure("option processing raised an unknown exception");
    return -1;
  }
  return 0;
}

auto isPrimaryTask(spank_t spank) -> bool {
  int taskId = 0;
  // Slurm exposes item queries through a variadic C ABI.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
  return spank_get_item(spank, S_TASK_ID, &taskId) != ESPANK_SUCCESS ||
         taskId == 0;
}

} // namespace

// These names and signatures are required by the Slurm SPANK ABI.
// NOLINTBEGIN(misc-use-internal-linkage, readability-identifier-naming,
//             cppcoreguidelines-avoid-c-arrays)
extern "C" {
int slurm_spank_init(spank_t spank, const int count, char* arguments[]) {
  logHook("slurm_spank_init");
  try {
    config.parsePlugstackArguments(count, arguments);
    if (spank_remote(spank) == 1) {
      config.resetValidation();
    }
    return config.registerOptions(spank);
  } catch (const std::exception& error) {
    // Slurm exposes logging through a variadic C ABI.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    slurm_spank_log(
        "amazon-braket-qdmi: initialization raised an exception: %s",
        error.what());
  } catch (...) {
    logFailure("initialization raised an unknown exception");
  }
  return ESPANK_ERROR;
}

int slurm_spank_init_post_opt(spank_t /*spank*/, int /*count*/,
                              char* /*arguments*/[]) {
  logHook("slurm_spank_init_post_opt");
  return ESPANK_SUCCESS;
}

int slurm_spank_user_init(spank_t spank, int /*count*/, char* /*arguments*/[]) {
  logHook("slurm_spank_user_init");
  if (spank_remote(spank) != 1 || config.validation().complete) {
    return ESPANK_SUCCESS;
  }

  try {
    if (!config.injectEnvironment(spank)) {
      config.finishValidation(false);
      return ESPANK_SUCCESS;
    }
    if (!config.isActive(spank)) {
      config.skipValidation();
      return ESPANK_SUCCESS;
    }

    config.finishValidation(config.validateBackend(spank));
  } catch (const std::exception& error) {
    config.finishValidation(false);
    // Slurm exposes logging through a variadic C ABI.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    slurm_spank_log(
        "amazon-braket-qdmi: user initialization raised an exception: %s",
        error.what());
  } catch (...) {
    config.finishValidation(false);
    logFailure("user initialization raised an unknown exception");
  }
  return ESPANK_SUCCESS;
}

int slurm_spank_task_init(spank_t spank, int /*count*/, char* /*arguments*/[]) {
  if (spank_remote(spank) != 1) {
    return ESPANK_SUCCESS;
  }
  const auto state = config.validation();
  if (!state.active) {
    return ESPANK_SUCCESS;
  }
  if (!state.complete || !state.successful) {
    if (isPrimaryTask(spank)) {
      logFailure("job rejected because Amazon Braket validation failed");
    }
    // slurmstepd treats only a negative task-init result as fatal.
    return -1;
  }
  return ESPANK_SUCCESS;
}
} // extern "C"

// NOLINTEND(misc-use-internal-linkage, readability-identifier-naming,
//           cppcoreguidelines-avoid-c-arrays)
