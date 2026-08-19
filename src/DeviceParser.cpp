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
 * @brief Implementation of the common gate-model capability parser.
 *
 * The common Amazon Braket paradigm and OpenQASM schemas define sites,
 * connectivity, native operations, and executable operations. Optional
 * provider enrichers add calibration data without changing that pipeline.
 */

#include "amazon-braket-qdmi-device/DeviceParser.hpp"

#include "amazon_braket_qdmi/device.h"

#include <algorithm>
#include <array>
#include <aws/braket/model/DeviceType.h>
#include <aws/core/utils/json/JsonSerializer.h>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

// ============================================================================
// Helper Functions (Common to All Parsers)
// ============================================================================

auto amazon::braket::qdmi::parseMeasurementResults(
    const Aws::Utils::Array<Aws::Utils::Json::JsonView>& measurements)
    -> std::vector<std::string> {
  std::vector<std::string> results;
  results.reserve(measurements.GetLength());
  for (size_t i = 0; i < measurements.GetLength(); ++i) {
    const auto shot = measurements[i].AsArray();
    std::string bitstring;

    // QDMI bit strings use conventional basis-state order: the
    // highest-index site is the left-most bit.
    for (size_t q = shot.GetLength(); q > 0; --q) {
      bitstring += std::to_string(shot[q - 1].AsInteger());
    }

    results.emplace_back(std::move(bitstring));
  }
  return results;
}

namespace {
using OperationSignature =
    std::pair<std::optional<size_t>, std::optional<size_t>>;

auto isDecimalSiteName(const std::string_view name) -> bool {
  return !name.empty() &&
         std::ranges::all_of(name, [](const unsigned char character) {
           return std::isdigit(character) != 0;
         });
}

auto decimalSiteIndex(const std::string_view name) -> std::optional<size_t> {
  if (!isDecimalSiteName(name)) {
    return std::nullopt;
  }
  size_t index = 0;
  const auto [end, error] =
      std::from_chars(name.data(), name.data() + name.size(), index);
  if (error != std::errc{} || end != name.data() + name.size()) {
    return std::nullopt;
  }
  return index;
}

struct SiteNameLess {
  auto operator()(const std::string& left, const std::string& right) const
      -> bool {
    const auto leftIndex = decimalSiteIndex(left);
    const auto rightIndex = decimalSiteIndex(right);
    if (leftIndex.has_value() && rightIndex.has_value() &&
        *leftIndex != *rightIndex) {
      return *leftIndex < *rightIndex;
    }
    if (leftIndex.has_value() != rightIndex.has_value()) {
      return leftIndex.has_value();
    }
    return left < right;
  }
};

auto secondsToNanoseconds(const double seconds) -> std::optional<uint64_t> {
  constexpr double nanosecondsPerSecond = 1'000'000'000.0;
  const double nanoseconds = seconds * nanosecondsPerSecond;
  if (!std::isfinite(nanoseconds) || nanoseconds <= 0.0 ||
      nanoseconds > static_cast<double>(std::numeric_limits<uint64_t>::max())) {
    return std::nullopt;
  }
  return static_cast<uint64_t>(std::llround(nanoseconds));
}

auto getOperationSignature(const std::string& operationName)
    -> OperationSignature {
  using namespace std::string_view_literals;
  static constexpr std::array SIGNATURES{
      std::pair{"cc_prx"sv, OperationSignature{1, std::nullopt}},
      std::pair{"ccnot"sv, OperationSignature{3, 0}},
      std::pair{"cnot"sv, OperationSignature{2, 0}},
      std::pair{"cphaseshift"sv, OperationSignature{2, 1}},
      std::pair{"cphaseshift00"sv, OperationSignature{2, 1}},
      std::pair{"cphaseshift01"sv, OperationSignature{2, 1}},
      std::pair{"cphaseshift10"sv, OperationSignature{2, 1}},
      std::pair{"cswap"sv, OperationSignature{3, 0}},
      std::pair{"cv"sv, OperationSignature{2, 0}},
      std::pair{"cy"sv, OperationSignature{2, 0}},
      std::pair{"cz"sv, OperationSignature{2, 0}},
      std::pair{"ecr"sv, OperationSignature{2, 0}},
      std::pair{"gphase"sv, OperationSignature{0, 1}},
      std::pair{"gpi"sv, OperationSignature{1, 1}},
      std::pair{"gpi2"sv, OperationSignature{1, 1}},
      std::pair{"h"sv, OperationSignature{1, 0}},
      std::pair{"i"sv, OperationSignature{1, 0}},
      std::pair{"iswap"sv, OperationSignature{2, 0}},
      std::pair{"measure_ff"sv, OperationSignature{1, std::nullopt}},
      std::pair{"ms"sv, OperationSignature{2, 3}},
      std::pair{"phaseshift"sv, OperationSignature{1, 1}},
      std::pair{"prx"sv, OperationSignature{1, 2}},
      std::pair{"pswap"sv, OperationSignature{2, 1}},
      std::pair{"rx"sv, OperationSignature{1, 1}},
      std::pair{"ry"sv, OperationSignature{1, 1}},
      std::pair{"rz"sv, OperationSignature{1, 1}},
      std::pair{"s"sv, OperationSignature{1, 0}},
      std::pair{"si"sv, OperationSignature{1, 0}},
      std::pair{"swap"sv, OperationSignature{2, 0}},
      std::pair{"t"sv, OperationSignature{1, 0}},
      std::pair{"ti"sv, OperationSignature{1, 0}},
      std::pair{"U"sv, OperationSignature{1, 3}},
      std::pair{"unitary"sv, OperationSignature{std::nullopt, std::nullopt}},
      std::pair{"v"sv, OperationSignature{1, 0}},
      std::pair{"vi"sv, OperationSignature{1, 0}},
      std::pair{"x"sv, OperationSignature{1, 0}},
      std::pair{"xx"sv, OperationSignature{2, 1}},
      std::pair{"xy"sv, OperationSignature{2, 1}},
      std::pair{"y"sv, OperationSignature{1, 0}},
      std::pair{"yy"sv, OperationSignature{2, 1}},
      std::pair{"z"sv, OperationSignature{1, 0}},
      std::pair{"zz"sv, OperationSignature{2, 1}},
  };

  const auto* const signature =
      std::find_if(SIGNATURES.data(), SIGNATURES.data() + SIGNATURES.size(),
                   [&operationName](const auto& entry) {
                     return entry.first == operationName;
                   });
  if (signature == SIGNATURES.data() + SIGNATURES.size()) {
    return {};
  }
  return signature->second;
}
} // namespace

