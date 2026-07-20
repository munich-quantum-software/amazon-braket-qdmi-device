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

#include "amazon-braket-qdmi-device/constants.hpp"
#include "spank_test_doubles.hpp"

#include <array>
#include <gtest/gtest.h>
#include <string>

int slurm_spank_init(spank_t spank, int argc, char* argv[]);
int slurm_spank_init_post_opt(spank_t spank, int argc, char* argv[]);
int slurm_spank_user_init(spank_t spank, int argc, char* argv[]);
int slurm_spank_task_init(spank_t spank, int argc, char* argv[]);

namespace {

constexpr auto baseUrlOption = "qdmi-device-session-parameter-baseurl";
constexpr auto authFileOption = "qdmi-device-session-parameter-authfile";

class SpankTest : public ::testing::Test {
protected:
  auto initialize(spank_t spank) -> void {
    ASSERT_EQ(slurm_spank_init(spank, 0, nullptr), 0);
  }

  auto initializeRemote() -> spank_t {
    auto spank = spank_test::beginRemote();
    initialize(spank);
    return spank;
  }

  auto initializeAllocator() -> spank_t {
    auto spank = spank_test::beginAllocator();
    initialize(spank);
    return spank;
  }

  void SetUp() override { spank_test::reset(); }
};

TEST_F(SpankTest, RegistersAllOptions) {
  const auto spank = initializeAllocator();
  ASSERT_EQ(spank_test::state().registeredOptions.size(), 4);

  const std::array expected = {
      baseUrlOption,
      "qdmi-device-session-parameter-region",
      "qdmi-device-session-parameter-reservation-arn",
      authFileOption,
  };
  for (const auto* name : expected) {
    ASSERT_NE(spank_test::registeredOption(name), nullptr);
  }
  (void)spank;
}

TEST_F(SpankTest, RejectsInvalidOptionValues) {
  initializeAllocator();
  auto* option = spank_test::registeredOption(baseUrlOption);
  ASSERT_NE(option, nullptr);

  EXPECT_NE(option->cb(0, "value", 0), 0);
  EXPECT_NE(option->cb(5, "value", 0), 0);
  EXPECT_NE(option->cb(option->val, nullptr, 0), 0);
  EXPECT_NE(option->cb(option->val, "", 0), 0);
  EXPECT_NE(option->cb(option->val, "value\n", 0), 0);
  EXPECT_NE(option->cb(option->val, "value\r", 0), 0);
  EXPECT_EQ(option->cb(option->val, "valid", 0), 0);
}

TEST_F(SpankTest, AllocatorWithoutOptInIsAccepted) {
  const auto spank = initializeAllocator();
  EXPECT_EQ(slurm_spank_init_post_opt(spank, 0, nullptr), 0);
  EXPECT_EQ(spank_test::state().initializeCalls, 0);
}

TEST_F(SpankTest, AllocatorRequiresBothOptInOptions) {
  auto spank = initializeAllocator();
  auto* baseUrl = spank_test::registeredOption(baseUrlOption);
  auto* authFile = spank_test::registeredOption(authFileOption);
  ASSERT_NE(baseUrl, nullptr);
  ASSERT_NE(authFile, nullptr);

  ASSERT_EQ(baseUrl->cb(baseUrl->val, "device", 0), 0);
  EXPECT_NE(slurm_spank_init_post_opt(spank, 0, nullptr), 0);

  spank = initializeAllocator();
  authFile = spank_test::registeredOption(authFileOption);
  ASSERT_EQ(authFile->cb(authFile->val, "/tmp/credentials", 0), 0);
  EXPECT_NE(slurm_spank_init_post_opt(spank, 0, nullptr), 0);
}

TEST_F(SpankTest, AllocatorAcceptsCompleteOptInAndOptionalParameters) {
  const auto spank = initializeAllocator();
  spank_test::configureOptIn();
  ASSERT_EQ(spank_test::registeredOption("qdmi-device-session-parameter-region")
                ->cb(2, "us-east-1", 0),
            0);
  ASSERT_EQ(spank_test::registeredOption(
                "qdmi-device-session-parameter-reservation-arn")
                ->cb(3, "reservation", 0),
            0);
  EXPECT_EQ(slurm_spank_init_post_opt(spank, 0, nullptr), 0);
}

TEST_F(SpankTest, RemoteWithoutOptInSkipsValidation) {
  const auto spank = initializeRemote();
  ASSERT_EQ(slurm_spank_init_post_opt(spank, 0, nullptr), 0);
  ASSERT_EQ(slurm_spank_user_init(spank, 0, nullptr), 0);
  EXPECT_EQ(slurm_spank_task_init(spank, 0, nullptr), 0);
  EXPECT_EQ(spank_test::state().initializeCalls, 0);
}

TEST_F(SpankTest, RemoteIncompleteOptInRejectsJob) {
  const auto spank = initializeRemote();
  auto* baseUrl = spank_test::registeredOption(baseUrlOption);
  ASSERT_EQ(baseUrl->cb(baseUrl->val, "device", 0), 0);
  ASSERT_EQ(slurm_spank_init_post_opt(spank, 0, nullptr), 0);
  ASSERT_EQ(slurm_spank_user_init(spank, 0, nullptr), 0);
  EXPECT_NE(slurm_spank_task_init(spank, 0, nullptr), 0);
  EXPECT_EQ(spank_test::state().initializeCalls, 0);
}

TEST_F(SpankTest, RemoteValidationUsesForwardedOptionsAndInjectsEnvironment) {
  const auto spank = initializeRemote();
  spank_test::configureOptIn();
  ASSERT_EQ(spank_test::registeredOption("qdmi-device-session-parameter-region")
                ->cb(2, "us-east-1", 0),
            0);
  ASSERT_EQ(spank_test::registeredOption(
                "qdmi-device-session-parameter-reservation-arn")
                ->cb(3, "reservation", 0),
            0);
  spank_test::state().forwardedOptions[baseUrlOption] = "forwarded-device";
  spank_test::state().forwardedOptions[authFileOption] = "/forwarded/auth";
  ASSERT_EQ(slurm_spank_init_post_opt(spank, 0, nullptr), 0);
  ASSERT_EQ(slurm_spank_user_init(spank, 0, nullptr), 0);
  EXPECT_EQ(slurm_spank_user_init(spank, 0, nullptr), 0);
  EXPECT_EQ(slurm_spank_task_init(spank, 0, nullptr), 0);

  EXPECT_EQ(spank_test::state()
                .parameterValues[QDMI_DEVICE_SESSION_PARAMETER_BASEURL],
            "forwarded-device");
  EXPECT_EQ(spank_test::state()
                .parameterValues[QDMI_DEVICE_SESSION_PARAMETER_AUTHFILE],
            "/forwarded/auth");
  EXPECT_EQ(
      spank_test::state().parameterValues[QDMI_DEVICE_SESSION_PARAMETER_REGION],
      "us-east-1");
  EXPECT_EQ(spank_test::state()
                .parameterValues[QDMI_DEVICE_SESSION_PARAMETER_RESERVATION_ARN],
            "reservation");
  EXPECT_EQ(
      spank_test::state().environment[AMAZON_BRAKET_QDMI_DEVICE_ENV_BASEURL],
      "forwarded-device");
  EXPECT_EQ(
      spank_test::state().environment[AMAZON_BRAKET_QDMI_DEVICE_ENV_AUTHFILE],
      "/forwarded/auth");
  EXPECT_EQ(
      spank_test::state().environment[AMAZON_BRAKET_QDMI_DEVICE_ENV_REGION],
      "us-east-1");
  EXPECT_EQ(spank_test::state()
                .environment[AMAZON_BRAKET_QDMI_DEVICE_ENV_RESERVATION_ARN],
            "reservation");
  ASSERT_EQ(spank_test::state().environmentOverwrites.size(), 4);
  for (const auto overwrite : spank_test::state().environmentOverwrites) {
    EXPECT_EQ(overwrite, 1);
  }
  EXPECT_EQ(spank_test::state().initializeCalls, 1);
}

TEST_F(SpankTest, RemoteAcceptsIdleAndBusyDevices) {
  for (const auto status : {QDMI_DEVICE_STATUS_IDLE, QDMI_DEVICE_STATUS_BUSY}) {
    spank_test::reset();
    const auto spank = initializeRemote();
    spank_test::configureOptIn();
    spank_test::state().deviceStatus = status;
    ASSERT_EQ(slurm_spank_init_post_opt(spank, 0, nullptr), 0);
    ASSERT_EQ(slurm_spank_user_init(spank, 0, nullptr), 0);
    EXPECT_EQ(slurm_spank_task_init(spank, 0, nullptr), 0);
  }
}

TEST_F(SpankTest, RemoteRejectsUnavailableDevices) {
  for (const auto status :
       {QDMI_DEVICE_STATUS_OFFLINE, QDMI_DEVICE_STATUS_MAINTENANCE}) {
    spank_test::reset();
    const auto spank = initializeRemote();
    spank_test::configureOptIn();
    spank_test::state().deviceStatus = status;
    ASSERT_EQ(slurm_spank_init_post_opt(spank, 0, nullptr), 0);
    ASSERT_EQ(slurm_spank_user_init(spank, 0, nullptr), 0);
    EXPECT_NE(slurm_spank_task_init(spank, 0, nullptr), 0);
    EXPECT_EQ(spank_test::state().sessionFreeCalls, 1);
    EXPECT_EQ(spank_test::state().finalizeCalls, 1);
  }
}

TEST_F(SpankTest, RemoteQdmiFailuresRejectAndCleanUp) {
  struct Failure {
    const char* name;
    int* result;
    int value;
    bool sessionWasAllocated;
  };

  const std::array failures = {
      Failure{"initialize", &spank_test::state().deviceInitializeResult,
              QDMI_ERROR_FATAL, false},
      Failure{"allocate", &spank_test::state().sessionAllocResult,
              QDMI_ERROR_OUTOFMEM, true},
      Failure{"parameter", &spank_test::state().setParameterResult,
              QDMI_ERROR_NOTSUPPORTED, true},
      Failure{"session init", &spank_test::state().sessionInitResult,
              QDMI_ERROR_PERMISSIONDENIED, true},
      Failure{"status query", &spank_test::state().queryStatusResult,
              QDMI_ERROR_FATAL, true},
  };

  for (const auto& failure : failures) {
    spank_test::reset();
    const auto spank = initializeRemote();
    spank_test::configureOptIn();
    *failure.result = failure.value;
    ASSERT_EQ(slurm_spank_init_post_opt(spank, 0, nullptr), 0) << failure.name;
    ASSERT_EQ(slurm_spank_user_init(spank, 0, nullptr), 0) << failure.name;
    EXPECT_NE(slurm_spank_task_init(spank, 0, nullptr), 0) << failure.name;
    EXPECT_EQ(spank_test::state().sessionFreeCalls,
              failure.sessionWasAllocated ? 1 : 0)
        << failure.name;
    EXPECT_EQ(spank_test::state().finalizeCalls,
              failure.sessionWasAllocated ? 1 : 0)
        << failure.name;
  }
}

TEST_F(SpankTest, EnvironmentFailureRejectsJob) {
  const auto spank = initializeRemote();
  spank_test::configureOptIn();
  spank_test::state().environmentResult = ESPANK_ERROR;
  ASSERT_EQ(slurm_spank_init_post_opt(spank, 0, nullptr), 0);
  ASSERT_EQ(slurm_spank_user_init(spank, 0, nullptr), 0);
  EXPECT_NE(slurm_spank_task_init(spank, 0, nullptr), 0);
  EXPECT_EQ(spank_test::state().finalizeCalls, 1);
}

TEST_F(SpankTest, LocalContextIsUnaffected) {
  const auto spank = initializeAllocator();
  spank_test::configureOptIn();
  spank_test::state().context = S_CTX_LOCAL;
  EXPECT_EQ(slurm_spank_init_post_opt(spank, 0, nullptr), 0);
  EXPECT_EQ(slurm_spank_user_init(spank, 0, nullptr), 0);
  EXPECT_EQ(slurm_spank_task_init(spank, 0, nullptr), 0);
  EXPECT_EQ(spank_test::state().initializeCalls, 0);
}

} // namespace
