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

#pragma once

#include <aws/braket/model/DeviceType.h>
#include <cstddef>
#include <cstdint>

// Forward declaration to avoid pulling aws-cpp-sdk-core headers into this
// public header. Implementations include the full header in their .cpp file.
namespace Aws {
namespace Utils {
template <typename T> class Array;
namespace Json {
class JsonView;
} // namespace Json
} // namespace Utils
} // namespace Aws
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

/**
 * @brief Site implementation structure.
 */
struct AMAZON_BRAKET_QDMI_Site_impl_d {
  std::string name_;
  size_t id_ = 0; ///< Site index used to address the qubit in a program
  std::optional<uint64_t> t1_; ///< T1 coherence time in nanoseconds
  std::optional<uint64_t> t2_; ///< T2 coherence time in nanoseconds
};

/**
 * @brief Operation implementation structure.
 */
struct AMAZON_BRAKET_QDMI_Operation_impl_d {
  struct SiteFidelity {
    std::vector<AMAZON_BRAKET_QDMI_Site_impl_d*> sites;
    double value = 0.0;
  };

  std::string name_;
  std::optional<size_t> numQubits_;
  std::optional<size_t> numParams_;
  std::vector<AMAZON_BRAKET_QDMI_Site_impl_d*> applicableSites_;
  std::vector<SiteFidelity> siteFidelities_;
};

/**
 * @brief Parsed device properties data structure
 *
 * This struct holds all the parsed information from device properties JSON.
 */
struct ParsedDeviceProperties {
  size_t qubitCount{0};

  std::vector<std::unique_ptr<AMAZON_BRAKET_QDMI_Site_impl_d>> sites;
  std::vector<AMAZON_BRAKET_QDMI_Site_impl_d*> sitesPtr;
  std::unordered_map<std::string, AMAZON_BRAKET_QDMI_Site_impl_d*> sitesMap;

  std::vector<AMAZON_BRAKET_QDMI_Site_impl_d*> connectivity;

  std::vector<std::unique_ptr<AMAZON_BRAKET_QDMI_Operation_impl_d>> operations;
  std::vector<AMAZON_BRAKET_QDMI_Operation_impl_d*> allOperationsPtr;
  std::vector<AMAZON_BRAKET_QDMI_Operation_impl_d*> operationsPtr;
  std::vector<AMAZON_BRAKET_QDMI_Operation_impl_d*> supportedOperationsPtr;
  std::unordered_map<std::string, AMAZON_BRAKET_QDMI_Operation_impl_d*>
      operationsMap;
};

namespace amazon::braket::qdmi {

/**
 * @brief Convert Amazon Braket measurement rows to QDMI basis-state order.
 */
auto parseMeasurementResults(
    const Aws::Utils::Array<Aws::Utils::Json::JsonView>& measurements)
    -> std::vector<std::string>;

} // namespace amazon::braket::qdmi

/**
 * @brief Common parser for Amazon Braket gate-model device capabilities.
 */
class GateModelCapabilityParser final {
public:
  using CalibrationEnricher = std::function<void(
      const Aws::Utils::Json::JsonView&, ParsedDeviceProperties&)>;

  explicit GateModelCapabilityParser(
      std::vector<CalibrationEnricher> calibrationEnrichers = {})
      : calibrationEnrichers_(std::move(calibrationEnrichers)) {}

  /**
   * @brief Parse a gate-model capability document.
   *
   * QPUs expose their standard operation list from `nativeGateSet` and their
   * broader operation list from the OpenQASM action. On-demand simulators use
   * the OpenQASM list for both views because they do not publish a hardware
   * native gate set.
   */
  auto parseProperties(Aws::Braket::Model::DeviceType deviceType,
                       const std::string& propertiesJson,
                       ParsedDeviceProperties& properties) const -> int;

  /** @brief Add IQM T1 and T2 values when the provider schema contains them. */
  static auto
  enrichIqmCalibration(const Aws::Utils::Json::JsonView& propertiesJson,
                       ParsedDeviceProperties& properties) -> void;

private:
  static auto parseQubitCount(const Aws::Utils::Json::JsonView& propertiesJson,
                              size_t& qubitCount) -> int;
  static auto
  parseSitesAndConnectivity(Aws::Braket::Model::DeviceType deviceType,
                            const Aws::Utils::Json::JsonView& propertiesJson,
                            ParsedDeviceProperties& properties) -> int;
  static auto buildFullConnectivity(ParsedDeviceProperties& properties) -> void;
  static auto parseOperations(Aws::Braket::Model::DeviceType deviceType,
                              const Aws::Utils::Json::JsonView& propertiesJson,
                              ParsedDeviceProperties& properties) -> int;
  static auto populateOperationSites(ParsedDeviceProperties& properties)
      -> void;
  static auto
  parseOperationFidelities(const Aws::Utils::Json::JsonView& propertiesJson,
                           ParsedDeviceProperties& properties) -> void;
  std::vector<CalibrationEnricher> calibrationEnrichers_;
};
