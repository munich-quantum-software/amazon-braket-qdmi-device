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

"""Offline tests for the Amazon Braket PennyLane specialization."""

from __future__ import annotations

from typing import TYPE_CHECKING

import pytest

try:
    import pennylane as qp
except ImportError:
    pytest.skip("Install the PennyLane extra to run these tests.", allow_module_level=True)

from mqt.core.plugins.pennylane import PennyLaneConfigurationError, QDMIDevice
from mqt.core.qdmi import driver

from amazon.braket.qdmi import AMAZON_BRAKET_QDMI_DEVICE_ID, _catalogue  # ruff: ignore[import-private-name]
from amazon.braket.qdmi import pennylane as braket_pennylane
from amazon.braket.qdmi.pennylane import (
    AmazonBraketAqtIbexQ1Device,
    AmazonBraketDevice,
    AmazonBraketDM1Device,
    AmazonBraketIonQForte1Device,
    AmazonBraketIonQForteEnterprise1Device,
    AmazonBraketIQMEmeraldDevice,
    AmazonBraketIQMGarnetDevice,
    AmazonBraketRigettiAnkaa3Device,
    AmazonBraketRigettiCepheus1108QDevice,
    AmazonBraketSV1Device,
)

if TYPE_CHECKING:
    from collections.abc import Mapping


@pytest.fixture
def device_configuration(
    monkeypatch: pytest.MonkeyPatch,
) -> list[tuple[str, Mapping[str, object], Mapping[str, object]]]:
    """Capture the provider-neutral constructor boundary without opening AWS.

    Returns:
        The captured device ID, session parameters, and job parameters.
    """
    calls: list[tuple[str, Mapping[str, object], Mapping[str, object]]] = []

    def initialize(
        _device: QDMIDevice,
        device_id: str,
        *_args: object,
        session_parameters: Mapping[str, object] | None = None,
        job_parameters: Mapping[str, object] | None = None,
        **_kwargs: object,
    ) -> None:
        calls.append((device_id, dict(session_parameters or {}), dict(job_parameters or {})))

    monkeypatch.setattr(QDMIDevice, "__init__", initialize)
    monkeypatch.setattr(braket_pennylane, "register_device", lambda _device_id: None)
    return calls


def test_configures_explicit_qdmi_parameters(
    device_configuration: list[tuple[str, Mapping[str, object], Mapping[str, object]]],
) -> None:
    """Translate explicit Braket options to QDMI session and job fields."""
    device = AmazonBraketDevice(
        "arn:aws:braket:::device/quantum-simulator/amazon/sv1",
        wires=4,
        shots=8,
        s3_destination_folder=("results", "/qaoa/run/"),
        region="eu-west-2",
        reservation_arn="arn:aws:braket:reservation/test",
    )

    assert device_configuration == [
        (
            AMAZON_BRAKET_QDMI_DEVICE_ID,
            {
                "base_url": "arn:aws:braket:::device/quantum-simulator/amazon/sv1",
                "custom2": "eu-west-2",
                "custom3": "arn:aws:braket:reservation/test",
            },
            {"custom1": "s3://results/qaoa/run"},
        )
    ]
    assert device.device_arn is not None
    assert device.device_arn.endswith("/amazon/sv1")
    assert device.s3_destination_folder == ("results", "/qaoa/run/")


def test_uses_native_configuration_fallbacks(
    device_configuration: list[tuple[str, Mapping[str, object], Mapping[str, object]]],
) -> None:
    """Leave AWS credentials, Region, reservation, and S3 fallback to the native device."""
    device = AmazonBraketDevice("arn:device", wires=2)

    assert device_configuration == [(AMAZON_BRAKET_QDMI_DEVICE_ID, {"base_url": "arn:device"}, {})]
    assert device.s3_destination_folder is None


@pytest.mark.parametrize(
    ("device_arn", "destination", "match"),
    [
        ("", ("results", "run"), "device_arn"),
        ("arn:device", ("", "run"), "bucket"),
        ("arn:device", ("ab", "run"), "bucket"),
        ("arn:device", ("Results", "run"), "bucket"),
        ("arn:device", ("s3://results", "run"), "bucket"),
        ("arn:device", ("results..archive", "run"), "bucket"),
        ("arn:device", ("results", "/"), "prefix"),
        ("arn:device", ("results", "run\nnext"), "prefix"),
    ],
)
def test_rejects_invalid_configuration(
    device_configuration: list[tuple[str, Mapping[str, object], Mapping[str, object]]],
    device_arn: str,
    destination: tuple[str, str],
    match: str,
) -> None:
    """Reject invalid explicit configuration before opening the native device."""
    with pytest.raises(PennyLaneConfigurationError, match=match):
        AmazonBraketDevice(device_arn, wires=2, s3_destination_folder=destination)
    assert not device_configuration


