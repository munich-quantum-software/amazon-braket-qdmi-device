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

#include "amazon-braket-qdmi-device/DeviceParser.hpp"

#include "aws/core/utils/json/JsonSerializer.h"
#include "gtest/gtest.h"

#include <stdexcept>
#include <string>
#include <vector>

TEST(MeasurementResultParserTest, OrdersColumnsByMeasuredQubit) {
  for (
      const auto* response : {
          R"({"measuredQubits":[0,1,2],"measurements":[[1,0,0],[1,0,1],[1,0,0]]})",
          R"({"measuredQubits":[2,1,0],"measurements":[[0,0,1],[1,0,1],[0,0,1]]})",
          R"({"measuredQubits":[17,2,9],"measurements":[[0,1,0],[1,1,0],[0,1,0]]})",
      }) {
    SCOPED_TRACE(response);
    const Aws::Utils::Json::JsonValue json(response);
    ASSERT_TRUE(json.WasParseSuccessful());

    const auto results =
        amazon::braket::qdmi::parseMeasurementResults(json.View());

    EXPECT_EQ(results, (std::vector<std::string>{"001", "101", "001"}));
  }
}

TEST(MeasurementResultParserTest, ProjectsReadoutFromReturnedSource) {
  const auto response = [](const std::string& assignments) {
    Aws::Utils::Json::JsonValue json(
        R"({"measuredQubits":[2,0,1],"measurements":[[1,1,0],[0,1,1]]})");
    json.WithObject(
        "additionalMetadata",
        Aws::Utils::Json::JsonValue{}.WithObject(
            "action", Aws::Utils::Json::JsonValue{}.WithString(
                          "source", "OPENQASM 3;\nqubit[3] q;\n" + assignments +
                                        "measure q[1];\n")));
    return json;
  };
  const auto selected =
      response("bit[2] c;\nc[0] = measure q[0];\nc[1] = measure q[2];\n");
  EXPECT_EQ(amazon::braket::qdmi::parseMeasurementResults(selected.View()),
            (std::vector<std::string>{"11", "01"}));
  const auto physical =
      response("bit[2] c;\nc[0] = measure $0;\nc[1] = measure $2;\n");
  EXPECT_EQ(amazon::braket::qdmi::parseMeasurementResults(physical.View()),
            (std::vector<std::string>{"11", "01"}));
  const auto unmarked = response("");
  EXPECT_EQ(amazon::braket::qdmi::parseMeasurementResults(
                unmarked.View(), "OPENQASM 3;\nqubit[3] q;\nmeasure "
                                 "q[0];\nmeasure q[1];\nmeasure q[2];\n"),
            (std::vector<std::string>{"", ""}));
  EXPECT_EQ(amazon::braket::qdmi::parseMeasurementResults(
                unmarked.View(), "OPENQASM 3;\nqubit[3] q;\nh q[0];\n"),
            (std::vector<std::string>{"", ""}));
  const auto empty = response("");
  EXPECT_EQ(amazon::braket::qdmi::parseMeasurementResults(empty.View()),
            (std::vector<std::string>{"", ""}));
  for (const auto* assignment : {
           "bit[1] c;\nc[0] = measure q[3];\n",
           "bit[1] c;\nc[0] = measure q[bad];\n",
       }) {
    const auto invalid = response(assignment);
    EXPECT_THROW(amazon::braket::qdmi::parseMeasurementResults(invalid.View()),
                 std::runtime_error);
  }
}

TEST(MeasurementResultParserTest, ValidatesAllMeasurementOutcomes) {
  for (const auto* source : {
           "OPENQASM 3;\nbit[1] c;\nc[0] = "
           "measure $0;\n",
           "OPENQASM 3;\nx $0;\n",
       }) {
    for (const auto* response : {
             R"({"measuredQubits":[0,1],"measurements":[[1,2]]})",
             R"({"measuredQubits":[0,1],"measurements":[[1,"0"]]})",
             R"({"measuredQubits":[0,1],"measurements":[[1]]})",
         }) {
      const Aws::Utils::Json::JsonValue json(response);
      EXPECT_THROW(
          amazon::braket::qdmi::parseMeasurementResults(json.View(), source),
          std::runtime_error);
    }
  }
}

TEST(MeasurementResultParserTest, RestoresDeclaredClassicalOutputs) {
  const Aws::Utils::Json::JsonValue json(
      R"({"measuredQubits":[5,1],"measurements":[[1,0],[0,1]]})");
  for (const auto* source : {
           "OPENQASM 3; bit[4] c; c[0] = measure $5; c[2] = measure $1;",
           "OPENQASM 3.0; /* measure $9; */ qubit[6] data; bit[4] out;\n"
           "// misleading: out[1] = measure data[4];\n"
           "out [ 0 ] = measure data [ 5 ]; out[2]=measure data[1];",
       }) {
    SCOPED_TRACE(source);
    EXPECT_EQ(
        amazon::braket::qdmi::parseMeasurementResults(json.View(), source),
        (std::vector<std::string>{"0001", "0100"}));
  }
  EXPECT_EQ(amazon::braket::qdmi::parseMeasurementResults(
                json.View(), "OPENQASM 3; bit[3] c; c = \"101\";"
                             "c[1] = measure $5; c[2] = measure $5;"),
            (std::vector<std::string>{"111", "001"}));
  EXPECT_EQ(amazon::braket::qdmi::parseMeasurementResults(
                json.View(), "OPENQASM 3; bit[4] c; c = \"0000\"; x $5;"),
            (std::vector<std::string>{"0000", "0000"}));
  EXPECT_EQ(amazon::braket::qdmi::parseMeasurementResults(
                json.View(), "OPENQASM 3; x $5; measure $5; measure $1;"),
            (std::vector<std::string>{"", ""}));
}

TEST(MeasurementResultParserTest, RejectsUnsupportedOrMalformedReadout) {
  const Aws::Utils::Json::JsonValue json(
      R"({"measuredQubits":[0],"measurements":[[1]]})");
  for (const auto* source : {
           "OPENQASM 3; bit[1] c; c[1] = measure $0;",
           "OPENQASM 3; bit[1] c; c[0] = measure $1;",
           "OPENQASM 3; bit[1] c; c[0] = measure $18446744073709551615;",
           "OPENQASM 3; bit[2] c; c = \"0\";",
           "OPENQASM 3; bit[1] c; c = \"2\";",
           "OPENQASM 3; /* unterminated",
       }) {
    SCOPED_TRACE(source);
    EXPECT_THROW(
        amazon::braket::qdmi::parseMeasurementResults(json.View(), source),
        std::runtime_error);
  }
}

TEST(MeasurementResultParserTest, UsesMeasuredQubitOrderForOtherSourceForms) {
  const Aws::Utils::Json::JsonValue json(
      R"({"measuredQubits":[0,1],"measurements":[[1,0]]})");
  for (const auto* source : {
           "OPENQASM 2.0; qreg q[2]; creg b[2]; measure q -> b;",
           "OPENQASM 3; bit c; c = measure $0;",
           "OPENQASM 3; bit[1] c; bit[1] other;",
           "OPENQASM 3; bit[1] c; c[0] = 1;",
           "OPENQASM 3; bit[1] c; c = measure $0;",
           "OPENQASM 3; bit[1] c; if (true) { c[0] = measure $0; }",
           "OPENQASM 3; bit[1] c; c[0] = measure $0; x $1;",
       }) {
    SCOPED_TRACE(source);
    EXPECT_EQ(
        amazon::braket::qdmi::parseMeasurementResults(json.View(), source),
        (std::vector<std::string>{"01"}));
  }
}
