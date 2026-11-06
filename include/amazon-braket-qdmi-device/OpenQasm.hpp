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

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <stdexcept>
#include <string_view>
#include <system_error>

namespace amazon::braket::qdmi::detail {

// Share lexical rules between submission adaptation and result recovery.
class OpenQasmTokens {
public:
  explicit OpenQasmTokens(std::string_view source) : source_(source) {}

  auto next() -> std::string_view {
    while (!source_.empty()) {
      if (std::isspace(static_cast<unsigned char>(source_.front())) != 0) {
        source_.remove_prefix(1);
      } else if (source_.starts_with("//")) {
        source_.remove_prefix(std::min(source_.find('\n'), source_.size()));
      } else if (source_.starts_with("/*")) {
        const auto end = source_.find("*/", 2);
        if (end == std::string_view::npos) {
          throw std::runtime_error("Unterminated OpenQASM comment");
        }
        source_.remove_prefix(end + 2);
      } else {
        break;
      }
    }
    if (source_.empty()) {
      return {};
    }
    size_t length = 1;
    if (source_.front() == '"') {
      for (; length < source_.size() && source_[length] != '"'; ++length) {
        if (source_[length] == '\\') {
          ++length;
        }
      }
      if (length >= source_.size()) {
        throw std::runtime_error("Unterminated OpenQASM string");
      }
      ++length;
    } else if (std::isalnum(static_cast<unsigned char>(source_.front())) != 0 ||
               source_.front() == '_') {
      while (length < source_.size() &&
             (std::isalnum(static_cast<unsigned char>(source_[length])) != 0 ||
              source_[length] == '_')) {
        ++length;
      }
    }
    const auto result = source_.substr(0, length);
    source_.remove_prefix(length);
    return result;
  }

  auto expect(std::string_view expected) -> void {
    if (next() != expected) {
      throw std::runtime_error("Unsupported OpenQASM readout syntax");
    }
  }

  auto number() -> size_t {
    const auto token = next();
    if (token.empty()) {
      throw std::runtime_error("Missing OpenQASM readout index");
    }
    size_t value = 0;
    const auto parsed =
        std::from_chars(token.data(), token.data() + token.size(), value);
    if (parsed.ec != std::errc{} || parsed.ptr != token.data() + token.size()) {
      throw std::runtime_error("Invalid OpenQASM readout index");
    }
    return value;
  }

  [[nodiscard]] auto remaining() const -> std::string_view { return source_; }

private:
  std::string_view source_;
};

} // namespace amazon::braket::qdmi::detail
