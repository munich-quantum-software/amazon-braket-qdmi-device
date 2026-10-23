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

#include "CapabilityFixtures.hpp"
#include "amazon-braket-qdmi-device/Device.hpp"
#include "amazon-braket-qdmi-device/constants.hpp"
#include "amazon_braket_qdmi/device.h"

#include <aws/braket/BraketClient.h>
#include <aws/braket/BraketServiceClientModel.h>
#include <aws/braket/model/DeviceStatus.h>
#include <aws/braket/model/DeviceType.h>
#include <aws/braket/model/GetDeviceResult.h>
#include <aws/core/auth/AWSCredentials.h>
#include <aws/core/client/ClientConfiguration.h>
#include <gtest/gtest.h>
#include <memory>
#include <stdlib.h> // NOLINT(modernize-deprecated-headers): POSIX setenv
#include <string>
#include <string_view>

// Match the existing test friend without exporting private SDK types.
struct AMAZON_BRAKET_QDMI_Device_Session_TestAccess {
  class QueryClient final : public Aws::Braket::BraketClient {
  public:
    QueryClient()
        : Aws::Braket::BraketClient(
              Aws::Auth::AWSCredentials{"access-key", "secret-key"},
              configuration()) {}

    auto GetDevice(const Aws::Braket::Model::GetDeviceRequest& request) const
        -> Aws::Braket::Model::GetDeviceOutcome override {
      static_cast<void>(request);
      Aws::Braket::Model::GetDeviceResult result;
      result.SetDeviceName("IQM Garnet fixture");
      result.SetProviderName("IQM");
      result.SetDeviceType(Aws::Braket::Model::DeviceType::QPU);
      result.SetDeviceStatus(Aws::Braket::Model::DeviceStatus::ONLINE);
      result.SetDeviceCapabilities(
          std::string{amazon::braket::qdmi::test::IQM_GARNET}.c_str());
      return result;
    }

  private:
    static auto configuration() -> Aws::Client::ClientConfiguration {
      Aws::Client::ClientConfiguration config;
      config.region = "us-east-1";
      return config;
    }
  };

  static void stubQueries(AMAZON_BRAKET_QDMI_Device_Session session) {
    session->client_ = std::make_unique<QueryClient>();
  }
};

class QDMIQueryFixture : public ::testing::Test {
protected:
  static void SetUpTestSuite() {
    // This executable owns its environment; discovery never initializes AWS.
#ifdef _WIN32
    ASSERT_EQ(_putenv_s("AWS_EC2_METADATA_DISABLED", "true"), 0);
#else
    ASSERT_EQ(setenv("AWS_EC2_METADATA_DISABLED", "true", 1), 0);
#endif
    ASSERT_EQ(AMAZON_BRAKET_QDMI_device_initialize(), QDMI_SUCCESS);
  }

  static void TearDownTestSuite() {
    EXPECT_EQ(AMAZON_BRAKET_QDMI_device_finalize(), QDMI_SUCCESS);
  }

  void SetUp() override {
    ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_alloc(&session), QDMI_SUCCESS);
    constexpr std::string_view arn =
        "arn:aws:braket:us-east-1::device/qpu/iqm/Garnet";
    ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                  session,
                  AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN,
                  arn.size() + 1, arn.data()),
              QDMI_SUCCESS);
    ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_init(session), QDMI_SUCCESS);
    AMAZON_BRAKET_QDMI_Device_Session_TestAccess::stubQueries(session);
  }

  void TearDown() override {
    if (session != nullptr) {
      AMAZON_BRAKET_QDMI_device_session_free(session);
    }
  }

  AMAZON_BRAKET_QDMI_Device_Session session = nullptr;
};
