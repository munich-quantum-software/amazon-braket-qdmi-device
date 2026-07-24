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

#include <slurm/slurm_errno.h>
#include <slurm/spank.h>
#include <string>
#include <unordered_map>
#include <vector>

namespace spank_test {

struct State {
  bool remote = false;
  spank_context_t context = S_CTX_LOCAL;
  std::vector<spank_option*> registeredOptions;
  std::unordered_map<std::string, std::string> forwardedOptions;
  std::unordered_map<std::string, std::string> jobEnvironment;
  std::unordered_map<std::string, std::string> environment;
  std::unordered_map<std::string, std::string> sessionEnvironmentAtInit;
  std::vector<int> environmentOverwrites;
  std::vector<std::string> logs;
  int environmentResult = ESPANK_SUCCESS;
  int taskId = 0;

  int deviceInitializeResult = QDMI_SUCCESS;
  int sessionAllocResult = QDMI_SUCCESS;
  int setParameterResult = QDMI_SUCCESS;
  int sessionInitResult = QDMI_SUCCESS;
  int queryStatusResult = QDMI_SUCCESS;
  int deviceStatus = QDMI_DEVICE_STATUS_IDLE;
  int initializeCalls = 0;
  int finalizeCalls = 0;
  int sessionAllocCalls = 0;
  int sessionFreeCalls = 0;
  int sessionInitCalls = 0;
  int queryStatusCalls = 0;
  std::vector<QDMI_Device_Session_Parameter> parameters;
  std::unordered_map<int, std::string> parameterValues;
};

auto state() -> State&;
auto reset() -> void;
auto registeredOption(const std::string& name) -> spank_option*;
auto configureOptIn() -> void;
auto beginRemote() -> spank_t;
auto beginAllocator() -> spank_t;

} // namespace spank_test
