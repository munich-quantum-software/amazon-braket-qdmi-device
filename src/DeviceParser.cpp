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
 * @file device_parser.cpp
 * @brief Implementation of device properties parsers for different providers
 *
 * This file contains parser implementations for:
 * - Amazon Braket Simulators (AWS SV1, AWS DM1, AWS TN1)
 * - IQM Devices (IQM Garnet, IQM Emerald, etc.)
 *
 * Each parser handles the provider-specific JSON format and populates
 * the device architecture data (qubits, connectivity, operations).
 */

#include "amazon-braket-qdmi-device/DeviceParser.hpp"

#include "amazon_braket_qdmi/device.h"

#include <algorithm>
#include <array>
#include <aws/core/utils/json/JsonSerializer.h>
#include <cctype>
#include <cstddef>
#include <iostream>
#include <memory>
#include <optional>
#include <ranges>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// ============================================================================
// Helper Functions (Common to All Parsers)
// ============================================================================

auto IDeviceParser::GetOperationSignature(const std::string& operationName)
    -> std::optional<std::pair<size_t, size_t>> {
  using namespace std::string_view_literals;
  using Signature = std::pair<size_t, size_t>;
  static constexpr std::array signatures{
      std::pair{"ccnot"sv, Signature{3, 0}},
      std::pair{"cnot"sv, Signature{2, 0}},
      std::pair{"cphaseshift"sv, Signature{2, 1}},
      std::pair{"cphaseshift00"sv, Signature{2, 1}},
      std::pair{"cphaseshift01"sv, Signature{2, 1}},
      std::pair{"cphaseshift10"sv, Signature{2, 1}},
      std::pair{"cswap"sv, Signature{3, 0}},
      std::pair{"cy"sv, Signature{2, 0}},
      std::pair{"cz"sv, Signature{2, 0}},
      std::pair{"ecr"sv, Signature{2, 0}},
      std::pair{"gpi"sv, Signature{1, 1}},
      std::pair{"gpi2"sv, Signature{1, 1}},
      std::pair{"h"sv, Signature{1, 0}},
      std::pair{"i"sv, Signature{1, 0}},
      std::pair{"iswap"sv, Signature{2, 0}},
      std::pair{"ms"sv, Signature{2, 3}},
      std::pair{"phaseshift"sv, Signature{1, 1}},
      std::pair{"prx"sv, Signature{1, 2}},
      std::pair{"pswap"sv, Signature{2, 1}},
      std::pair{"rx"sv, Signature{1, 1}},
      std::pair{"ry"sv, Signature{1, 1}},
      std::pair{"rz"sv, Signature{1, 1}},
      std::pair{"s"sv, Signature{1, 0}},
      std::pair{"si"sv, Signature{1, 0}},
      std::pair{"swap"sv, Signature{2, 0}},
      std::pair{"t"sv, Signature{1, 0}},
      std::pair{"ti"sv, Signature{1, 0}},
      std::pair{"v"sv, Signature{1, 0}},
      std::pair{"vi"sv, Signature{1, 0}},
      std::pair{"x"sv, Signature{1, 0}},
      std::pair{"xx"sv, Signature{2, 1}},
      std::pair{"xy"sv, Signature{2, 1}},
      std::pair{"y"sv, Signature{1, 0}},
      std::pair{"yy"sv, Signature{2, 1}},
      std::pair{"z"sv, Signature{1, 0}},
      std::pair{"zz"sv, Signature{2, 1}},
      // IQM's experimental feed-forward operations have one quantum operand.
      // Their feedback keys are scalar program arguments and are counted here
      // because QDMI does not distinguish scalar parameter types.
      std::pair{"cc_prx"sv, Signature{1, 3}},
      std::pair{"measure_ff"sv, Signature{1, 1}},
  };

  const auto signature =
      std::ranges::find_if(signatures, [&operationName](const auto& entry) {
        return entry.first == operationName;
      });
  if (signature == signatures.end()) {
    return std::nullopt;
  }
  return signature->second;
}