auto GateModelCapabilityParser::parseQubitCount(
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

  const auto count = paradigm.GetInteger("qubitCount");
  if (count <= 0) {
    std::cerr << "Invalid 'qubitCount' in paradigm\n";
    return QDMI_ERROR_FATAL;
  }
  qubitCount = static_cast<size_t>(count);
  return QDMI_SUCCESS;
}

auto GateModelCapabilityParser::buildFullConnectivity(
    ParsedDeviceProperties& properties) -> void {
  properties.connectivity.clear();
  if (properties.qubitCount < 2) {
    return;
  }
  properties.connectivity.reserve(properties.qubitCount *
                                  (properties.qubitCount - 1) * 2);

  for (size_t i = 0; i < properties.qubitCount; ++i) {
    for (size_t j = i + 1; j < properties.qubitCount; ++j) {
      properties.connectivity.push_back(properties.sitesPtr[i]);
      properties.connectivity.push_back(properties.sitesPtr[j]);
      properties.connectivity.push_back(properties.sitesPtr[j]);
      properties.connectivity.push_back(properties.sitesPtr[i]);
    }
  }
}

auto GateModelCapabilityParser::parseSitesAndConnectivity(
    const Aws::Braket::Model::DeviceType deviceType,
    const Aws::Utils::Json::JsonView& propertiesJson,
    ParsedDeviceProperties& properties) -> int {
  const auto paradigm = propertiesJson.GetObject("paradigm");
  const bool simulator =
      deviceType == Aws::Braket::Model::DeviceType::SIMULATOR;
  bool fullyConnected = simulator;
  if (paradigm.ValueExists("connectivity")) {
    const auto connectivity = paradigm.GetObject("connectivity");
    if (connectivity.ValueExists("fullyConnected")) {
      fullyConnected = connectivity.GetBool("fullyConnected");
    }
  } else if (!simulator) {
    std::cerr << "Missing 'connectivity' in gate-model QPU paradigm\n";
    return QDMI_ERROR_FATAL;
  }

  properties.sites.reserve(properties.qubitCount);
  properties.sitesPtr.reserve(properties.qubitCount);
  auto addSite = [&properties](const std::string& name, const size_t index) {
    auto site = std::make_unique<AMAZON_BRAKET_QDMI_Site_impl_d>();
    site->name_ = name;
    site->id_ = index;
    properties.sitesPtr.push_back(site.get());
    properties.sitesMap.emplace(name, site.get());
    properties.sites.push_back(std::move(site));
  };

  if (fullyConnected) {
    for (size_t index = 0; index < properties.qubitCount; ++index) {
      addSite(std::to_string(index), index);
    }
    buildFullConnectivity(properties);
    return QDMI_SUCCESS;
  }

  const auto connectivity = paradigm.GetObject("connectivity");
  if (!connectivity.ValueExists("connectivityGraph")) {
    std::cerr << "Missing 'connectivityGraph' in gate-model paradigm\n";
    return QDMI_ERROR_FATAL;
  }
  const auto graph = connectivity.GetObject("connectivityGraph");

  std::set<std::string, SiteNameLess> siteNames;
  std::map<std::string, std::vector<std::string>> edges;
  for (const auto& [source, targetsView] : graph.GetAllObjects()) {
    const std::string sourceName = source;
    siteNames.insert(sourceName);
    auto& targets = edges[sourceName];
    const auto targetsJson = targetsView.AsArray();
    targets.reserve(targetsJson.GetLength());
    for (size_t index = 0; index < targetsJson.GetLength(); ++index) {
      auto target = std::string{targetsJson.GetItem(index).AsString()};
      siteNames.insert(target);
      targets.push_back(std::move(target));
    }
  }

  if (siteNames.size() != properties.qubitCount) {
    std::cerr << "Connectivity graph contains " << siteNames.size()
              << " sites, but paradigm.qubitCount is " << properties.qubitCount
              << "\n";
    return QDMI_ERROR_FATAL;
  }
  // Decimal provider labels are the physical indices used in programs. Reserve
  // all of them first so that nonnumeric labels can receive deterministic,
  // collision-free fallback indices. Numeric aliases such as "1" and "01"
  // would otherwise identify the same program site and are rejected.
  std::set<size_t> assignedIndices;
  for (const auto& name : siteNames) {
    const auto index = decimalSiteIndex(name);
    if (isDecimalSiteName(name) && !index.has_value()) {
      std::cerr << "Numeric site label is outside the size_t range: " << name
                << "\n";
      return QDMI_ERROR_FATAL;
    }
    if (index.has_value() && !assignedIndices.insert(*index).second) {
      std::cerr << "Connectivity graph contains duplicate numeric site index "
                << *index << "\n";
      return QDMI_ERROR_FATAL;
    }
  }

  size_t fallbackIndex = 0;
  for (const auto& name : siteNames) {
    auto index = decimalSiteIndex(name);
    if (!index.has_value()) {
      while (assignedIndices.contains(fallbackIndex)) {
        if (fallbackIndex == std::numeric_limits<size_t>::max()) {
          std::cerr << "No unused site index is available\n";
          return QDMI_ERROR_FATAL;
        }
        ++fallbackIndex;
      }
      index = fallbackIndex;
      assignedIndices.insert(fallbackIndex);
    }
    addSite(name, *index);
  }

  for (const auto& [source, targets] : edges) {
    const auto sourceSite = properties.sitesMap.find(source);
    if (sourceSite == properties.sitesMap.end()) {
      return QDMI_ERROR_FATAL;
    }
    for (const auto& target : targets) {
      const auto targetSite = properties.sitesMap.find(target);
      if (targetSite == properties.sitesMap.end()) {
        return QDMI_ERROR_FATAL;
      }
      properties.connectivity.insert(properties.connectivity.end(),
                                     {sourceSite->second, targetSite->second});
    }
  }
  return QDMI_SUCCESS;
}

