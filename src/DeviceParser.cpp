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
 * @file DeviceParser.cpp
 * @brief Implementation of device properties parsers for different providers
 *
 * This file contains parser implementations for:
 * - Amazon Braket Simulators (SV1, DM1, TN1)
 * - IQM Quantum Devices (Garnet, Emerald, etc.)
 *
 * Each parser handles the provider-specific JSON format and populates
 * the device architecture data (qubits, connectivity, operations).
 */

#include "amazon-braket-qdmi-device/DeviceParser.hpp"

#include "amazon-braket-qdmi-device/Device.hpp"
#include "amazon_braket_qdmi/constants.h"

#include <aws/braket/model/DeviceType.h>
#include <aws/core/utils/json/JsonSerializer.h>
#include <cstddef>
#include <iostream>
#include <memory>
#include <ostream>
#include <string>
#include <unordered_set>
#include <utility>

// ============================================================================
// Helper Functions (Common to All Parsers)
// ============================================================================

auto IDeviceParser::GetGateQubitCount(const std::string& gateName) -> size_t {
  // Three-qubit gates
  if (gateName == "ccnot" || gateName == "cswap" || gateName == "cc_prx") {
    return 3;
  }

  // Two-qubit gates
  if (gateName == "cnot" || gateName == "cz" || gateName == "swap" ||
      gateName == "xx" || gateName == "yy" || gateName == "zz" ||
      gateName == "xy" || gateName == "cphaseshift" ||
      gateName == "cphaseshift00" || gateName == "cphaseshift01" ||
      gateName == "cphaseshift10" || gateName == "iswap" ||
      gateName == "pswap" || gateName == "ecr" || gateName == "cy" ||
      gateName == "ms" || gateName == "gpi2") {
    return 2;
  }

  // Default: single-qubit gates
  // (x, y, z, h, rx, ry, rz, s, si, t, ti, v, vi, i, phaseshift, gpi,
  // unitary, prx, measure_ff)
  return 1;
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

auto IDeviceParser::HasFullConnectivity(
    const Aws::Utils::Json::JsonView& propertiesJson, bool& fullyConnected)
    -> int {

  if (!propertiesJson.ValueExists("paradigm")) {
    std::cerr << "Missing 'paradigm' field in device properties\n";
    return QDMI_ERROR_FATAL;
  }

  auto paradigm = propertiesJson.GetObject("paradigm");
  if (!paradigm.ValueExists("connectivity")) {
    // Some devices may not have connectivity field - assume not fully connected
    fullyConnected = false;
    return QDMI_SUCCESS;
  }

  auto connectivity = paradigm.GetObject("connectivity");
  if (!connectivity.ValueExists("fullyConnected")) {
    // If fullyConnected field missing, assume false (need explicit graph)
    fullyConnected = false;
    return QDMI_SUCCESS;
  }

  fullyConnected = connectivity.GetBool("fullyConnected");
  return QDMI_SUCCESS;
}

auto IDeviceParser::BuildFullConnectivity(ParsedDeviceProperties& properties)
    -> int {
  properties.connectivity.clear();

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

  auto gateSet = openqasm.GetArray("supportedOperations");
  properties.operations.clear();
  properties.operationsPtr.clear();
  properties.operationsMap.clear();

  for (size_t i = 0; i < gateSet.GetLength(); ++i) {
    const std::string gateName = gateSet[i].AsString();

    auto op = std::make_unique<AMAZON_BRAKET_QDMI_Operation_impl_d>();
    op->name_ = gateName;
    op->numQubits_ = GetGateQubitCount(gateName);

    // Assume perfect fidelity for now (will be overridden for QPUs if needed)
    op->fidelity_ = 1.0;

    properties.operationsPtr.push_back(op.get());
    properties.operationsMap[gateName] = op.get();
    properties.operations.push_back(std::move(op));
  }

  return QDMI_SUCCESS;
}

// ============================================================================
// Simulator Parser Implementation
// ============================================================================

auto SimulatorPropertiesParser::ParseProperties(
    const Aws::Utils::Json::JsonView& propertiesJson,
    ParsedDeviceProperties& properties) -> int {

  // 1. Parse qubit count (reusable helper - standard format)
  auto status = ParseQubitCount(propertiesJson, properties.qubitCount);
  if (status != QDMI_SUCCESS) {
    return status;
  }

  // 2. Create sites (qubits) - simulators use simple numeric IDs
  properties.sites.clear();
  properties.sitesPtr.clear();
  properties.sitesMap.clear();

  for (size_t i = 0; i < properties.qubitCount; ++i) {
    auto site = std::make_unique<AMAZON_BRAKET_QDMI_Site_impl_d>();
    site->id_ = i;
    site->name_ = "Q" + std::to_string(i);

    properties.sitesPtr.push_back(site.get());
    properties.sitesMap[site->name_] = site.get();
    properties.sites.push_back(std::move(site));
  }

  // 3. Build connectivity - simulators always have full connectivity
  //    (We expect paradigm.connectivity.fullyConnected == true, but we
  //     build it regardless since simulators have no physical constraints)
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

auto IQMDeviceParser::ParseProperties(
    const Aws::Utils::Json::JsonView& propertiesJson,
    ParsedDeviceProperties& properties) -> int {

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

  // Collect all unique qubit IDs from the connectivity graph
  std::unordered_set<std::string> qubitIds;
  for (const auto& entry : connGraph.GetAllObjects()) {
    const std::string& sourceId = entry.first;
    qubitIds.insert(sourceId);

    auto targets = entry.second.AsArray();
    for (size_t i = 0; i < targets.GetLength(); ++i) {
      qubitIds.insert(targets[i].AsString());
    }
  }

  // Create sites for all discovered qubits
  for (const auto& qubitId : qubitIds) {
    auto site = std::make_unique<AMAZON_BRAKET_QDMI_Site_impl_d>();
    site->name_ = qubitId;

    // Convert string ID to integer (IQM uses numeric strings like "1", "2",
    // etc.)
    try {
      site->id_ = std::stoull(qubitId);
    } catch (...) {
      std::cerr << "Invalid qubit ID: " << qubitId << "\n";
      return QDMI_ERROR_FATAL;
    }

    properties.sitesPtr.push_back(site.get());
    properties.sitesMap[qubitId] = site.get();
    properties.sites.push_back(std::move(site));
  }

  // 3. Parse connectivity graph (IQM-specific - limited hardware connectivity)
  status = ParseConnectivityGraph(paradigm, properties);
  if (status != QDMI_SUCCESS) {
    return status;
  }

  // 4. Parse operations (reusable helper - standard OpenQASM format)
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
    auto* const sourceSite = properties.sitesMap[sourceId];

    auto const targets = entry.second.AsArray();
    for (size_t i = 0; i < targets.GetLength(); ++i) {
      const std::string targetId = targets[i].AsString();
      auto* const targetSite = properties.sitesMap[targetId];

      if (sourceSite == nullptr || targetSite == nullptr) {
        std::cerr << "Invalid qubit in connectivity graph: " << sourceId
                  << " -> " << targetId << "\n";
        return QDMI_ERROR_FATAL;
      }

      // Add edge (stored as alternating source/target per QDMI spec)
      properties.connectivity.push_back(sourceSite);
      properties.connectivity.push_back(targetSite);
    }
  }

  return QDMI_SUCCESS;
}

// ============================================================================
// Factory Function
// ============================================================================

auto CreateDeviceParser(const Aws::Braket::Model::DeviceType& deviceType,
                        const std::string& provider)
    -> std::unique_ptr<IDeviceParser> {

  if (deviceType == Aws::Braket::Model::DeviceType::SIMULATOR) {
    return std::make_unique<SimulatorPropertiesParser>();
  }

  if (deviceType == Aws::Braket::Model::DeviceType::QPU) {
    if (provider == "IQM") {
      return std::make_unique<IQMDeviceParser>();
    }

    std::cerr << "Unsupported QPU provider: " << provider << "\n";
    return nullptr;
  }

  std::cerr << "Unsupported device type\n";
  return nullptr;
}
