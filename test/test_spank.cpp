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
#include "amazon_braket_qdmi/device.h"
#include "spank_test_doubles.hpp"

#include <array>
#include <gtest/gtest.h>
#include <slurm/slurm_errno.h>
#include <slurm/spank.h>
#include <string>

// These declarations mirror the required Slurm SPANK entry-point ABI.
// NOLINTBEGIN(readability-identifier-naming, cppcoreguidelines-avoid-c-arrays,
//             modernize-avoid-c-arrays)
int slurm_spank_init(spank_t, int, char*[]);
int slurm_spank_init_post_opt(spank_t, int, char*[]);
int slurm_spank_user_init(spank_t, int, char*[]);
int slurm_spank_task_init(spank_t, int, char*[]);
// NOLINTEND(readability-identifier-naming, cppcoreguidelines-avoid-c-arrays,
//           modernize-avoid-c-arrays)

namespace {

constexpr auto BASE_URL_OPTION = "qdmi-device-session-parameter-baseurl";
constexpr auto AUTH_FILE_OPTION = "qdmi-device-session-parameter-authfile";

class SpankTest : public ::testing::Test {
protected:
  static auto initialize(spank_t spank) -> void {
    ASSERT_EQ(slurm_spank_init(spank, 0, nullptr), 0);
  }

  static auto initializeRemote() -> spank_t {
    auto* spank = spank_test::beginRemote();
    initialize(spank);
    return spank;
  }

  static auto initializeAllocator() -> spank_t {
    auto* spank = spank_test::beginAllocator();
    initialize(spank);
    return spank;
  }

  void SetUp() override { spank_test::reset(); }
};

// GoogleTest owns the fixture objects created by TEST_F.
// NOLINTBEGIN(cppcoreguidelines-owning-memory)
TEST_F(SpankTest, RegistersAllOptions) {
  auto* const spank = initializeAllocator();
  ASSERT_EQ(spank_test::state().registeredOptions.size(), 4);

  const std::array expected = {
      BASE_URL_OPTION,
      "qdmi-device-session-parameter-region",
      "qdmi-device-session-parameter-reservation-arn",
      AUTH_FILE_OPTION,
  };
  for (const auto* name : expected) {
    ASSERT_NE(spank_test::registeredOption(name), nullptr);
  }
  (void)spank;
}

TEST_F(SpankTest, RejectsInvalidOptionValues) {
  initializeAllocator();
  auto* option = spank_test::registeredOption(BASE_URL_OPTION);
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
  auto* const spank = initializeAllocator();
  EXPECT_EQ(slurm_spank_init_post_opt(spank, 0, nullptr), 0);
  EXPECT_EQ(spank_test::state().initializeCalls, 0);
}

TEST_F(SpankTest, AllocatorRequiresBothOptInOptions) {
  auto* spank = initializeAllocator();
  auto* baseUrl = spank_test::registeredOption(BASE_URL_OPTION);
  auto* authFile = spank_test::registeredOption(AUTH_FILE_OPTION);
  ASSERT_NE(baseUrl, nullptr);
  ASSERT_NE(authFile, nullptr);

  ASSERT_EQ(baseUrl->cb(baseUrl->val, "device", 0), 0);
  EXPECT_NE(slurm_spank_init_post_opt(spank, 0, nullptr), 0);

  spank = initializeAllocator();
  authFile = spank_test::registeredOption(AUTH_FILE_OPTION);
  ASSERT_EQ(authFile->cb(authFile->val, "/tmp/credentials", 0), 0);
  EXPECT_NE(slurm_spank_init_post_opt(spank, 0, nullptr), 0);
}

TEST_F(SpankTest, AllocatorAcceptsCompleteOptInAndOptionalParameters) {
  auto* const spank = initializeAllocator();
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
  auto* const spank = initializeRemote();
  ASSERT_EQ(slurm_spank_init_post_opt(spank, 0, nullptr), 0);
  ASSERT_EQ(slurm_spank_user_init(spank, 0, nullptr), 0);
  EXPECT_EQ(slurm_spank_task_init(spank, 0, nullptr), 0);
  EXPECT_EQ(spank_test::state().initializeCalls, 0);
}

TEST_F(SpankTest, RemoteIncompleteOptInRejectsJob) {
  auto* const spank = initializeRemote();
  auto* baseUrl = spank_test::registeredOption(BASE_URL_OPTION);
  ASSERT_EQ(baseUrl->cb(baseUrl->val, "device", 0), 0);
  ASSERT_EQ(slurm_spank_init_post_opt(spank, 0, nullptr), 0);
  ASSERT_EQ(slurm_spank_user_init(spank, 0, nullptr), 0);
  EXPECT_LT(slurm_spank_task_init(spank, 0, nullptr), 0);
  EXPECT_EQ(spank_test::state().initializeCalls, 0);
}

TEST_F(SpankTest, RemoteValidationUsesForwardedOptionsAndInjectsEnvironment) {
  auto* const spank = initializeRemote();
  spank_test::configureOptIn();
  ASSERT_EQ(spank_test::registeredOption("qdmi-device-session-parameter-region")
                ->cb(2, "us-east-1", 0),
            0);
  ASSERT_EQ(spank_test::registeredOption(
                "qdmi-device-session-parameter-reservation-arn")
                ->cb(3, "reservation", 0),
            0);
  spank_test::state().forwardedOptions[BASE_URL_OPTION] = "forwarded-device";
  spank_test::state().forwardedOptions[AUTH_FILE_OPTION] = "/forwarded/auth";
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
    auto* const spank = initializeRemote();
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
    auto* const spank = initializeRemote();
    spank_test::configureOptIn();
    spank_test::state().deviceStatus = status;
    ASSERT_EQ(slurm_spank_init_post_opt(spank, 0, nullptr), 0);
    ASSERT_EQ(slurm_spank_user_init(spank, 0, nullptr), 0);
    EXPECT_LT(slurm_spank_task_init(spank, 0, nullptr), 0);
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
    bool shouldFinalize;
  };

