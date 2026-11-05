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

"""Tests for the Amazon Braket Qiskit backend, with an opt-in SV1 check."""

from __future__ import annotations

import os
from pathlib import Path
from types import SimpleNamespace
from typing import TYPE_CHECKING, cast

import pytest
from mqt.core.mlir import (
    CompilationOptions,
    CompilerTarget,
    MappingOptions,
    PayloadSpecification,
    TargetEnvironment,
    compile_program,
    serialize_program,
)
from mqt.core.plugins.qiskit.backend import QDMIBackend
from mqt.core.qdmi import ProgramFormat
from qiskit import ClassicalRegister, QuantumCircuit
from qiskit.circuit import Gate

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


def test_core_measure_all_transport_fixture() -> None:
    """Keep the native transport fixture tied to real Core compiler output."""
    target = CompilerTarget(
        "native fixture",
        [CompilerTarget.Site(site) for site in (1, 2, 5)],
        connectivity=CompilerTarget.Connectivity.all_to_all(),
        native_operations=CompilerTarget.NativeOperations([
            CompilerTarget.OperationCapability("prx", 1, 2),
            CompilerTarget.OperationCapability("cz", 2, 0),
            CompilerTarget.OperationCapability("measure", 1, 0),
        ]),
    )
    circuit = QuantumCircuit(2)
    circuit.rx(1.0, 0)
    circuit.cz(0, 1)
    circuit.measure_all()
    native = compile_program(
        circuit,
        target=target,
        options=CompilationOptions(seed=7, mapping=MappingOptions(trials=1)),
    )
    program = serialize_program(
        native, target=TargetEnvironment(target, PayloadSpecification.from_program_format(ProgramFormat.QASM3))
    )
    # Also submitted through the actual C++ transport in test_device_unit.cpp.
    fixtures = Path(__file__).parent / "fixtures"
    assert program.payload == (fixtures / "core-native.qasm").read_text()
    partial = QuantumCircuit(6, 3)
    partial.append(Gate("prx", 1, [1.0, 0.0]), [1])
    partial.cz(1, 2)
    partial.measure(5, 1)
    program = serialize_program(
        partial, target=TargetEnvironment(target, PayloadSpecification.from_program_format(ProgramFormat.QASM3))
    )
    assert program.payload == (fixtures / "core-partial.qasm").read_text()
    assert program.result_layout is not None
    assert program.result_layout.decode_bits("1") == "010"
    permuted = partial.copy()
    permuted.remove_final_measurements()
    permuted.add_register(ClassicalRegister(3, "out"))
    permuted.measure(5, 0)
    permuted.measure(1, 2)
    program = serialize_program(
        permuted, target=TargetEnvironment(target, PayloadSpecification.from_program_format(ProgramFormat.QASM3))
    )
    assert program.payload == (fixtures / "core-permuted.qasm").read_text()
    assert program.result_layout is not None
    assert program.result_layout.decode_bits("10") == "001"


def test_core_sv1_phase_transport_fixture(monkeypatch: pytest.MonkeyPatch) -> None:
    """Preserve native SV1 names when Core emits a sampled global phase."""
    # Supported gates from CapabilityFixtures.hpp's SV1 metadata. Arbitrary
    # unitary is not needed by this compiler target.
    names = ("h", "cnot", "rx", "ry", "rz", "measure", "gphase")
    target = CompilerTarget(
        "SV1 fixture",
        [CompilerTarget.Site(site) for site in range(4)],
        connectivity=CompilerTarget.Connectivity.all_to_all(),
        native_operations=CompilerTarget.NativeOperations([
            CompilerTarget.OperationCapability(
                name,
                0 if name == "gphase" else 2 if name == "cnot" else 1,
                int(name in {"rx", "ry", "rz", "gphase"}),
            )
            for name in names
        ]),
    )
    environment = TargetEnvironment(target, PayloadSpecification.from_program_format(ProgramFormat.QASM3))
    monkeypatch.setattr(TargetEnvironment, "from_device", lambda _device: environment)
    device = cast("QDMIDeviceHandle", SimpleNamespace(name=lambda: "SV1", version=lambda: "1.0"))
    backend = AmazonBraketBackend(device=device)
    assert backend.coupling_map is None
    circuit = QuantumCircuit(4, 4)
    circuit.x(3)
    circuit.cx(3, 0)
    circuit.measure(range(4), range(4))
    native = compile_program(
        circuit, target=target, options=CompilationOptions(seed=7, mapping=MappingOptions(trials=1))
    )
    program = serialize_program(
        native, target=TargetEnvironment(target, PayloadSpecification.from_program_format(ProgramFormat.QASM3))
    )
    assert program.payload == (Path(__file__).parent / "fixtures" / "core-sv1-phase.qasm").read_text()


def test_sv1_live() -> None:
    """Compile and submit one eight-shot circuit through the Qiskit backend."""
    if os.environ.get("AMAZON_BRAKET_PENNYLANE_LIVE") != "1":
        pytest.skip("Set AMAZON_BRAKET_PENNYLANE_LIVE=1 to run the SV1 smoke tests.")

    backend = AmazonBraketBackend("amazon.braket.sv1")
    circuit = QuantumCircuit(2)
    circuit.x(0)
    circuit.measure_all()

    native = backend.compile(circuit)
    assert backend.run(native, shots=8).result().get_counts() == {"01": 8}