auto IDeviceParser::ParseQubitCount(
    const Aws::Utils::Json::JsonView& propertiesJson, size_t& qubitCount)
    -> int {

  if (!propertiesJson.ValueExists("paradigm")) {
    std::cerr << "Missing 'paradigm' field in device properties\n";
    return QDMI_ERROR_FATAL;
  }

  auto paradigm = propertiesJson.GetObject("paradigm");
  if (!paradigm.ValueExists("qubitCount")) {
    std::cerr << "Missing 'qubitCount' in paradigm\n";
    return QDMI_ERROR_FATAL;
  }

  qubitCount = static_cast<size_t>(paradigm.GetInteger("qubitCount"));
  return QDMI_SUCCESS;
}

// ============================================================================
// Helper Functions (shared across parsers)
// ============================================================================

auto IDeviceParser::BuildFullConnectivity(ParsedDeviceProperties& properties)
    -> int {
  properties.connectivity.clear();
  // Each unordered pair (i,j) contributes 4 entries (both directions, 2 ptrs
  // each)
  properties.connectivity.reserve(properties.qubitCount *
                                  (properties.qubitCount - 1) * 2);

  // Create bidirectional edges between all pairs of qubits
  // Stored as flat list with alternating source/target per QDMI spec:
  // source at index 2n, target at index 2n+1
  for (size_t i = 0; i < properties.qubitCount; ++i) {
    for (size_t j = i + 1; j < properties.qubitCount; ++j) {
      // Edge i -> j
      properties.connectivity.push_back(properties.sitesPtr[i]);
      properties.connectivity.push_back(properties.sitesPtr[j]);
      // Edge j -> i
      properties.connectivity.push_back(properties.sitesPtr[j]);
      properties.connectivity.push_back(properties.sitesPtr[i]);
    }
  }

  return QDMI_SUCCESS;
}

auto IDeviceParser::ParseOperationsFromOpenQASM(
    const Aws::Utils::Json::JsonView& propertiesJson,
    ParsedDeviceProperties& properties) -> int {

  if (!propertiesJson.ValueExists("action")) {
    std::cerr << "Missing 'action' field in device properties\n";
    return QDMI_ERROR_FATAL;
  }

  auto action = propertiesJson.GetObject("action");
  if (!action.ValueExists("braket.ir.openqasm.program")) {
    std::cerr << "Missing 'braket.ir.openqasm.program' in action\n";
    return QDMI_ERROR_FATAL;
  }

  auto openqasm = action.GetObject("braket.ir.openqasm.program");
  if (!openqasm.ValueExists("supportedOperations")) {
    std::cerr << "Missing 'supportedOperations' in OpenQASM program\n";
    return QDMI_ERROR_FATAL;
  }

  std::vector<std::string> operationNames;
  if (propertiesJson.ValueExists("paradigm")) {
    const auto paradigm = propertiesJson.GetObject("paradigm");
    if (paradigm.ValueExists("nativeGateSet")) {
      const auto nativeGateSet = paradigm.GetArray("nativeGateSet");
      operationNames.reserve(nativeGateSet.GetLength());
      for (size_t i = 0; i < nativeGateSet.GetLength(); ++i) {
        operationNames.emplace_back(nativeGateSet[i].AsString());
      }
    }
  }
  if (operationNames.empty()) {
    const auto supportedOperations = openqasm.GetArray("supportedOperations");
    operationNames.reserve(supportedOperations.GetLength());
    for (size_t i = 0; i < supportedOperations.GetLength(); ++i) {
      operationNames.emplace_back(supportedOperations[i].AsString());
    }
  }

  properties.operations.clear();
  properties.operationsPtr.clear();
  properties.operationsMap.clear();
  properties.operations.reserve(operationNames.size());
  properties.operationsPtr.reserve(operationNames.size());

  for (const auto& operationName : operationNames) {
    const auto signature = GetOperationSignature(operationName);
    if (!signature.has_value()) {
      std::cerr << "Skipping Braket operation with no fixed QDMI signature: "
                << operationName << "\n";
      continue;
    }

    auto op = std::make_unique<AMAZON_BRAKET_QDMI_Operation_impl_d>();
    op->name_ = operationName;
    op->numQubits_ = signature->first;
    op->numParams_ = signature->second;

    properties.operationsPtr.push_back(op.get());
    properties.operationsMap[operationName] = op.get();
    properties.operations.push_back(std::move(op));
  }

  PopulateOperationSites(properties);
  ParseOperationFidelities(propertiesJson, properties);
  return QDMI_SUCCESS;
}

