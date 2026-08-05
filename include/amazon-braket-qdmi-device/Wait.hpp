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

#include "amazon_braket_qdmi/device.h"

#include <chrono>
#include <cstddef>

namespace amazon::braket::qdmi::detail {
using WaitClock = std::chrono::steady_clock;

// Small dependency table for exercising the wait loop offline without AWS calls
// or wall-clock delays. This header is private and is not installed.
struct JobWaitFunctions {
  using CheckStatus = auto (*)(void*, QDMI_Job_Status*) -> QDMI_STATUS;
  using Now = auto (*)(void*) -> WaitClock::time_point;
  using SleepFor = auto (*)(void*, WaitClock::duration) -> void;

  void* context;
  CheckStatus checkStatus;
  Now now;
  SleepFor sleepFor;
};

[[nodiscard]] inline auto waitTimedOut(const WaitClock::time_point start,
                                       const WaitClock::time_point now,
                                       const size_t timeout) -> bool {
  if (timeout == 0U) {
    return false;
  }
  using UnsignedSeconds = std::chrono::duration<size_t>;
  const auto elapsed = std::chrono::duration_cast<UnsignedSeconds>(now - start);
  return elapsed.count() >= timeout;
}
} // namespace amazon::braket::qdmi::detail
