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
 * @brief Optional Slurm environment injection for Amazon Braket jobs.
 *
 * The plugin transports references to AWS configuration. It does not select a
 * device, resolve credentials, or contact AWS. The process-mutable Slurm
 * license environment only determines whether injection applies. MQT Core
 * validates the license and opens the device in the job process. Neither step
 * attests an allocation or authorizes AWS access. AWS IAM remains the
 * access-control boundary.
 */

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <optional>
#include <string>
#include <string_view>

extern "C" {
#include <slurm/slurm_errno.h>
#include <slurm/slurm_version.h>
#include <slurm/spank.h>
}

static_assert(SLURM_VERSION_NUMBER >= SLURM_VERSION_NUM(23, 2, 0),
              "The Amazon Braket SPANK plugin requires Slurm 23.02 or newer");

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
constexpr std::string_view BRAKET_LICENSE_PREFIX = "amazon.braket.";
constexpr std::string_view GENERIC_BRAKET_LICENSE = "amazon.braket.default";
constexpr auto LICENSE_ENVIRONMENT = "SLURM_JOB_LICENSES";
// slurmstepd treats only a negative task-init return as fatal. ESPANK_ERROR is
// positive and would log an error without preventing the workload from running.
constexpr auto TASK_REJECTED = -1;

struct ConfigMapping {
  std::string_view plugstackKey;
  const char* environment = nullptr;
  const char* optionName = nullptr;
  const char* argumentName = nullptr;
  const char* usage = nullptr;
};

constexpr std::array CONFIG_MAPPINGS{
    ConfigMapping{.plugstackKey = "amazon_braket_qdmi_config_file",
                  .environment = "MQT_CORE_QDMI_CONFIG_FILE",
                  .optionName = "amazon-braket-qdmi-config-file",
                  .argumentName = "PATH",
                  .usage = "MQT Core QDMI device catalogue"},
    ConfigMapping{.plugstackKey = "amazon_braket_profile",
                  .environment = "AWS_PROFILE",
                  .optionName = "amazon-braket-profile",
                  .argumentName = "PROFILE",
                  .usage = "AWS profile name"},
    ConfigMapping{.plugstackKey = "amazon_braket_config_file",
                  .environment = "AWS_CONFIG_FILE",
                  .optionName = "amazon-braket-config-file",
                  .argumentName = "PATH",
                  .usage = "AWS config file"},
    ConfigMapping{.plugstackKey = "amazon_braket_shared_credentials_file",
                  .environment = "AWS_SHARED_CREDENTIALS_FILE",
                  .optionName = "amazon-braket-shared-credentials-file",
                  .argumentName = "PATH",
                  .usage = "AWS shared credentials file"},
    ConfigMapping{.plugstackKey = "amazon_braket_task_results_s3_uri",
                  .environment = "AMZN_BRAKET_TASK_RESULTS_S3_URI",
                  .optionName = "amazon-braket-task-results-s3-uri",
                  .argumentName = "S3_URI",
                  .usage = "Amazon Braket task result destination"},
    ConfigMapping{.plugstackKey = "amazon_braket_reservation_arn",
                  .environment = "AMAZON_BRAKET_RESERVATION_ARN",
                  .optionName = "amazon-braket-reservation-arn",
                  .argumentName = "ARN",
                  .usage = "Amazon Braket reservation ARN"},
};

enum class BraketLicenseContext : std::uint8_t {
  NotApplicable,
  Applicable,
  Invalid,
};

enum class JobEnvironmentState : std::uint8_t {
  Missing,
  Valid,
  Invalid,
};

struct JobEnvironmentValue {
  JobEnvironmentState state = JobEnvironmentState::Missing;
  std::string value;
};

auto mutableText(const char* text) -> char* {
  // Slurm's option structure predates const-correct string fields.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
  return const_cast<char*>(text);
}