auto IDeviceParser::PopulateOperationSites(ParsedDeviceProperties& properties)
    -> void {
  for (auto& operation : properties.operations) {
    auto& sites = operation->applicableSites_;
    if (operation->numQubits_ == 1) {
      sites = properties.sitesPtr;
      continue;
    }
    if (operation->numQubits_ == 2) {
      sites = properties.connectivity;
      continue;
    }
    const auto numSites = properties.sitesPtr.size();
    const auto completeConnectivitySize =
        numSites < 2 ? 0 : 2 * numSites * (numSites - 1);
    if (operation->numQubits_ == 3 && numSites >= 3 &&
        properties.connectivity.size() == completeConnectivitySize) {
      for (auto* first : properties.sitesPtr) {
        for (auto* second : properties.sitesPtr) {
          if (second == first) {
            continue;
          }
          for (auto* third : properties.sitesPtr) {
            if (third == first || third == second) {
              continue;
            }
            sites.insert(sites.end(), {first, second, third});
          }
        }
      }
    }
  }
}

auto IDeviceParser::ParseOperationFidelities(
    const Aws::Utils::Json::JsonView& propertiesJson,
    ParsedDeviceProperties& properties) -> void {
  if (!propertiesJson.ValueExists("standardized")) {
    return;
  }
  const auto standardized = propertiesJson.GetObject("standardized");
  if (!standardized.ValueExists("twoQubitProperties")) {
    return;
  }

  const auto twoQubitProperties = standardized.GetObject("twoQubitProperties");
  for (const auto& [sitePair, siteData] : twoQubitProperties.GetAllObjects()) {
    if (!siteData.ValueExists("twoQubitGateFidelity")) {
      continue;
    }
    const std::string sitePairString = sitePair.c_str();
    const auto separator = sitePairString.find('-');
    if (separator == std::string::npos) {
      continue;
    }
    const auto defaultFirst = sitePairString.substr(0, separator);
    const auto defaultSecond = sitePairString.substr(separator + 1);
    const auto fidelities = siteData.GetArray("twoQubitGateFidelity");
    for (size_t i = 0; i < fidelities.GetLength(); ++i) {
      const auto fidelity = fidelities[i].AsObject();
      if (!fidelity.ValueExists("gateName") ||
          !fidelity.ValueExists("fidelity")) {
        continue;
      }
      std::string gateName = fidelity.GetString("gateName").c_str();
      std::ranges::transform(gateName, gateName.begin(), [](const char value) {
        return static_cast<char>(
            std::tolower(static_cast<unsigned char>(value)));
      });
      const auto operation = properties.operationsMap.find(gateName);
      if (operation == properties.operationsMap.end()) {
        continue;
      }

      auto first = defaultFirst;
      auto second = defaultSecond;
      const bool directed = fidelity.ValueExists("direction");
      if (directed) {
        const auto direction = fidelity.GetObject("direction");
        if (!direction.ValueExists("control") ||
            !direction.ValueExists("target")) {
          continue;
        }
        first = std::to_string(direction.GetInteger("control"));
        second = std::to_string(direction.GetInteger("target"));
      }

      const auto firstSite = properties.sitesMap.find(first);
      const auto secondSite = properties.sitesMap.find(second);
      if (firstSite == properties.sitesMap.end() ||
          secondSite == properties.sitesMap.end()) {
        continue;
      }

      const auto value = fidelity.GetDouble("fidelity");
      operation->second->siteFidelities_.push_back(
          {{firstSite->second, secondSite->second}, value});
      if (!directed && firstSite->second != secondSite->second) {
        operation->second->siteFidelities_.push_back(
            {{secondSite->second, firstSite->second}, value});
      }
    }
  }
}

// ============================================================================
// Simulator Parser Implementation
// ============================================================================

