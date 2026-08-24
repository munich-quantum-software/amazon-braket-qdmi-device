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

"""Offline tests for the Amazon Braket Qiskit backend."""

from __future__ import annotations

from types import SimpleNamespace
from typing import TYPE_CHECKING, cast

import pytest
from mqt.core.plugins.qiskit.backend import QDMIBackend
from qiskit import QuantumCircuit

from amazon.braket.qdmi import AMAZON_BRAKET_QDMI_DEVICE_ID
from amazon.braket.qdmi import qiskit as braket_qiskit
from amazon.braket.qdmi.qiskit import AmazonBraketBackend

if TYPE_CHECKING:
    from collections.abc import Mapping

    from mqt.core.qdmi import Device as QDMIDeviceHandle


@pytest.fixture
def backend_configuration(
    monkeypatch: pytest.MonkeyPatch,
) -> list[tuple[str, Mapping[str, object]]]:
    """Capture the QDMI constructor boundary without contacting AWS.

    Returns:
        Captured device-open calls.
    """
    calls: list[tuple[str, Mapping[str, object]]] = []
    device = SimpleNamespace(operations=lambda: ())

    def open_device(device_id: str, **parameters: object) -> object:
        calls.append((device_id, parameters))
        return device

    def initialize(
        _backend: QDMIBackend,
        *,
        device: object,
        device_id: str | None = None,
    ) -> None:
        assert device is not None
        if calls:
            assert device_id == calls[-1][0]
        else:
            assert device_id is None

    monkeypatch.setattr(braket_qiskit, "register_device", lambda _device_id: None)
    monkeypatch.setattr(braket_qiskit, "open_device", open_device)
    monkeypatch.setattr(QDMIBackend, "__init__", initialize)
    return calls


def test_opens_catalogue_device(
    backend_configuration: list[tuple[str, Mapping[str, object]]],
) -> None:
    """Open a concrete device with optional session overrides."""
    AmazonBraketBackend(
        "amazon.braket.sv1",
        region="eu-west-2",
        reservation_arn="arn:reservation",
    )

    assert backend_configuration == [
        (
            "amazon.braket.sv1",
            {"base_url": None, "custom2": "eu-west-2", "custom3": "arn:reservation"},
        )
    ]


def test_opens_generic_device(
    backend_configuration: list[tuple[str, Mapping[str, object]]],
) -> None:
    """Open the generic device with an explicit ARN."""
    AmazonBraketBackend(device_arn="arn:device")

    assert backend_configuration == [
        (
            AMAZON_BRAKET_QDMI_DEVICE_ID,
            {"base_url": "arn:device", "custom2": None, "custom3": None},
        )
    ]


def test_generic_device_requires_arn(
    backend_configuration: list[tuple[str, Mapping[str, object]]],
) -> None:
    """Reject an unconfigured generic device before opening it."""
    with pytest.raises(ValueError, match="device_arn"):
        AmazonBraketBackend()
    assert not backend_configuration


def test_reuses_open_device(
    backend_configuration: list[tuple[str, Mapping[str, object]]],
) -> None:
    """Keep a Slurm-selected QDMI session instead of opening it again."""
    device = cast("QDMIDeviceHandle", SimpleNamespace(operations=lambda: ()))
    AmazonBraketBackend(device=device)
    assert not backend_configuration


def test_open_device_rejects_session_overrides(
    backend_configuration: list[tuple[str, Mapping[str, object]]],
) -> None:
    """Reject session settings that cannot apply to an existing handle."""
    with pytest.raises(ValueError, match="cannot be combined"):
        AmazonBraketBackend(
            device=cast("QDMIDeviceHandle", object()),
            region="us-east-1",
        )
    assert not backend_configuration


@pytest.mark.parametrize(
    ("operation_name", "gate_name"),
    [
        ("ccnot", "ccx"),
        ("cphaseshift", "cp"),
        ("cphaseshift00", "cphaseshift00"),
        ("gpi", "gpi"),
        ("ms", "ms"),
        ("phaseshift", "p"),
        ("pswap", "pswap"),
        ("si", "sdg"),
        ("ti", "tdg"),
        ("v", "sx"),
        ("vi", "sxdg"),
        ("xx", "rxx"),
        ("xy", "xy"),
        ("yy", "ryy"),
        ("zz", "rzz"),
    ],
)
def test_maps_braket_operation_names(operation_name: str, gate_name: str) -> None:
    """Represent Braket OpenQASM operation names in the Qiskit target."""
    gate = AmazonBraketBackend._map_operation_to_gate(operation_name)  # ruff: ignore[private-member-access]
    assert gate is not None
    assert not isinstance(gate, type)
    assert gate.name == gate_name


def test_serializes_openqasm_without_includes() -> None:
    """Emit self-contained OpenQASM because Amazon Braket rejects includes."""
    circuit = QuantumCircuit(2, 2)
    circuit.h(0)
    circuit.cx(0, 1)
    circuit.measure([0, 1], [0, 1])

    program = braket_qiskit._serialize_to_braket_qasm3(  # ruff: ignore[private-member-access]
        circuit,
        {"cnot", "h", "measure"},
        AmazonBraketBackend._QISKIT_TO_QDMI_GATE_MAP,  # ruff: ignore[private-member-access]
    )

    assert "include" not in program
    assert "gate " not in program
    assert "cnot q[0], q[1]" in program
    assert "measure q[0]" in program
