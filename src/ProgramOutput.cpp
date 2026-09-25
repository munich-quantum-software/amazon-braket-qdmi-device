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

#include "amazon-braket-qdmi-device/ProgramOutput.hpp"

#include <algorithm>
#include <aws/core/utils/Array.h>
#include <aws/core/utils/json/JsonSerializer.h>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace amazon::braket::qdmi {
namespace {
using Aws::Utils::Json::JsonValue;

[[noreturn]] auto unsupported() -> void {
  throw std::invalid_argument(
      "Unsupported OpenQASM output or nonterminal measurement program");
}

auto number(const std::string_view token) -> size_t {
  size_t value = 0;
  const auto [end, error] =
      std::from_chars(token.data(), token.data() + token.size(), value);
  if (error != std::errc{} || end != token.data() + token.size() ||
      value > 1'000'000) {
    unsupported();
  }
  return value;
}

auto identifier(const std::string_view token) -> bool {
  return !token.empty() &&
         (std::isalpha(static_cast<unsigned char>(token.front())) != 0 ||
          token.front() == '_') &&
         std::ranges::all_of(token, [](const unsigned char ch) {
           return std::isalnum(ch) != 0 || ch == '_';
         });
}

auto tokenize(const std::string_view source) -> std::vector<std::string> {
  std::vector<std::string> tokens;
  for (size_t i = 0; i < source.size();) {
    const auto start = i;
    const auto ch = static_cast<unsigned char>(source[i]);
    if (std::isspace(ch) != 0) {
      ++i;
    } else if (source.substr(i, 2) == "//") {
      i = source.find('\n', i);
      if (i == std::string_view::npos) {
        break;
      }
    } else if (source.substr(i, 2) == "/*") {
      const auto end = source.find("*/", i + 2);
      if (end == std::string_view::npos) {
        unsupported();
      }
      i = end + 2;
    } else if (ch == '"') {
      ++i;
      while (i < source.size() && source[i] != '"') {
        if (source[i] == '\\') {
          unsupported();
        }
        ++i;
      }
      if (i == source.size()) {
        unsupported();
      }
      tokens.emplace_back(source.substr(start, ++i - start));
    } else if (std::isalnum(ch) != 0 || ch == '_' || ch == '.') {
      const bool numeric = std::isdigit(ch) != 0 || ch == '.';
      while (++i < source.size()) {
        const auto next = static_cast<unsigned char>(source[i]);
        if (std::isalnum(next) != 0 || next == '_' ||
            (numeric && next == '.')) {
          continue;
        }
        if (numeric && (next == '+' || next == '-') &&
            (source[i - 1] == 'e' || source[i - 1] == 'E')) {
          continue;
        }
        break;
      }
      tokens.emplace_back(source.substr(start, i - start));
    } else {
      const auto pair = source.substr(i, 2);
      if (pair == "->" || pair == "**") {
        tokens.emplace_back(pair);
        i += 2;
      } else {
        tokens.emplace_back(source.substr(i++, 1));
      }
    }
  }
  return tokens;
}

class Parser {
  std::vector<std::string> tokens_;
  size_t cursor_ = 0;
  size_t qubits_ = 0;
  std::map<std::string, std::vector<size_t>> quantum_;
  std::map<std::string, size_t> classical_;
  std::vector<OutputVariable> variables_;
  std::vector<bool> selected_;
  std::set<std::string> constants_;
  std::map<size_t, std::string> measured_;
  bool explicitOutputs_ = false;
  bool measurements_ = false;
  bool physical_ = false;
  bool qasm3_ = true;
  std::string quantumSource_ = "OPENQASM 3.0;\n";

