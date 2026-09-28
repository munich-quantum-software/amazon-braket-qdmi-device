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

import json
from importlib.metadata import distribution

from amazon.braket.qdmi import AMAZON_BRAKET_QDMI_CATALOG_PATH


def test_installed_manifest_discovery() -> None:
    """Advertise installed metadata without opening a device or contacting AWS."""
    dist = distribution("amazon-braket-qdmi")
    entries = [entry for entry in dist.entry_points if entry.group == "mqt.core.qdmi.manifests"]
    assert [(entry.name, entry.value) for entry in entries] == [("braket", "amazon.braket.qdmi")]
    devices = json.loads(AMAZON_BRAKET_QDMI_CATALOG_PATH.read_text())["qdmi"]["devices"]
    assert len({device["id"] for device in devices}) == len(devices)
    sv1 = next(device for device in devices if device["id"] == "amazon.braket.sv1")
    assert sv1["session"] == {
        "base-url": "arn:aws:braket:::device/quantum-simulator/amazon/sv1",
        "custom2": "us-east-1",
    }
