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

#include <aws/core/utils/json/JsonSerializer.h>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <vector>

using amazon::braket::qdmi::parseMeasurementResults;
using amazon::braket::qdmi::prepareProgram;
using Aws::Utils::Json::JsonValue;

TEST(MeasurementResultParserTest,
     PreservesClassicalDestinationsAndFinalWrites) {
  const auto program = prepareProgram(R"(OPENQASM 3.0;
include "stdgates.inc";
qubit[3] q;
bit[3] a = "100";
bit[2] b = "00";
x q[0];
a[0] = measure q[0];
b[1] = measure q[1];
a[0] = measure q[2];
)");
  const JsonValue json(
      R"({"measuredQubits":[2,0,1],"measurements":[[0,1,1],[1,0,0]]})");
  const auto result = parseMeasurementResults(json.View(), program, 2);
  EXPECT_TRUE(result.binary);
  EXPECT_EQ(result.shots, (std::vector<std::string>{"10100", "00101"}));
  const JsonValue expected(
      R"([{"a":[0,0,1],"b":[0,1]},{"a":[1,0,1],"b":[0,0]}])");
  EXPECT_EQ(JsonValue(result.output), expected);
  EXPECT_TRUE(program.source.starts_with("// QDMI_SOURCE "));
  EXPECT_EQ(program.source.find("include \""), std::string::npos);
}

TEST(MeasurementResultParserTest,
     SelectsExplicitOutputsAndRetainsUndefinedValues) {
  const auto program = prepareProgram(R"(OPENQASM 3.0;
qubit q;
bit hidden;
output bit[2] bits;
output bool accepted;
accepted = true;
bits[1] = measure q;
)");
  const JsonValue json(R"({"measuredQubits":[0],"measurements":[[1],[0]]})");
  const auto result = parseMeasurementResults(json.View(), program, 2);
  EXPECT_FALSE(result.binary);
  EXPECT_TRUE(result.shots.empty());
  const JsonValue expected(
      R"([{"bits":[null,1],"accepted":true},{"bits":[null,0],"accepted":true}])");
  EXPECT_EQ(JsonValue(result.output), expected);
}

TEST(MeasurementResultParserTest, IncludesConstantsAndPreservesScalarTypes) {
  const auto program = prepareProgram(R"(OPENQASM 3.0;
const bool accepted = true;
const bit answer = 0;
qubit q;
x q;
)");
  const JsonValue json(R"({"measuredQubits":[0],"measurements":[[1]]})");
  const auto result = parseMeasurementResults(json.View(), program, 1);
  EXPECT_EQ(result.shots, (std::vector<std::string>{"01"}));
  EXPECT_EQ(JsonValue(result.output),
            JsonValue(R"([{"accepted":true,"answer":0}])"));
}

TEST(MeasurementResultParserTest,
     SeparatesEmptyOutputsFromImplicitMeasurement) {
  const JsonValue json(R"({"measuredQubits":[1,0],"measurements":[[0,1]]})");
  const auto implicit = prepareProgram("OPENQASM 3.0; qubit[2] q; x q[0];");
  EXPECT_EQ(parseMeasurementResults(json.View(), implicit, 1).shots,
            (std::vector<std::string>{"01"}));
  const auto explicitMeasurement =
      prepareProgram("OPENQASM 3.0; qubit[2] q; measure q;");
  EXPECT_EQ(parseMeasurementResults(json.View(), explicitMeasurement, 1).shots,
            (std::vector<std::string>{""}));
}

TEST(MeasurementResultParserTest, HandlesQASM2AndPhysicalSites) {
  const auto qasm2 =
      prepareProgram("OPENQASM 2.0; include \"qelib1.inc\"; qreg q[2]; creg "
                     "c[3]; cx q[0],q[1]; measure q[0] -> c[2];");
  EXPECT_FALSE(qasm2.qasm3);
  EXPECT_NE(qasm2.source.find("cnot q [ 0 ] , q [ 1 ]"), std::string::npos);
  const JsonValue json(R"({"measuredQubits":[0],"measurements":[[1]]})");
  EXPECT_EQ(parseMeasurementResults(json.View(), qasm2, 1).shots,
            (std::vector<std::string>{"100"}));
  const auto physical = prepareProgram(
      "OPENQASM 3.0; bit[2] c; x $9; c[0] = measure $9; c[1] = measure $2;");
  const JsonValue physicalResult(
      R"({"measuredQubits":[2,9],"measurements":[[0,1]]})");
  EXPECT_EQ(parseMeasurementResults(physicalResult.View(), physical, 1).shots,
            (std::vector<std::string>{"01"}));
}

TEST(MeasurementResultParserTest, RejectsUnsupportedOutputsBeforeSubmission) {
  for (const auto* source : {
           "OPENQASM 3.0; qubit q; int count = 1;",
           "OPENQASM 3.0; output float value; value = 0.0;",
           "OPENQASM 3.0; input bool value;",
           "OPENQASM 3.0; qubit q; bit c; c = measure q; x q;",
           "OPENQASM 3.0; qubit q; bit c; if (true) { c = measure q; }",
           "OPENQASM 3.0; qubit q; bit c; c = !c;",
           "OPENQASM 3.0; bit[2] c = \"10\"; c[2] = 0;",
           "OPENQASM 3.0; const bit c = 0; c = 1;",
           "OPENQASM 3.0; array[bool, 2] c;",
           "OPENQASM 3.0; include \"unknown.inc\";",
       }) {
    EXPECT_THROW(prepareProgram(source), std::invalid_argument) << source;
  }
}

TEST(MeasurementResultParserTest, RejectsMalformedBackendColumns) {
  const auto program =
      prepareProgram("OPENQASM 3.0; qubit[2] q; bit[2] c; c = measure q;");
  for (const auto* response : {
           R"({"measurements":[[0,1]]})",
           R"({"measuredQubits":[0,0],"measurements":[[0,1]]})",
           R"({"measuredQubits":[0,1],"measurements":[[0]]})",
           R"({"measuredQubits":[0,1],"measurements":[[0,2]]})",
           R"({"measuredQubits":[0,1],"measurements":[[0,true]]})",
           R"({"measuredQubits":[0,2],"measurements":[[0,1]]})",
           R"({"measuredQubits":[0,1],"measurements":[]})",
       }) {
    const JsonValue json(response);
    EXPECT_THROW(parseMeasurementResults(json.View(), program, 1),
                 std::invalid_argument)
        << response;
  }
}
