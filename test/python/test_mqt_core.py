# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# Licensed under the Apache License v2.0 with LLVM Exceptions (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# https://llvm.org/LICENSE.txt
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
#
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

"""Hermetic integration tests for MQT Core's stable QDMI device registry."""

from __future__ import annotations

from typing import TYPE_CHECKING

from mqt.core.qdmi import driver

from amazon.braket.qdmi import (
    AMAZON_BRAKET_QDMI_LIBRARY_PATH,
    AMAZON_BRAKET_QDMI_PREFIX,
)

if TYPE_CHECKING:
    import pytest

TEST_DEVICE_ID = "test.amazon.braket.packaged"


def test_register_device_if_absent() -> None:
    """Register the packaged device without loading it or contacting AWS."""
    definition = driver.DeviceDefinition(
        TEST_DEVICE_ID,
        AMAZON_BRAKET_QDMI_LIBRARY_PATH,
        AMAZON_BRAKET_QDMI_PREFIX,
    )

    assert driver.register_device_if_absent(definition)
    assert not driver.register_device_if_absent(definition)


def test_query_local_duration_unit(monkeypatch: pytest.MonkeyPatch) -> None:
    """Query a native property through Core without contacting AWS."""
    monkeypatch.setenv("AWS_EC2_METADATA_DISABLED", "true")
    device_id = "test.amazon.braket.properties"
    driver.register_device_if_absent(
        driver.DeviceDefinition(device_id, AMAZON_BRAKET_QDMI_LIBRARY_PATH, AMAZON_BRAKET_QDMI_PREFIX)
    )
    device = driver.open_device(
        device_id,
        base_url="arn:aws:braket:::device/quantum-simulator/amazon/sv1",
        custom2="us-east-1",
    )

    assert device.duration_unit() == "us"
