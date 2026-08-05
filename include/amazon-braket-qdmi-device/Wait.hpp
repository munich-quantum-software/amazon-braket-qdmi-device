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
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#pragma once

#include <chrono>
#include <cstddef>

namespace amazon::braket::qdmi::detail {
using WaitClock = std::chrono::steady_clock;

[[nodiscard]] inline auto waitTimedOut(const WaitClock::time_point start,
                                       const WaitClock::time_point now,
                                       const size_t timeout) -> bool {
  if (timeout == 0U) {
    return false;
  }
  using UnsignedSeconds = std::chrono::duration<size_t>;
  const auto elapsed =
      std::chrono::duration_cast<UnsignedSeconds>(now - start);
  return elapsed.count() >= timeout;
}
} // namespace amazon::braket::qdmi::detail
