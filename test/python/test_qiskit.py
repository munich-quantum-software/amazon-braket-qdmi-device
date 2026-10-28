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
from types import SimpleNamespace
from typing import TYPE_CHECKING, cast

import pytest
from mqt.core.mlir import (
    CompilationOptions,
    CompilerTarget,
    MappingOptions,
    TerminalReadout,
    compile_program,
    serialize_program,
)
from mqt.core.plugins.qiskit.backend import QDMIBackend
from mqt.core.qdmi import CustomProperty, ProgramFormat
from qiskit import QuantumCircuit
from qiskit.transpiler import CouplingMap, Target

from amazon.braket.qdmi import AMAZON_BRAKET_QDMI_DEVICE_ID
from amazon.braket.qdmi import qiskit as braket_qiskit
from amazon.braket.qdmi.qiskit import AmazonBraketBackend

if TYPE_CHECKING:
    from collections.abc import Mapping

    from mqt.core.mlir import CompiledProgram
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


@pytest.mark.parametrize(
    ("receiver", "terminal_readout", "physical_qubits"),
    [("simulator", TerminalReadout.ALL, False), ("native", TerminalReadout.MEASURED, True)],
)
def test_serializes_native_sites_with_result_layout(
    monkeypatch: pytest.MonkeyPatch, receiver: str, terminal_readout: TerminalReadout, *, physical_qubits: bool
) -> None:
    """Pass native sites and noncanonical readout unchanged to the shared serializer."""
    circuit = QuantumCircuit(6, 2)
    circuit.x(5)
    circuit.measure([5, 2], [0, 1])
    device = cast(
        "QDMIDeviceHandle",
        SimpleNamespace(
            query_custom_property=lambda prop, kind: (
                receiver if prop is CustomProperty.CUSTOM2 and kind is str else None
            )
        ),
    )
    serialized = cast("CompiledProgram", object())

    def serialize(native: QuantumCircuit, **options: object) -> CompiledProgram:
        assert native is circuit
        assert options == {
            "target": device,
            "program_format": ProgramFormat.QASM3,
            "terminal_readout": terminal_readout,
            "physical_qubits": physical_qubits,
        }
        return serialized

    monkeypatch.setattr(braket_qiskit, "serialize_program", serialize)
    backend = object.__new__(AmazonBraketBackend)
    backend._device = device

    result, program_format = backend._serialize_circuit(circuit, [ProgramFormat.QASM3])

    assert result is serialized  # Keep the payload and result layout together.
    assert program_format is ProgramFormat.QASM3


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
        native, target=target, program_format=ProgramFormat.QASM3, terminal_readout=TerminalReadout.MEASURED
    )
    # Also submitted through the actual C++ transport in test_device_unit.cpp.
    assert program.payload == (
        'OPENQASM 3;\ninclude "stdgates.inc";\n\nbit[2] c;\n\nprx(1.0, 0.0) $1;\n'
        "cz $1, $2;\nbarrier $1, $2;\nc[0] = measure $1;\nc[1] = measure $2;\n"
    )


@pytest.mark.filterwarnings(
    "default:Cannot represent custom or unsupported operation for .*; omitting it from the compiler target$:UserWarning"
)
def test_sv1_live() -> None:
    """Compile and submit one eight-shot circuit through the Qiskit backend."""
    if os.environ.get("AMAZON_BRAKET_PENNYLANE_LIVE") != "1":
        pytest.skip("Set AMAZON_BRAKET_PENNYLANE_LIVE=1 to run the SV1 smoke tests.")

    backend = AmazonBraketBackend("amazon.braket.sv1")
    circuit = QuantumCircuit(2)
    circuit.x(0)
    circuit.measure_all()

    native = compile_program(circuit, target=backend)
    assert backend.run(native, shots=8).result().get_counts() == {"01": 8}


def test_adds_variable_width_barrier_to_target(monkeypatch: pytest.MonkeyPatch) -> None:
    """Expose Qiskit's backend-level barrier without claiming native reset support."""
    monkeypatch.setattr(QDMIBackend, "_build_target", lambda _backend: Target(num_qubits=3))

    target = object.__new__(AmazonBraketBackend)._build_target()  # ruff: ignore[private-member-access]

    assert target.instruction_supported("barrier", (0,))
    assert target.instruction_supported("barrier", (0, 1, 2))
    assert "reset" not in target.operation_names


def test_reports_all_to_all_target_without_connectivity_constraint(monkeypatch: pytest.MonkeyPatch) -> None:
    """Let Qiskit transpile native multi-qubit gates on an all-to-all device."""
    monkeypatch.setattr(QDMIBackend, "coupling_map", property(lambda _backend: CouplingMap.from_full(3)))

    backend = object.__new__(AmazonBraketBackend)
    assert backend.coupling_map is None

    line = CouplingMap.from_line(3)
    monkeypatch.setattr(QDMIBackend, "coupling_map", property(lambda _backend: line))
    assert backend.coupling_map is line


@pytest.mark.parametrize("receiver", [None, "unknown"])
def test_rejects_unknown_receiver(receiver: str | None) -> None:
    """Do not guess virtual or physical addressing from an ARN or device name."""
    backend = object.__new__(AmazonBraketBackend)
    backend._device = cast("QDMIDeviceHandle", SimpleNamespace(query_custom_property=lambda *_args: receiver))
    with pytest.raises(ValueError, match="receiver"):
        backend._serialize_circuit(QuantumCircuit(1), [ProgramFormat.QASM3])