auto SimulatorPropertiesParser::ParseProperties(
    const std::string& propertiesJsonStr,
    ParsedDeviceProperties& properties) const -> int {
  const Aws::Utils::Json::JsonValue jsonValue(propertiesJsonStr);
  if (!jsonValue.WasParseSuccessful()) {
    std::cerr << "Failed to parse device properties JSON\n";
    return QDMI_ERROR_FATAL;
  }
  const auto propertiesJson = jsonValue.View();

  // 1. Parse qubit count (reusable helper - standard format)
  auto status = ParseQubitCount(propertiesJson, properties.qubitCount);
  if (status != QDMI_SUCCESS) {
    return status;
  }

  // 2. Create sites (qubits) - simulators use simple numeric IDs
  properties.sites.clear();
  properties.sitesPtr.clear();
  properties.sitesMap.clear();
  properties.sites.reserve(properties.qubitCount);
  properties.sitesPtr.reserve(properties.qubitCount);

  for (size_t i = 0; i < properties.qubitCount; ++i) {
    auto site = std::make_unique<AMAZON_BRAKET_QDMI_Site_impl_d>();
    site->id_ = i;
    site->name_ = "Q" + std::to_string(i);

    properties.sitesPtr.push_back(site.get());
    properties.sitesMap[site->name_] = site.get();
    properties.sites.push_back(std::move(site));
  }

  // 3. Build full all-to-all connectivity — simulators support any gate
  // topology regardless of what the capabilities JSON reports.
  status = BuildFullConnectivity(properties);
  if (status != QDMI_SUCCESS) {
    return status;
  }

  // 4. Parse operations (reusable helper - standard OpenQASM format)
  return ParseOperationsFromOpenQASM(propertiesJson, properties);
}

// ============================================================================
// IQM Parser Implementation
// ============================================================================

auto IQMDeviceParser::ParseProperties(const std::string& propertiesJsonStr,
                                      ParsedDeviceProperties& properties) const
    -> int {
  const Aws::Utils::Json::JsonValue jsonValue(propertiesJsonStr);
  if (!jsonValue.WasParseSuccessful()) {
    std::cerr << "Failed to parse device properties JSON\n";
    return QDMI_ERROR_FATAL;
  }
  const auto propertiesJson = jsonValue.View();

  // 1. Parse qubit count (reusable helper - standard format)
  auto status = ParseQubitCount(propertiesJson, properties.qubitCount);
  if (status != QDMI_SUCCESS) {
    return status;
  }

  // 2. Create sites - IQM uses string-based qubit IDs
  // We need to parse the connectivity graph first to know which qubits exist
  properties.sites.clear();
  properties.sitesPtr.clear();
  properties.sitesMap.clear();

  // IQM devices have numbered qubits, but they may not be contiguous
  // Parse connectivity graph to discover all qubit IDs
  if (!propertiesJson.ValueExists("paradigm")) {
    std::cerr << "Missing 'paradigm' field in IQM device properties\n";
    return QDMI_ERROR_FATAL;
  }

  auto const paradigm = propertiesJson.GetObject("paradigm");
  if (!paradigm.ValueExists("connectivity")) {
    std::cerr << "Missing 'connectivity' in IQM paradigm\n";
    return QDMI_ERROR_FATAL;
  }

  const auto connectivityObj = paradigm.GetObject("connectivity");
  if (!connectivityObj.ValueExists("connectivityGraph")) {
    std::cerr << "Missing 'connectivityGraph' in IQM connectivity\n";
    return QDMI_ERROR_FATAL;
  }

  const auto connGraph = connectivityObj.GetObject("connectivityGraph");

  // Collect all unique qubit IDs as integers for deterministic numeric order.
  std::set<size_t> qubitNums;
  for (const auto& entry : connGraph.GetAllObjects()) {
    try {
      qubitNums.insert(std::stoull(entry.first));
      auto targets = entry.second.AsArray();
      for (size_t i = 0; i < targets.GetLength(); ++i) {
        qubitNums.insert(std::stoull(targets[i].AsString()));
      }
    } catch (...) {
      std::cerr << "Invalid qubit ID in connectivity graph\n";
      return QDMI_ERROR_FATAL;
    }
  }

  // Create sites in ascending numeric order
  properties.sites.reserve(qubitNums.size());
  properties.sitesPtr.reserve(qubitNums.size());
  for (const size_t qubitNum : qubitNums) {
    const std::string qubitId = std::to_string(qubitNum);
    auto site = std::make_unique<AMAZON_BRAKET_QDMI_Site_impl_d>();
    site->name_ = qubitId;
    site->id_ = qubitNum;

    properties.sitesPtr.push_back(site.get());
    properties.sitesMap[qubitId] = site.get();
    properties.sites.push_back(std::move(site));
  }

  // 3. Parse connectivity graph (IQM-specific - limited hardware connectivity)
  status = ParseConnectivityGraph(paradigm, properties);
  if (status != QDMI_SUCCESS) {
    return status;
  }

  // 4. Populate T1/T2 coherence times from provider calibration data
  //    (best-effort: missing fields are silently skipped)
  ParseSiteCoherenceTimes(propertiesJson, properties);

  // 5. Parse operations (reusable helper - standard OpenQASM format)
  return ParseOperationsFromOpenQASM(propertiesJson, properties);
}