auto GateModelCapabilityParser::parseOperations(
    const Aws::Braket::Model::DeviceType deviceType,
    const Aws::Utils::Json::JsonView& propertiesJson,
    ParsedDeviceProperties& properties) -> int {
  if (!propertiesJson.ValueExists("action")) {
    std::cerr << "Missing 'action' field in device properties\n";
    return QDMI_ERROR_FATAL;
  }
  const auto action = propertiesJson.GetObject("action");
  if (!action.ValueExists("braket.ir.openqasm.program")) {
    std::cerr << "Missing 'braket.ir.openqasm.program' in action\n";
    return QDMI_ERROR_FATAL;
  }
  const auto openqasm = action.GetObject("braket.ir.openqasm.program");
  if (!openqasm.ValueExists("supportedOperations")) {
    std::cerr << "Missing 'supportedOperations' in OpenQASM program\n";
    return QDMI_ERROR_FATAL;
  }

  auto readOperationNames = [](const auto& array) {
    std::vector<std::string> names;
    names.reserve(array.GetLength());
    for (size_t index = 0; index < array.GetLength(); ++index) {
      names.emplace_back(array.GetItem(index).AsString());
    }
    return names;
  };

  const auto supportedNames =
      readOperationNames(openqasm.GetArray("supportedOperations"));
  std::vector<std::string> nativeNames;
  if (deviceType == Aws::Braket::Model::DeviceType::QPU) {
    const auto paradigm = propertiesJson.GetObject("paradigm");
    if (!paradigm.ValueExists("nativeGateSet")) {
      std::cerr << "Missing 'nativeGateSet' in gate-model QPU paradigm\n";
      return QDMI_ERROR_FATAL;
    }
    nativeNames = readOperationNames(paradigm.GetArray("nativeGateSet"));
  } else {
    nativeNames = supportedNames;
  }

  properties.operations.reserve(nativeNames.size() + supportedNames.size());
  properties.allOperationsPtr.reserve(nativeNames.size() +
                                      supportedNames.size());
  auto operationForName = [&properties](const std::string& name) {
    if (const auto existing = properties.operationsMap.find(name);
        existing != properties.operationsMap.end()) {
      return existing->second;
    }
    auto operation = std::make_unique<AMAZON_BRAKET_QDMI_Operation_impl_d>();
    operation->name_ = name;
    const auto [numQubits, numParams] = getOperationSignature(name);
    operation->numQubits_ = numQubits;
    operation->numParams_ = numParams;
    auto* const handle = operation.get();
    properties.operationsMap.emplace(name, handle);
    properties.allOperationsPtr.push_back(handle);
    properties.operations.push_back(std::move(operation));
    return handle;
  };
  auto appendView =
      [&operationForName](
          const std::vector<std::string>& names,
          std::vector<AMAZON_BRAKET_QDMI_Operation_impl_d*>& view) {
        view.reserve(names.size());
        for (const auto& name : names) {
          auto* const operation = operationForName(name);
          if (std::ranges::find(view, operation) == view.end()) {
            view.push_back(operation);
          }
        }
      };
  appendView(nativeNames, properties.operationsPtr);
  appendView(supportedNames, properties.supportedOperationsPtr);

  populateOperationSites(properties);
  parseOperationFidelities(propertiesJson, properties);
  return QDMI_SUCCESS;
}

