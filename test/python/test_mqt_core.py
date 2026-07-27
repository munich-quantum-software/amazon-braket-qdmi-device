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

from mqt.core.fomac import DeviceDefinition, register_device_if_absent

from amazon.braket.qdmi import (
    AMAZON_BRAKET_QDMI_DEVICE_ID,
    AMAZON_BRAKET_QDMI_LIBRARY_PATH,
    AMAZON_BRAKET_QDMI_PREFIX,
)


def test_register_device_if_absent() -> None:
    """Register the packaged device without loading it or contacting AWS."""
    definition = DeviceDefinition(
        AMAZON_BRAKET_QDMI_DEVICE_ID,
        AMAZON_BRAKET_QDMI_LIBRARY_PATH,
        AMAZON_BRAKET_QDMI_PREFIX,
    )

    assert register_device_if_absent(definition)
    assert not register_device_if_absent(definition)
