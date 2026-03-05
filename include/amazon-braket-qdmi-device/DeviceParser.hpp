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
#include <aws/core/utils/json/JsonSerializer.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// Forward declarations for internal implementation types
struct AMAZON_BRAKET_QDMI_Site_impl_d;
struct AMAZON_BRAKET_QDMI_Operation_impl_d;

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
  std::vector<AMAZON_BRAKET_QDMI_Operation_impl_d*> operationsPtr;
  std::unordered_map<std::string, AMAZON_BRAKET_QDMI_Operation_impl_d*>
      operationsMap;
};

/**
 * @brief Abstract interface for device properties parsers
 *
 * AWS Braket provides device capabilities through GetDeviceCapabilities() API.
 *
 * This interface allows implementing parsers for different device types and
 * vendors, each handling their specific JSON format while reusing common
 * components where possible.
 */
class IDeviceParser {
public:
  virtual ~IDeviceParser() = default;

  /**
   * @brief Parse device properties from JSON
   *
   * @param propertiesJson The JSON view of the device properties
   * @param properties Output structure to populate with parsed data
   * @return QDMI_SUCCESS on success, error code otherwise
   */
  virtual auto ParseProperties(const Aws::Utils::Json::JsonView& propertiesJson,
                               ParsedDeviceProperties& properties) -> int = 0;

protected:
  // ============================================================================
  // Reusable Component Parsers
  // ============================================================================
  // These static helpers can be used by any parser implementation as the
  // JSON format is standardized across providers for specific components.

  /**
   * @brief Parse operations from standard OpenQASM action schema
   *
   * Parses: action.braket.ir.openqasm.program.supportedOperations
   * Reusable across devices that use this standard format.
   */
  static auto
  ParseOperationsFromOpenQASM(const Aws::Utils::Json::JsonView& propertiesJson,
                              ParsedDeviceProperties& properties) -> int;

  /**
   * @brief Parse qubit count from standard paradigm field
   *
   * Parses: paradigm.qubitCount
   * Works for gate-model devices with standard paradigm structure.
   */
  static auto ParseQubitCount(const Aws::Utils::Json::JsonView& propertiesJson,
                              size_t& qubitCount) -> int;

  /**
   * @brief Check if device has full connectivity from paradigm
   *
   * Parses: paradigm.connectivity.fullyConnected
   * Returns true for all-to-all connectivity, false for limited connectivity.
   */
  static auto
  HasFullConnectivity(const Aws::Utils::Json::JsonView& propertiesJson,
                      bool& fullyConnected) -> int;

  /**
   * @brief Build full all-to-all connectivity graph
   *
   * Creates bidirectional edges between all qubit pairs.
   * Use when paradigm.connectivity.fullyConnected == true.
   */
  static auto BuildFullConnectivity(ParsedDeviceProperties& properties) -> int;

  /**
   * @brief Helper to determine the number of qubits for a gate
   */
  static auto GetGateQubitCount(const std::string& gateName) -> size_t;
};

/**
 * @brief Parser for Amazon Braket simulator devices
 *
 * Handles simulator-type devices with full connectivity.
 */
class SimulatorPropertiesParser : public IDeviceParser {
public:
  auto ParseProperties(const Aws::Utils::Json::JsonView& propertiesJson,
                       ParsedDeviceProperties& properties) -> int override;
};

/**
 * @brief Parser for IQM quantum devices
 *
 * Handles IQM-specific format including string-based qubit IDs
 * and custom connectivity graph structure.
 */
class IQMDeviceParser : public IDeviceParser {
public:
  auto ParseProperties(const Aws::Utils::Json::JsonView& propertiesJson,
                       ParsedDeviceProperties& properties) -> int override;

private:
  /**
   * @brief Parse IQM-specific connectivity graph
   */
  static auto ParseConnectivityGraph(const Aws::Utils::Json::JsonView& paradigm,
                                     ParsedDeviceProperties& properties) -> int;

  /**
   * @brief Populate T1/T2 coherence times from provider calibration data
   *
   * Reads provider.properties.one_qubit.<qubitId>.T1 and .T2 (seconds)
   * and stores them as microseconds in site->t1_ and site->t2_.
   */
  static auto
  ParseSiteCoherenceTimes(const Aws::Utils::Json::JsonView& propertiesJson,
                          ParsedDeviceProperties& properties) -> void;
};

/**
 * @brief Factory function to create the appropriate parser
 *
 * @param deviceType DeviceType enum from AWS Braket
 * @param provider Provider name from device capabilities
 * @return Parser instance, or nullptr if unsupported
 */
auto CreateDeviceParser(const Aws::Braket::Model::DeviceType& deviceType,
                        const std::string& provider)
    -> std::unique_ptr<IDeviceParser>;