  [[nodiscard]] auto peek() const -> std::string_view {
    return cursor_ < tokens_.size() ? tokens_[cursor_] : std::string_view{};
  }
  auto take() -> std::string {
    if (cursor_ == tokens_.size()) {
      unsupported();
    }
    return tokens_[cursor_++];
  }
  auto accept(const std::string_view token) -> bool {
    if (peek() != token) {
      return false;
    }
    ++cursor_;
    return true;
  }
  auto expect(const std::string_view token) -> void {
    if (!accept(token)) {
      unsupported();
    }
  }
  auto name() -> std::string {
    auto result = take();
    if (!identifier(result) || classical_.contains(result) ||
        quantum_.contains(result)) {
      unsupported();
    }
    return result;
  }
  auto width() -> size_t {
    const auto result = number(take());
    expect("]");
    if (result == 0) {
      unsupported();
    }
    return result;
  }
  auto indices(const size_t size) -> std::vector<size_t> {
    if (accept("[")) {
      const auto index = number(take());
      expect("]");
      if (index >= size) {
        unsupported();
      }
      return {index};
    }
    std::vector<size_t> result(size);
    std::iota(result.begin(), result.end(), 0);
    return result;
  }
  auto qubitReference() -> std::vector<size_t> {
    if (accept("$")) {
      if (!quantum_.empty()) {
        unsupported();
      }
      physical_ = true;
      const auto index = number(take());
      measured_.try_emplace(index, "$" + std::to_string(index));
      return {index};
    }
    const auto reg = take();
    if (!quantum_.contains(reg)) {
      unsupported();
    }
    std::vector<size_t> result;
    const auto& sites = quantum_.at(reg);
    for (const auto index : indices(sites.size())) {
      const auto site = sites[index];
      measured_.try_emplace(site, reg + "[" + std::to_string(index) + "]");
      result.push_back(site);
    }
    return result;
  }
  auto assignment(OutputVariable& variable,
                  const std::vector<size_t>& positions) -> void {
    if (accept("measure")) {
      const auto sites = qubitReference();
      if (variable.boolean || sites.size() != positions.size()) {
        unsupported();
      }
      measurements_ = true;
      for (size_t i = 0; i < sites.size(); ++i) {
        variable.bits[positions[i]] = {.qubit = sites[i],
                                       .value = std::nullopt};
      }
      return;
    }
    const auto literal = take();
    if (literal.starts_with('"') && literal.ends_with('"')) {
      if (variable.boolean || literal.size() != positions.size() + 2) {
        unsupported();
      }
      for (size_t i = 0; i < positions.size(); ++i) {
        const auto bit = literal[literal.size() - 2 - i];
        if (bit != '0' && bit != '1') {
          unsupported();
        }
        variable.bits[positions[i]] = {.qubit = std::nullopt,
                                       .value = bit - '0'};
      }
    } else {
      if (positions.size() != 1 ||
          (variable.boolean ? literal != "true" && literal != "false"
                            : literal != "0" && literal != "1" &&
                                  literal != "true" && literal != "false")) {
        unsupported();
      }
      variable.bits[positions[0]] = {
          .qubit = std::nullopt,
          .value = literal == "true" || literal == "1" ? 1 : 0};
    }
  }

public:
  explicit Parser(const std::string_view source) : tokens_(tokenize(source)) {}

