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

#include <charconv>
#include <cstddef>
#include <optional>
#include <string_view>

namespace amazon::braket::qdmi::detail {

[[nodiscard]] inline auto parseQueueValue(std::string_view queueValue)
    -> std::optional<size_t> {
  if (queueValue.starts_with('>')) {
    queueValue.remove_prefix(1);
  }
  if (queueValue.empty()) {
    return std::nullopt;
  }
  size_t value = 0;
  const auto [end, error] = std::from_chars(
      queueValue.data(), queueValue.data() + queueValue.size(), value);
  if (error != std::errc{} || end != queueValue.data() + queueValue.size()) {
    return std::nullopt;
  }
  return value;
}

} // namespace amazon::braket::qdmi::detail
