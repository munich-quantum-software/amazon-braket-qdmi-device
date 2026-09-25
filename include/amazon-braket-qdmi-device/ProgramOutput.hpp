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

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Aws::Utils::Json {
class JsonView;
}

namespace amazon::braket::qdmi {

struct OutputBit {
  std::optional<size_t> qubit;
  std::optional<int> value;
};

struct OutputVariable {
  std::string name;
  bool boolean = false;
  bool array = false;
  std::vector<OutputBit> bits;
};

struct ProgramOutput {
  std::string source;
  std::vector<OutputVariable> variables;
  bool implicitMeasurement = false;
  bool qasm3 = true;
};

struct MeasurementResults {
  std::vector<std::string> shots;
  std::string output;
  bool binary = true;
};

/// Parse the supported terminal-measurement subset and prepare Braket source.
/// Throws std::invalid_argument for unsupported program features.
auto prepareProgram(std::string_view source) -> ProgramOutput;

/// Reconstruct selected final values using the response's measuredQubits
/// columns. Throws std::invalid_argument for malformed or inconsistent backend
/// results.
auto parseMeasurementResults(const Aws::Utils::Json::JsonView& result,
                             const ProgramOutput& program, size_t shots)
    -> MeasurementResults;

} // namespace amazon::braket::qdmi