  auto parse(const std::string_view source) -> ProgramOutput {
    expect("OPENQASM");
    const auto version = take();
    if (version != "2.0" && version != "3.0" && version != "3.1") {
      unsupported();
    }
    qasm3_ = version != "2.0";
    expect(";");
    while (!peek().empty()) {
      if (accept("include")) {
        const auto file = take();
        if (file != "\"stdgates.inc\"" && file != "\"qelib1.inc\"") {
          unsupported();
        }
        expect(";");
        continue;
      }
      if (peek() == "qubit" || peek() == "qreg") {
        const bool old = take() == "qreg";
        size_t size = 1;
        if (!old && accept("[")) {
          size = width();
        }
        const auto reg = name();
        if (old) {
          expect("[");
          size = width();
        }
        if (physical_ || qubits_ + size > 1'000'000) {
          unsupported();
        }
        auto& sites = quantum_[reg];
        sites.resize(size);
        std::iota(sites.begin(), sites.end(), qubits_);
        qubits_ += size;
        quantumSource_ += "qubit[" + std::to_string(size) + "] " + reg + ";\n";
        expect(";");
        continue;
      }
      const bool selected = accept("output");
      const bool constant = accept("const");
      if (peek() == "bit" || peek() == "bool" || peek() == "creg") {
        const auto type = take();
        OutputVariable variable{
            .name = {}, .boolean = type == "bool", .array = false, .bits = {}};
        size_t size = 1;
        if (accept("[")) {
          if (variable.boolean || type == "creg") {
            unsupported();
          }
          size = width();
          variable.array = true;
        }
        variable.name = name();
        if (type == "creg") {
          expect("[");
          size = width();
          variable.array = true;
        }
        variable.bits.resize(size);
        if (!qasm3_) {
          for (auto& bit : variable.bits) {
            bit.value = 0;
          }
        }
        if (accept("=")) {
          if (selected || (constant && peek() == "measure")) {
            unsupported();
          }
          std::vector<size_t> positions(size);
          std::iota(positions.begin(), positions.end(), 0);
          assignment(variable, positions);
        } else if (constant) {
          unsupported();
        }
        if (constant) {
          constants_.insert(variable.name);
        }
        classical_.emplace(variable.name, variables_.size());
        variables_.push_back(std::move(variable));
        selected_.push_back(selected);
        explicitOutputs_ |= selected;
        expect(";");
        continue;
      }
      if (selected || constant) {
        unsupported();
      }
      if (accept("measure")) {
        const auto sites = qubitReference();
        measurements_ = true;
        if (accept("->")) {
          const auto reg = take();
          if (!classical_.contains(reg) || constants_.contains(reg)) {
            unsupported();
          }
          auto& variable = variables_[classical_.at(reg)];
          const auto positions = indices(variable.bits.size());
          if (variable.boolean || positions.size() != sites.size()) {
            unsupported();
          }
          for (size_t i = 0; i < sites.size(); ++i) {
            variable.bits[positions[i]] = {.qubit = sites[i],
                                           .value = std::nullopt};
          }
        }
        expect(";");
        continue;
      }
      if (classical_.contains(std::string(peek()))) {
        const auto reg = take();
        if (constants_.contains(reg)) {
          unsupported();
        }
        auto& variable = variables_[classical_.at(reg)];
        const auto positions = indices(variable.bits.size());
        expect("=");
        assignment(variable, positions);
        expect(";");
        continue;
      }
      /// Quantum operations are opaque to output reconstruction. The service
      /// validates their gate set; reject classical syntax and measurement use.
      static const std::set<std::string_view> UNSUPPORTED_KEYWORDS{
          "input",   "int",      "uint",    "float",  "angle",
          "complex", "duration", "stretch", "array",  "if",
          "else",    "for",      "while",   "switch", "gate",
          "def",     "extern",   "defcal",  "cal",    "return",
          "break",   "continue", "let",     "end",    "pragma",
      };
      if (UNSUPPORTED_KEYWORDS.contains(peek()) || !identifier(peek()) ||
          (measurements_ && peek() != "barrier")) {
        unsupported();
      }
      const bool barrier = peek() == "barrier";
      auto gate = take();
      if (gate == "cx") {
        gate = "cnot";
      } else if (gate == "ccx") {
        gate = "ccnot";
      }
      std::string statement = gate + ' ';
      int depth = 0;
      while (!accept(";")) {
        const auto token = take();
        if (token == "{" || token == "}" || token == "=" || token == "->" ||
            token == "measure" || classical_.contains(token)) {
          unsupported();
        }
        if (token == "(") {
          ++depth;
        } else if (token == ")" && --depth < 0) {
          unsupported();
        }
        if (token == "$") {
          if (!quantum_.empty()) {
            unsupported();
          }
          physical_ = true;
          statement += '$' + std::to_string(number(take())) + ' ';
        } else {
          statement += token + ' ';
        }
      }
      if (depth != 0) {
        unsupported();
      }
      if (!barrier) {
        quantumSource_ += statement + ";\n";
      }
    }
    ProgramOutput result;
    result.qasm3 = qasm3_;
    result.implicitMeasurement = variables_.empty() && !measurements_;
    for (size_t i = 0; i < variables_.size(); ++i) {
      if (!explicitOutputs_ || selected_[i]) {
        result.variables.push_back(std::move(variables_[i]));
      }
    }
    if (!measured_.empty()) {
      auto reg = std::string("_qdmi_measure");
      while (quantum_.contains(reg) || classical_.contains(reg)) {
        reg += '_';
      }
      quantumSource_ +=
          "bit[" + std::to_string(measured_.size()) + "] " + reg + ";\n";
      size_t i = 0;
      for (const auto& [site, expression] : measured_) {
        quantumSource_.append(reg)
            .append("[")
            .append(std::to_string(i++))
            .append("] = measure ")
            .append(expression)
            .append(";\n");
      }
    }
    /// Keep the source contract in the task's action for reopen-by-ID.
    JsonValue original;
    original.WithString("source", std::string(source));
    result.source = "// QDMI_SOURCE " + original.View().WriteCompact() + '\n' +
                    quantumSource_;
    return result;
  }
};
} // namespace