auto IQMDeviceParser::ParseConnectivityGraph(
    const Aws::Utils::Json::JsonView& paradigm,
    ParsedDeviceProperties& properties) -> int {

  if (!paradigm.ValueExists("connectivity")) {
    return QDMI_ERROR_FATAL;
  }

  const auto connectivityObj = paradigm.GetObject("connectivity");
  if (!connectivityObj.ValueExists("connectivityGraph")) {
    return QDMI_ERROR_FATAL;
  }

  const auto connGraph = connectivityObj.GetObject("connectivityGraph");
  properties.connectivity.clear();

  // IQM connectivity graph format:
  // {
  //   "1": ["2", "5"],
  //   "2": ["1", "6"],
  //   ...
  // }
  // Each entry represents edges from the key qubit to the listed qubits

  for (const auto& entry : connGraph.GetAllObjects()) {
    const std::string& sourceId = entry.first;
    const auto srcIt = properties.sitesMap.find(sourceId);
    if (srcIt == properties.sitesMap.end()) {
      std::cerr << "Invalid source qubit in connectivity graph: " << sourceId
                << "\n";
      return QDMI_ERROR_FATAL;
    }
    auto* const sourceSite = srcIt->second;

    auto const targets = entry.second.AsArray();
    for (size_t i = 0; i < targets.GetLength(); ++i) {
      const std::string targetId = targets[i].AsString();
      const auto tgtIt = properties.sitesMap.find(targetId);
      if (tgtIt == properties.sitesMap.end()) {
        std::cerr << "Invalid target qubit in connectivity graph: " << sourceId
                  << " -> " << targetId << "\n";
        return QDMI_ERROR_FATAL;
      }
      auto* const targetSite = tgtIt->second;

      // Add edge (stored as alternating source/target per QDMI spec)
      properties.connectivity.push_back(sourceSite);
      properties.connectivity.push_back(targetSite);
    }
  }

  return QDMI_SUCCESS;
}

auto IQMDeviceParser::ParseSiteCoherenceTimes(
    const Aws::Utils::Json::JsonView& propertiesJson,
    ParsedDeviceProperties& properties) -> void {
  // Guard: provider.properties.one_qubit must exist
  if (!propertiesJson.ValueExists("provider")) {
    return;
  }
  const auto providerObj = propertiesJson.GetObject("provider");
  if (!providerObj.ValueExists("properties")) {
    return;
  }
  const auto propsObj = providerObj.GetObject("properties");
  if (!propsObj.ValueExists("one_qubit")) {
    return;
  }
  const auto oneQubit = propsObj.GetObject("one_qubit");

  // Each entry key is the qubit ID string ("1", "2", ...)
  // Values are seconds (double); stored directly without conversion.
  for (const auto& entry : oneQubit.GetAllObjects()) {
    const std::string& qubitId = entry.first;
    const auto it = properties.sitesMap.find(qubitId);
    if (it == properties.sitesMap.end()) {
      continue; // qubit not in topology — skip
    }
    auto* site = it->second;
    const auto& qubitData = entry.second;
    if (qubitData.ValueExists("T1")) {
      const double t1Seconds = qubitData.GetDouble("T1");
      if (t1Seconds > 0.0) {
        site->t1_ = t1Seconds;
      }
    }
    if (qubitData.ValueExists("T2")) {
      const double t2Seconds = qubitData.GetDouble("T2");
      if (t2Seconds > 0.0) {
        site->t2_ = t2Seconds;
      }
    }
  }
}
