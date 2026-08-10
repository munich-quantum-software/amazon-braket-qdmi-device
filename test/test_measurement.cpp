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

#include <aws/core/utils/json/JsonSerializer.h>
#include <gtest/gtest.h>
#include <string>
#include <vector>

TEST(MeasurementResultParserTest, UsesQDMIOrderForShotsAndCounts) {
  const Aws::Utils::Json::JsonValue json(
      R"({"measurements":[[1,0,0],[1,0,1],[1,0,0]]})");
  ASSERT_TRUE(json.WasParseSuccessful());

  const auto results = amazon::braket::qdmi::parseMeasurementResults(
      json.View().GetArray("measurements"));

  EXPECT_EQ(results, (std::vector<std::string>{"001", "101", "001"}));
}
