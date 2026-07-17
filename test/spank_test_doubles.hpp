#pragma once

#include "amazon_braket_qdmi/device.h"

#include <slurm/spank.h>
#include <string>
#include <unordered_map>
#include <vector>

namespace spank_test {

struct State {
  bool remote = false;
  spank_context_type context = S_CTX_LOCAL;
  std::vector<spank_option*> registeredOptions;
  std::unordered_map<std::string, std::string> forwardedOptions;
  std::unordered_map<std::string, std::string> environment;
  std::vector<int> environmentOverwrites;
  std::vector<std::string> logs;
  int environmentResult = ESPANK_SUCCESS;

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
