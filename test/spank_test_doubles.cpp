#include "spank_test_doubles.hpp"

#include "amazon-braket-qdmi-device/constants.hpp"

#include <cstring>

namespace {
spank_test::State testState;
char testSessionStorage;
} // namespace

namespace spank_test {

auto state() -> State& { return testState; }

auto reset() -> void { testState = {}; }

auto registeredOption(const std::string& name) -> spank_option* {
  for (auto* option : testState.registeredOptions) {
    if (name == option->name) {
      return option;
    }
  }
  return nullptr;
}

auto configureOptIn() -> void {
  registeredOption("qdmi-device-session-parameter-baseurl")
      ->cb(registeredOption("qdmi-device-session-parameter-baseurl")->val,
           "arn:aws:braket:::device/quantum-simulator/amazon/sv1", 0);
  registeredOption("qdmi-device-session-parameter-authfile")
      ->cb(registeredOption("qdmi-device-session-parameter-authfile")->val,
           "/tmp/credentials", 0);
}

auto beginRemote() -> spank_t {
  testState.remote = true;
  testState.context = S_CTX_REMOTE;
  return reinterpret_cast<spank_t>(&testState);
}

auto beginAllocator() -> spank_t {
  testState.remote = false;
  testState.context = S_CTX_ALLOCATOR;
  return reinterpret_cast<spank_t>(&testState);
}

} // namespace spank_test

extern "C" {

int spank_remote(spank_t /*spank*/) { return spank_test::state().remote; }

spank_context_type spank_context(void) { return spank_test::state().context; }

int spank_option_register(spank_t /*spank*/, spank_option* option) {
  spank_test::state().registeredOptions.push_back(option);
  return ESPANK_SUCCESS;
}

int spank_option_getopt(spank_t /*spank*/, spank_option* option,
                        char** argument) {
  const auto found = spank_test::state().forwardedOptions.find(option->name);
  if (found == spank_test::state().forwardedOptions.end()) {
    return ESPANK_ERROR;
  }
  *argument = const_cast<char*>(found->second.c_str());
  return ESPANK_SUCCESS;
}

int spank_setenv(spank_t /*spank*/, const char* name, const char* value,
                 int overwrite) {
  spank_test::state().environmentOverwrites.push_back(overwrite);
  if (spank_test::state().environmentResult != ESPANK_SUCCESS) {
    return spank_test::state().environmentResult;
  }
  spank_test::state().environment[name] = value;
  return ESPANK_SUCCESS;
}

int slurm_spank_log(spank_t /*spank*/, int /*level*/, const char* format, ...) {
  spank_test::state().logs.emplace_back(format);
  return ESPANK_SUCCESS;
}

int AMAZON_BRAKET_QDMI_device_initialize(void) {
  ++spank_test::state().initializeCalls;
  return spank_test::state().deviceInitializeResult;
}

int AMAZON_BRAKET_QDMI_device_finalize(void) {
  ++spank_test::state().finalizeCalls;
  return QDMI_SUCCESS;
}

int AMAZON_BRAKET_QDMI_device_session_alloc(
    AMAZON_BRAKET_QDMI_Device_Session* session) {
  ++spank_test::state().sessionAllocCalls;
  if (spank_test::state().sessionAllocResult == QDMI_SUCCESS) {
    *session = reinterpret_cast<AMAZON_BRAKET_QDMI_Device_Session>(
        &testSessionStorage);
  }
  return spank_test::state().sessionAllocResult;
}

int AMAZON_BRAKET_QDMI_device_session_set_parameter(
    AMAZON_BRAKET_QDMI_Device_Session /*session*/,
    QDMI_Device_Session_Parameter parameter, size_t size, const void* value) {
  spank_test::state().parameters.push_back(parameter);
  if (spank_test::state().setParameterResult != QDMI_SUCCESS) {
    return spank_test::state().setParameterResult;
  }
  spank_test::state().parameterValues[parameter] =
      std::string(static_cast<const char*>(value), size - 1);
  return QDMI_SUCCESS;
}

int AMAZON_BRAKET_QDMI_device_session_init(
    AMAZON_BRAKET_QDMI_Device_Session /*session*/) {
  ++spank_test::state().sessionInitCalls;
  return spank_test::state().sessionInitResult;
}

void AMAZON_BRAKET_QDMI_device_session_free(
    AMAZON_BRAKET_QDMI_Device_Session /*session*/) {
  ++spank_test::state().sessionFreeCalls;
}

int AMAZON_BRAKET_QDMI_device_session_query_device_property(
    AMAZON_BRAKET_QDMI_Device_Session /*session*/,
    QDMI_Device_Property property, size_t size, void* value,
    size_t* /*sizeRet*/) {
  ++spank_test::state().queryStatusCalls;
  if (spank_test::state().queryStatusResult != QDMI_SUCCESS) {
    return spank_test::state().queryStatusResult;
  }
  if (property == QDMI_DEVICE_PROPERTY_STATUS &&
      size >= sizeof(QDMI_Device_Status)) {
    *static_cast<QDMI_Device_Status*>(value) =
        static_cast<QDMI_Device_Status>(spank_test::state().deviceStatus);
  }
  return QDMI_SUCCESS;
}

} // extern "C"