auto prepareProgram(const std::string_view source) -> ProgramOutput {
  return Parser(source).parse(source);
}

auto parseMeasurementResults(const Aws::Utils::Json::JsonView& result,
                             const ProgramOutput& program, const size_t shots)
    -> MeasurementResults {
  if (!result.IsObject() || !result.ValueExists("measuredQubits") ||
      !result.GetObject("measuredQubits").IsListType() ||
      !result.ValueExists("measurements") ||
      !result.GetObject("measurements").IsListType()) {
    throw std::invalid_argument("Missing measurement arrays");
  }
  std::map<size_t, size_t> columns;
  if (result.ValueExists("measuredQubits")) {
    const auto sites = result.GetArray("measuredQubits");
    for (size_t i = 0; i < sites.GetLength(); ++i) {
      if (!sites[i].IsIntegerType() || sites[i].AsInt64() < 0 ||
          !columns.emplace(static_cast<size_t>(sites[i].AsInt64()), i).second) {
        throw std::invalid_argument("Invalid measuredQubits metadata");
      }
    }
  }
  const auto measurements =
      result.ValueExists("measurements")
          ? result.GetArray("measurements")
          : Aws::Utils::Array<Aws::Utils::Json::JsonView>{};
  if (measurements.GetLength() != shots) {
    throw std::invalid_argument(
        "Measurement shot count does not match task metadata");
  }
  MeasurementResults parsed;
  Aws::Utils::Array<JsonValue> output(shots);
  for (size_t shot = 0; shot < shots; ++shot) {
    if (!measurements[shot].IsListType()) {
      throw std::invalid_argument("Invalid measurement row");
    }
    const auto values = measurements[shot].AsArray();
    if (values.GetLength() != columns.size()) {
      throw std::invalid_argument(
          "Measurement width does not match measuredQubits");
    }
    for (size_t i = 0; i < values.GetLength(); ++i) {
      if (!values[i].IsIntegerType() ||
          (values[i].AsInteger() != 0 && values[i].AsInteger() != 1)) {
        throw std::invalid_argument("Nonbinary backend measurement");
      }
    }
    std::string bits;
    for (const auto& variable : program.variables) {
      Aws::Utils::Array<JsonValue> array(variable.bits.size());
      for (size_t i = 0; i < variable.bits.size(); ++i) {
        const auto& bit = variable.bits[i];
        auto value = bit.value;
        if (bit.qubit) {
          const auto column = columns.find(*bit.qubit);
          if (column == columns.end()) {
            throw std::invalid_argument("Missing measured qubit");
          }
          value = values[column->second].AsInteger();
        }
        if (!value) {
          parsed.binary = false;
          array[i].AsNull();
        } else {
          bits += *value == 0 ? '0' : '1';
          if (variable.boolean) {
            array[i].AsBool(*value != 0);
          } else {
            array[i].AsInteger(*value);
          }
        }
      }
      JsonValue encoded;
      if (variable.array) {
        encoded.AsArray(std::move(array));
      } else {
        encoded = std::move(array[0]);
      }
      output[shot].WithObject(variable.name, std::move(encoded));
    }
    if (program.implicitMeasurement) {
      for (const auto& [site, column] : columns) {
        bits += values[column].AsInteger() == 0 ? '0' : '1';
      }
    }
    if (program.variables.empty()) {
      output[shot] = JsonValue("{}");
    }
    std::ranges::reverse(bits);
    parsed.shots.push_back(std::move(bits));
  }
  JsonValue json;
  parsed.output = json.AsArray(std::move(output)).View().WriteCompact();
  if (!parsed.binary) {
    parsed.shots.clear();
  }
  return parsed;
}
} // namespace amazon::braket::qdmi