auto validValue(const char* value) -> bool {
  if (value == nullptr || *value == '\0') {
    return false;
  }
  for (const char* current = value; *current != '\0'; ++current) {
    if (*current == '\n' || *current == '\r') {
      return false;
    }
  }
  return true;
}

auto trim(std::string_view value) -> std::string_view {
  constexpr std::string_view whitespace = " \t";
  const auto begin = value.find_first_not_of(whitespace);
  if (begin == std::string_view::npos) {
    return {};
  }
  const auto end = value.find_last_not_of(whitespace);
  return value.substr(begin, end - begin + 1);
}

auto getJobEnvironment(spank_t spank, const char* name) -> JobEnvironmentValue {
  std::array<char, 4096> buffer{};
  const auto result =
      spank_getenv(spank, name, buffer.data(), static_cast<int>(buffer.size()));
  if (result == ESPANK_NOSPACE) {
    return {.state = JobEnvironmentState::Invalid, .value = {}};
  }
  if (result != ESPANK_SUCCESS) {
    return {.state = JobEnvironmentState::Missing, .value = {}};
  }
  if (!validValue(buffer.data())) {
    return {.state = JobEnvironmentState::Invalid, .value = {}};
  }
  return {.state = JobEnvironmentState::Valid, .value = buffer.data()};
}

auto logFailure(const char* message) -> void {
  // Slurm exposes logging through a variadic C ABI.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
  slurm_spank_log("amazon-braket-qdmi: %s", message);
}

auto classifyBraketLicense(const std::string_view expression)
    -> BraketLicenseContext {
  auto context = BraketLicenseContext::NotApplicable;
  size_t tokenBegin = 0;
  while (tokenBegin <= expression.size()) {
    const auto tokenEnd = expression.find_first_of(",|", tokenBegin);
    const auto token =
        trim(expression.substr(tokenBegin, tokenEnd == std::string_view::npos
                                               ? std::string_view::npos
                                               : tokenEnd - tokenBegin));
    const auto nameEnd = token.find_first_of(":@");
    const auto name = token.substr(0, nameEnd);
    if (name == GENERIC_BRAKET_LICENSE) {
      return BraketLicenseContext::Invalid;
    }
    if (name.starts_with(BRAKET_LICENSE_PREFIX)) {
      context = BraketLicenseContext::Applicable;
    }
    if (tokenEnd == std::string_view::npos) {
      return context;
    }
    tokenBegin = tokenEnd + 1;
  }
  return context;
}

