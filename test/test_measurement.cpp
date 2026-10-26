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

TEST(MeasurementResultParserTest,
     ReconstructsClassicalDestinationsAcrossColumns) {
  const auto program = prepareProgram(
      "OPENQASM 3.0; qubit[3] q; bit[4] a = \"0000\"; bit b = 1; "
      "x q[0]; a[3] = measure q[0]; a[1] = measure q[2];");
  for (const auto* response :
       {R"({"measuredQubits":[0,2],"measurements":[[1,0],[0,1]]})",
        R"({"measuredQubits":[2,0],"measurements":[[0,1],[1,0]]})"}) {
    const Aws::Utils::Json::JsonValue json(response);
    EXPECT_EQ(parseMeasurementResults(json.View(), program, 2),
              (std::vector<std::string>{"11000", "10010"}));
  }
}

TEST(MeasurementResultParserTest, PreservesFinalValuesAndOutputSelection) {
  const auto program = prepareProgram(
      "OPENQASM 3.0; qubit[2] q; bit ignored; output bit[3] c; "
      "c = \"001\"; c[2] = measure q[0]; c[2] = measure q[1]; c[1] = 1;");
  const Aws::Utils::Json::JsonValue json(
      R"({"measuredQubits":[1,0],"measurements":[[0,1]]})");
  EXPECT_EQ(parseMeasurementResults(json.View(), program, 1),
            (std::vector<std::string>{"011"}));
}

TEST(MeasurementResultParserTest, RetainsImplicitMeasurementConvention) {
  const auto program = prepareProgram("OPENQASM 3.0; qubit[3] q; x q[0];");
  const Aws::Utils::Json::JsonValue json(
      R"({"measuredQubits":[2,0,1],"measurements":[[0,1,0]]})");
  EXPECT_EQ(parseMeasurementResults(json.View(), program, 1),
            (std::vector<std::string>{"001"}));
}

TEST(MeasurementResultParserTest,
     SupportsQasm2InitialZerosAndRegisterMeasurement) {
  const auto program = prepareProgram(
      "OPENQASM 2.0; include \"qelib1.inc\"; qreg q[2]; creg c[2]; creg d[1]; "
      "x q[0]; measure q -> c;");
  const Aws::Utils::Json::JsonValue json(
      R"({"measuredQubits":[1,0],"measurements":[[0,1]]})");
  EXPECT_EQ(parseMeasurementResults(json.View(), program, 1),
            (std::vector<std::string>{"001"}));
}

TEST(MeasurementResultParserTest, RejectsUnsupportedProgramsBeforeExecution) {
  for (const auto* source :
       {"OPENQASM 3.0; qubit q; bit c;", // undefined selected bit
        "OPENQASM 3.0; qubit q; bit c; c=measure q; x q;",
        "OPENQASM 3.0; qubit q; int c = 0;",
        "OPENQASM 3.0; qubit q; bit c = 0; if(c) x q;",
        "OPENQASM 3.0; qubit q; bit c = 0; c = c + 1;",
        "OPENQASM 3.0; qubit[0] q;", "OPENQASM 3.0; qubit q; measure q;",
        "OPENQASM 3.0; /* unterminated"}) {
    SCOPED_TRACE(source);
    EXPECT_THROW(prepareProgram(source), std::invalid_argument);
  }
}

TEST(MeasurementResultParserTest, RejectsMalformedOrIncompleteReadout) {
  const auto program =
      prepareProgram("OPENQASM 3.0; qubit[2] q; bit[2] c; c=measure q;");
  for (const auto* response :
       {R"({"measuredQubits":[0,0],"measurements":[[0,1]]})",
        R"({"measuredQubits":[0,1],"measurements":[[0,2]]})",
        R"({"measuredQubits":[0,1],"measurements":[[0,4294967296]]})",
        R"({"measuredQubits":[0,1],"measurements":[[0]]})",
        R"({"measuredQubits":[0],"measurements":[[0]]})",
        R"({"measuredQubits":[0,1],"measurements":[]})"}) {
    const Aws::Utils::Json::JsonValue json(response);
    EXPECT_THROW(parseMeasurementResults(json.View(), program, 1),
                 std::invalid_argument);
  }
}