def test_stable_pennylane_entry_point(
    device_configuration: list[tuple[str, Mapping[str, object], Mapping[str, object]]],
) -> None:
    """Construct the specialization through its stable PennyLane device ID."""
    device = qp.device(
        AMAZON_BRAKET_QDMI_DEVICE_ID,
        device_arn="arn:device",
        wires=["left", "right"],
        shots=5,
    )

    assert isinstance(device, AmazonBraketDevice)
    assert device_configuration


@pytest.mark.parametrize(
    ("device_id", "device_type"),
    [
        ("amazon.braket.aqt.ibex-q1", AmazonBraketAqtIbexQ1Device),
        ("amazon.braket.ionq.forte-1", AmazonBraketIonQForte1Device),
        ("amazon.braket.ionq.forte-enterprise-1", AmazonBraketIonQForteEnterprise1Device),
        ("amazon.braket.iqm.garnet", AmazonBraketIQMGarnetDevice),
        ("amazon.braket.iqm.emerald", AmazonBraketIQMEmeraldDevice),
        ("amazon.braket.rigetti.ankaa-3", AmazonBraketRigettiAnkaa3Device),
        ("amazon.braket.rigetti.cepheus-1-108q", AmazonBraketRigettiCepheus1108QDevice),
        ("amazon.braket.sv1", AmazonBraketSV1Device),
        ("amazon.braket.dm1", AmazonBraketDM1Device),
    ],
)
def test_catalogue_pennylane_entry_points(
    device_configuration: list[tuple[str, Mapping[str, object], Mapping[str, object]]],
    device_id: str,
    device_type: type[AmazonBraketDevice],
) -> None:
    """Construct every concrete catalogue entry without repeating its ARN."""
    device = qp.device(device_id, wires=2, shots=5)

    assert isinstance(device, device_type)
    assert device.device_arn is None
    assert device_configuration == [(device_id, {}, {})]


def test_overrides_catalogue_configuration(
    device_configuration: list[tuple[str, Mapping[str, object], Mapping[str, object]]],
) -> None:
    """Override catalogue defaults through the same provider-specific options."""
    device = AmazonBraketSV1Device(
        "arn:override",
        wires=2,
        region="eu-west-2",
        reservation_arn="arn:reservation",
        s3_destination_folder=("results", "run"),
    )

    assert device.device_arn == "arn:override"
    assert device_configuration == [
        (
            "amazon.braket.sv1",
            {"base_url": "arn:override", "custom2": "eu-west-2", "custom3": "arn:reservation"},
            {"custom1": "s3://results/run"},
        )
    ]


def test_registers_packaged_qdmi_catalogue() -> None:
    """Register every packaged definition under its stable PennyLane device ID."""
    device_ids = {
        AMAZON_BRAKET_QDMI_DEVICE_ID,
        "amazon.braket.aqt.ibex-q1",
        "amazon.braket.ionq.forte-1",
        "amazon.braket.ionq.forte-enterprise-1",
        "amazon.braket.iqm.garnet",
        "amazon.braket.iqm.emerald",
        "amazon.braket.rigetti.ankaa-3",
        "amazon.braket.rigetti.cepheus-1-108q",
        "amazon.braket.sv1",
        "amazon.braket.dm1",
    }
    for device_id in device_ids:
        _catalogue.register_device(device_id)

    assert device_ids <= set(driver.registered_device_ids())


def test_reads_catalogue_session_defaults() -> None:
    """Preserve the catalogue's ARN and Region when registering concrete IDs."""
    assert _catalogue.catalogue_session(AMAZON_BRAKET_QDMI_DEVICE_ID) == {}
    assert _catalogue.catalogue_session("amazon.braket.sv1") == {
        "base-url": "arn:aws:braket:::device/quantum-simulator/amazon/sv1",
        "custom2": "us-east-1",
    }