class BraketSpankConfig final {
public:
  void parsePlugstackArguments(const int count, char** arguments) {
    plugstackValues_ = {};
    optionValues_ = {};
    for (int index = 0; index < count; ++index) {
      if (arguments[index] == nullptr || !validValue(arguments[index])) {
        logFailure("ignoring a malformed plugstack argument");
        continue;
      }
      const std::string_view argument{arguments[index]};
      const auto separator = argument.find('=');
      if (separator == std::string_view::npos || separator == 0 ||
          separator + 1 >= argument.size()) {
        logFailure("ignoring a malformed plugstack argument");
        continue;
      }

      const auto key = argument.substr(0, separator);
      const auto value = argument.substr(separator + 1);
      bool matched = false;
      for (size_t mappingIndex = 0; mappingIndex < CONFIG_MAPPINGS.size();
           ++mappingIndex) {
        if (key == CONFIG_MAPPINGS[mappingIndex].plugstackKey) {
          plugstackValues_[mappingIndex] = value;
          matched = true;
          break;
        }
      }
      if (!matched) {
        logFailure("ignoring an unknown plugstack argument");
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

  [[nodiscard]] auto optionsPresent() const -> bool {
    return std::ranges::any_of(
        optionValues_, [](const auto& value) { return value.has_value(); });
  }

  // The referenced profiles, files, roles, and destinations must already be
  // available to the job user. Setting their names does not grant access.
  auto injectEnvironment(spank_t spank) const -> bool {
    for (size_t index = 0; index < CONFIG_MAPPINGS.size(); ++index) {
      const auto& mapping = CONFIG_MAPPINGS[index];
      if (optionValues_[index].has_value()) {
        if (spank_setenv(spank, mapping.environment,
                         optionValues_[index]->c_str(), 1) != ESPANK_SUCCESS) {
          logFailure("failed to inject an Amazon Braket job option");
          return false;
        }
        continue;
      }
      const auto submittedValue = getJobEnvironment(spank, mapping.environment);
      if (submittedValue.state == JobEnvironmentState::Invalid) {
        logFailure(
            "Amazon Braket job environment value is malformed or too long");
        return false;
      }
      if (submittedValue.state == JobEnvironmentState::Valid ||
          !plugstackValues_[index].has_value()) {
        continue;
      }
      const auto result = spank_setenv(spank, mapping.environment,
                                       plugstackValues_[index]->c_str(), 0);
      if (result != ESPANK_SUCCESS && result != ESPANK_ENV_EXISTS) {
        logFailure("failed to inject an Amazon Braket administrator default");
        return false;
      }
    }
    return true;
  }

private:
  static auto optionCallback(int value, const char* argument, int remote)
      -> int;

  std::array<std::optional<std::string>, CONFIG_MAPPINGS.size()>
      plugstackValues_{};
  std::array<std::optional<std::string>, CONFIG_MAPPINGS.size()>
      optionValues_{};
  std::array<spank_option, CONFIG_MAPPINGS.size()> optionDefinitions_{};
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
  } catch (const std::exception&) {
    logFailure("Amazon Braket option processing failed");
    return -1;
  } catch (...) {
    logFailure("Amazon Braket option processing failed");
    return -1;
  }
  return 0;
}
} // namespace

// These names and signatures are required by the Slurm SPANK ABI.
// NOLINTBEGIN(misc-use-internal-linkage, readability-identifier-naming,
//             cppcoreguidelines-avoid-c-arrays)
extern "C" {
int slurm_spank_init(spank_t spank, const int count, char* arguments[]) {
  try {
    config.parsePlugstackArguments(count, arguments);
    return config.registerOptions(spank);
  } catch (const std::exception&) {
    logFailure("Amazon Braket SPANK initialization failed");
  } catch (...) {
    logFailure("Amazon Braket SPANK initialization failed");
  }
  return ESPANK_ERROR;
}

int slurm_spank_task_init(spank_t spank, int /*count*/, char* /*arguments*/[]) {
  if (spank_remote(spank) != 1) {
    return ESPANK_SUCCESS;
  }
  try {
    // SLURM_JOB_LICENSES is process-mutable. It controls plugin applicability
    // and consistency checks only; it is not allocation attestation or AWS
    // authorization.
    const auto licenses = getJobEnvironment(spank, LICENSE_ENVIRONMENT);
    if (licenses.state == JobEnvironmentState::Invalid) {
      logFailure("SLURM_JOB_LICENSES is malformed or too long to validate");
      return TASK_REJECTED;
    }
    const auto context = classifyBraketLicense(
        licenses.state == JobEnvironmentState::Valid ? licenses.value : "");
    if (context == BraketLicenseContext::Invalid) {
      logFailure("amazon.braket.default is not a Slurm device license");
      return TASK_REJECTED;
    }
    if (context == BraketLicenseContext::NotApplicable) {
      if (config.optionsPresent()) {
        logFailure(
            "Amazon Braket options require a concrete amazon.braket.* license");
        return TASK_REJECTED;
      }
      return ESPANK_SUCCESS;
    }
    return config.injectEnvironment(spank) ? ESPANK_SUCCESS : TASK_REJECTED;
  } catch (const std::exception&) {
    logFailure("Amazon Braket environment injection failed");
  } catch (...) {
    logFailure("Amazon Braket environment injection failed");
  }
  return TASK_REJECTED;
}
} // extern "C"

// NOLINTEND(misc-use-internal-linkage, readability-identifier-naming,
//           cppcoreguidelines-avoid-c-arrays)