  const std::array failures = {
      Failure{.name = "initialize",
              .result = &spank_test::state().deviceInitializeResult,
              .value = QDMI_ERROR_FATAL,
              .sessionWasAllocated = false,
              .shouldFinalize = false},
      Failure{.name = "allocate",
              .result = &spank_test::state().sessionAllocResult,
              .value = QDMI_ERROR_OUTOFMEM,
              .sessionWasAllocated = false,
              .shouldFinalize = true},
      Failure{.name = "parameter",
              .result = &spank_test::state().setParameterResult,
              .value = QDMI_ERROR_NOTSUPPORTED,
              .sessionWasAllocated = true,
              .shouldFinalize = true},
      Failure{.name = "session init",
              .result = &spank_test::state().sessionInitResult,
              .value = QDMI_ERROR_PERMISSIONDENIED,
              .sessionWasAllocated = true,
              .shouldFinalize = true},
      Failure{.name = "status query",
              .result = &spank_test::state().queryStatusResult,
              .value = QDMI_ERROR_FATAL,
              .sessionWasAllocated = true,
              .shouldFinalize = true},
  };

  for (const auto& failure : failures) {
    spank_test::reset();
    auto* const spank = initializeRemote();
    spank_test::configureOptIn();
    *failure.result = failure.value;
    ASSERT_EQ(slurm_spank_init_post_opt(spank, 0, nullptr), 0) << failure.name;
    ASSERT_EQ(slurm_spank_user_init(spank, 0, nullptr), 0) << failure.name;
    EXPECT_LT(slurm_spank_task_init(spank, 0, nullptr), 0) << failure.name;
    EXPECT_EQ(spank_test::state().sessionFreeCalls,
              failure.sessionWasAllocated ? 1 : 0)
        << failure.name;
    EXPECT_EQ(spank_test::state().finalizeCalls, failure.shouldFinalize ? 1 : 0)
        << failure.name;
  }
}

TEST_F(SpankTest, EnvironmentFailureRejectsJob) {
  auto* const spank = initializeRemote();
  spank_test::configureOptIn();
  spank_test::state().environmentResult = ESPANK_ERROR;
  ASSERT_EQ(slurm_spank_init_post_opt(spank, 0, nullptr), 0);
  ASSERT_EQ(slurm_spank_user_init(spank, 0, nullptr), 0);
  EXPECT_LT(slurm_spank_task_init(spank, 0, nullptr), 0);
  EXPECT_EQ(spank_test::state().finalizeCalls, 1);
}

TEST_F(SpankTest, LocalContextIsUnaffected) {
  auto* const spank = initializeAllocator();
  spank_test::configureOptIn();
  spank_test::state().context = S_CTX_LOCAL;
  EXPECT_EQ(slurm_spank_init_post_opt(spank, 0, nullptr), 0);
  EXPECT_EQ(slurm_spank_user_init(spank, 0, nullptr), 0);
  EXPECT_EQ(slurm_spank_task_init(spank, 0, nullptr), 0);
  EXPECT_EQ(spank_test::state().initializeCalls, 0);
}

// NOLINTEND(cppcoreguidelines-owning-memory)

} // namespace
