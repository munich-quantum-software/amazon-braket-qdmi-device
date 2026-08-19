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

#include <array>
#include <cstdlib>
#include <gtest/gtest.h>
#include <string_view>

namespace {
struct CatalogEntry {
  std::string_view id;
  std::string_view arn;
  std::string_view region;
};

constexpr std::array CATALOG{
    CatalogEntry{.id = "amazon.braket.aqt.ibex-q1",
                 .arn = "arn:aws:braket:eu-north-1::device/qpu/aqt/Ibex-Q1",
                 .region = "eu-north-1"},
    CatalogEntry{.id = "amazon.braket.ionq.forte-1",
                 .arn = "arn:aws:braket:us-east-1::device/qpu/ionq/Forte-1",
                 .region = "us-east-1"},
    CatalogEntry{
        .id = "amazon.braket.ionq.forte-enterprise-1",
        .arn = "arn:aws:braket:us-east-1::device/qpu/ionq/Forte-Enterprise-1",
        .region = "us-east-1"},
    CatalogEntry{.id = "amazon.braket.iqm.garnet",
                 .arn = "arn:aws:braket:eu-north-1::device/qpu/iqm/Garnet",
                 .region = "eu-north-1"},
    CatalogEntry{.id = "amazon.braket.iqm.emerald",
                 .arn = "arn:aws:braket:eu-north-1::device/qpu/iqm/Emerald",
                 .region = "eu-north-1"},
    CatalogEntry{.id = "amazon.braket.rigetti.ankaa-3",
                 .arn = "arn:aws:braket:us-west-1::device/qpu/rigetti/Ankaa-3",
                 .region = "us-west-1"},
    CatalogEntry{
        .id = "amazon.braket.rigetti.cepheus-1-108q",
        .arn = "arn:aws:braket:us-west-1::device/qpu/rigetti/Cepheus-1-108Q",
        .region = "us-west-1"},
    CatalogEntry{.id = "amazon.braket.sv1",
                 .arn = "arn:aws:braket:::device/quantum-simulator/amazon/sv1",
                 .region = "us-east-1"},
    CatalogEntry{.id = "amazon.braket.dm1",
                 .arn = "arn:aws:braket:::device/quantum-simulator/amazon/dm1",
                 .region = "us-east-1"},
};

[[nodiscard]] auto isEnabled(const char* variable) -> bool {
  const auto* value = std::getenv(variable);
  return value != nullptr && std::string_view{value} == "1";
}
} // namespace

TEST(AmazonBraketQDMILiveTest, OpensInstalledCatalog) {
  if (!isEnabled("AMAZON_BRAKET_QDMI_RUN_LIVE_CATALOG")) {
    // NOLINTNEXTLINE(readability-implicit-bool-conversion)
    GTEST_SKIP() << "Set AMAZON_BRAKET_QDMI_RUN_LIVE_CATALOG=1 to query the "
                    "current Amazon Braket catalog.";
  }

  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_initialize(), QDMI_SUCCESS);
  for (const auto& entry : CATALOG) {
    AMAZON_BRAKET_QDMI_Device_Session session = nullptr;
    ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_alloc(&session), QDMI_SUCCESS)
        << entry.id;
    ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                  session,
                  AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN,
                  entry.arn.size() + 1, entry.arn.data()),
              QDMI_SUCCESS)
        << entry.id;
    ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                  session, AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_REGION,
                  entry.region.size() + 1, entry.region.data()),
              QDMI_SUCCESS)
        << entry.id;
    ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_init(session), QDMI_SUCCESS)
        << entry.id;
    size_t nameSize = 0;
    EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_query_device_property(
                  session, QDMI_DEVICE_PROPERTY_NAME, 0, nullptr, &nameSize),
              QDMI_SUCCESS)
        << entry.id;
    EXPECT_GT(nameSize, 1U) << entry.id;
    AMAZON_BRAKET_QDMI_device_session_free(session);
  }
  AMAZON_BRAKET_QDMI_device_finalize();
}