auto GateModelCapabilityParser::parseProperties(
    const Aws::Braket::Model::DeviceType deviceType,
    const std::string& propertiesJsonStr,
    ParsedDeviceProperties& properties) const -> int {
  if (deviceType != Aws::Braket::Model::DeviceType::QPU &&
      deviceType != Aws::Braket::Model::DeviceType::SIMULATOR) {
    std::cerr << "Unsupported non-gate-model Braket device type\n";
    return QDMI_ERROR_NOTSUPPORTED;
  }
  const Aws::Utils::Json::JsonValue jsonValue(propertiesJsonStr);
  if (!jsonValue.WasParseSuccessful()) {
    std::cerr << "Failed to parse device properties JSON\n";
    return QDMI_ERROR_FATAL;
  }

  properties = {};
  const auto propertiesJson = jsonValue.View();
  if (auto status = parseQubitCount(propertiesJson, properties.qubitCount);
      status != QDMI_SUCCESS) {
    return status;
  }
  if (auto status =
          parseSitesAndConnectivity(deviceType, propertiesJson, properties);
      status != QDMI_SUCCESS) {
    return status;
  }
  if (auto status = parseOperations(deviceType, propertiesJson, properties);
      status != QDMI_SUCCESS) {
    return status;
  }
  for (const auto& enrich : calibrationEnrichers_) {
    enrich(propertiesJson, properties);
  }
  return QDMI_SUCCESS;
}

auto GateModelCapabilityParser::populateOperationSites(
    ParsedDeviceProperties& properties) -> void {
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
    if (operation->numQubits_ == 3) {
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

auto GateModelCapabilityParser::parseOperationFidelities(
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
    const std::string sitePairString = sitePair;
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
      std::string gateName = fidelity.GetString("gateName");
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
          {.sites = {firstSite->second, secondSite->second}, .value = value});
      if (!directed && firstSite->second != secondSite->second) {
        operation->second->siteFidelities_.push_back(
            {.sites = {secondSite->second, firstSite->second}, .value = value});
      }
    }
  }
}

auto GateModelCapabilityParser::enrichIqmCalibration(
    const Aws::Utils::Json::JsonView& propertiesJson,
    ParsedDeviceProperties& properties) -> void {
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

  for (const auto& entry : oneQubit.GetAllObjects()) {
    const std::string& qubitId = entry.first;
    const auto it = properties.sitesMap.find(qubitId);
    if (it == properties.sitesMap.end()) {
      continue;
    }
    auto* site = it->second;
    const auto& qubitData = entry.second;
    if (qubitData.ValueExists("T1")) {
      site->t1_ = secondsToNanoseconds(qubitData.GetDouble("T1"));
    }
    if (qubitData.ValueExists("T2")) {
      site->t2_ = secondsToNanoseconds(qubitData.GetDouble("T2"));
    }
  }
}
