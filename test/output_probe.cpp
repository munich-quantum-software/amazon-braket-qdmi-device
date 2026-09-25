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

#include <aws/core/utils/Array.h>
#include <aws/core/utils/json/JsonSerializer.h>
#include <cstddef>
#include <exception>
#include <iostream>
#include <iterator>
#include <string>
#include <utility>

int main() {
  using Aws::Utils::Json::JsonValue;
  try {
    const std::string input(std::istreambuf_iterator<char>{std::cin}, {});
    const JsonValue request(input);
    if (!request.WasParseSuccessful()) {
      return 1;
    }
    const auto view = request.View();
    const auto program =
        amazon::braket::qdmi::prepareProgram(view.GetString("source"));
    JsonValue response;
    response.WithString("source", program.source);
    if (view.ValueExists("result")) {
      const auto result = amazon::braket::qdmi::parseMeasurementResults(
          view.GetObject("result"), program,
          static_cast<size_t>(view.GetInt64("shots")));
      Aws::Utils::Array<JsonValue> shots(result.shots.size());
      for (size_t i = 0; i < result.shots.size(); ++i) {
        shots[i].AsString(result.shots[i]);
      }
      response.WithArray("shots", std::move(shots));
      response.WithBool("binary", result.binary);
      response.WithObject("output", JsonValue(result.output));
    }
    std::cout << response.View().WriteCompact() << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
