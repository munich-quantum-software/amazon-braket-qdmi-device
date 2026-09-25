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

"""Offline parity through the native adapter and Braket's local simulator.

Build the native test targets first, then run this file with amazon-braket-sdk
and amazon-braket-default-simulator installed. No AWS credentials are used.
"""

from __future__ import annotations

import json
import subprocess
from collections import Counter
from pathlib import Path
from typing import Any
from unittest.mock import Mock

import numpy as np
import pennylane as qp
import pytest
from braket.devices import LocalSimulator
from braket.ir.openqasm import Program
from braket.tasks import GateModelQuantumTaskResult
from mqt.core.plugins.pennylane import QDMIDevice
from mqt.core.plugins.qiskit import program_serializer
from mqt.core.plugins.qiskit.job import QDMIJob
from mqt.core.qdmi import Device, Job, ProgramFormat
from qiskit import ClassicalRegister, QuantumCircuit, QuantumRegister
from qiskit.providers.basic_provider import BasicSimulator

from amazon.braket.qdmi.qiskit import AmazonBraketBackend

PROBE = Path(__file__).parents[2] / "build" / "test" / "amazon-braket-output-probe"
if not PROBE.is_file() and PROBE.with_suffix(".exe").is_file():
    PROBE = PROBE.with_suffix(".exe")


def _probe(request: dict[str, Any]) -> dict[str, Any]:
    """Run the same source preparation and result reconstruction as the device.

    Returns:
        The native adapter's JSON response.
    """
    completed = subprocess.run(  # ruff: ignore[subprocess-without-shell-equals-true] Fixed native test executable.
        [str(PROBE)],
        input=json.dumps(request),
        capture_output=True,
        text=True,
        check=True,
    )
    return json.loads(completed.stdout)


def _run(source: str, shots: int) -> Job:
    """Execute the prepared source locally and normalize the SDK response.

    Returns:
        A completed QDMI job boundary carrying the adapter's genuine samples.
    """
    prepared = _probe({"source": source})
    result = LocalSimulator().run(Program(source=prepared["source"], inputs={}), shots=shots).result()
    assert isinstance(result, GateModelQuantumTaskResult)
    assert result.measurements is not None
    normalized = _probe({
        "source": source,
        "shots": shots,
        "result": {"measuredQubits": result.measured_qubits, "measurements": result.measurements.tolist()},
    })
    assert normalized["binary"]
    handle = Mock(spec=Job)
    handle.id = "local-braket"
    handle.check.return_value = Job.Status.DONE
    handle.get_shots.return_value = normalized["shots"]
    handle.get_counts.return_value = dict(Counter(normalized["shots"]))
    return handle


@pytest.mark.parametrize("memory", [False, True])
@pytest.mark.parametrize("serializer_kind", ["core", "braket"])
def test_qiskit_classical_outputs_match_basic_simulator(*, memory: bool, serializer_kind: str) -> None:
    """Keep register widths, unwritten bits, and overwritten destinations."""
    circuit = QuantumCircuit(QuantumRegister(3), ClassicalRegister(3, "a"), ClassicalRegister(2, "b"))
    circuit.x(0)
    circuit.cx(0, 1)
    circuit.measure(0, 2)
    circuit.measure(1, 4)
    circuit.measure(2, 2)
    backend = Mock()
    backend.name = "local-braket"
    backend.backend_version = "test"
    backend.target.operation_names = ["x", "cx", "measure"]
    serializer = program_serializer(ProgramFormat.QASM3)
    assert serializer is not None
    if serializer_kind == "core":
        source = serializer(circuit, backend)
    else:
        braket_backend = object.__new__(AmazonBraketBackend)
        device = Mock(spec=Device)
        operations = []
        for name in ("x", "cnot", "measure"):
            operation = Mock()
            operation.name.return_value = name
            operations.append(operation)
        device.operations.return_value = operations
        braket_backend._device = device  # ruff: ignore[private-member-access] Exercise the actual serializer offline.
        source, _ = braket_backend._serialize_circuit(  # ruff: ignore[private-member-access]
            circuit, [ProgramFormat.QASM3]
        )
    assert isinstance(source, str)
    handle = _run(source, 8)
    actual = QDMIJob(backend, [handle], [circuit], shots=8, memory=memory).result()
    expected = BasicSimulator().run(circuit, shots=8, memory=memory).result()
    assert actual.get_counts() == expected.get_counts() == {"10 000": 8}
    if memory:
        assert actual.get_memory() == expected.get_memory()


def test_pennylane_wire_subsets_match_default_qubit() -> None:
    """Run the actual PennyLane converter and sample reconstruction around Braket."""
    device = Mock(spec=Device)
    device.name.return_value = "local-braket"
    device.qubits_num.return_value = 3
    device.coupling_map.return_value = None
    device.supported_program_formats.return_value = [ProgramFormat.QASM3]
    operations = []
    for name, width in [("x", 1), ("cnot", 2)]:
        operation = Mock()
        operation.name.return_value = name
        operation.qubits_num.return_value = width
        operation.parameters_num.return_value = 0
        operation.sites.return_value = None
        operation.site_pairs.return_value = None
        operations.append(operation)
    device.operations.return_value = operations
    device.submit_job.side_effect = lambda program, _format, shots, **_kwargs: _run(program, shots)
    actual_device = QDMIDevice(device=device, wires=["left", "middle", "right"])
    reference = qp.device("default.qubit", wires=actual_device.wires)
    tape = qp.tape.QuantumScript(
        [qp.PauliX("left"), qp.CNOT(wires=["left", "right"])],
        [
            qp.counts(wires=["right", "middle", "left"]),
            qp.sample(wires=["middle", "right"]),
            qp.probs(wires=["middle", "left"]),
        ],
        shots=8,
    )
    (actual,) = qp.execute([tape], actual_device, diff_method=None)
    (expected,) = qp.execute([tape], reference, diff_method=None)
    assert actual[0] == expected[0] == {"101": 8}
    np.testing.assert_array_equal(actual[1], expected[1])
    np.testing.assert_array_equal(actual[2], expected[2])
