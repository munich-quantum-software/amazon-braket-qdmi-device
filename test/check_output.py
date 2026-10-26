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

# ruff: file-ignore[implicit-namespace-package]

"""Compare native output reconstruction against independent Qiskit simulation."""

from __future__ import annotations

import argparse
import json
import subprocess
from collections import Counter
from pathlib import Path

from braket.devices import LocalSimulator  # ty: ignore[unresolved-import] - optional offline SDK check
from braket.ir.openqasm import Program  # ty: ignore[unresolved-import] - optional offline SDK check
from qiskit import ClassicalRegister, QuantumCircuit, QuantumRegister, qasm2
from qiskit.providers.basic_provider import BasicSimulator


def main() -> None:
    """Check asymmetric states, holes, overwrites, and nontrivial amplitudes."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("probe", type=Path)
    args = parser.parse_args()

    def native(request: dict) -> dict:
        response = subprocess.run(  # ruff: ignore[subprocess-without-shell-equals-true] - explicit local test binary
            [str(args.probe.resolve())],
            input=json.dumps(request),
            text=True,
            capture_output=True,
            check=True,
            timeout=30,
        )
        return json.loads(response.stdout)

    circuits = []
    for value in range(16):
        circuit = QuantumCircuit(4, 4)
        for index in range(4):
            if value & (1 << index):
                circuit.x(index)
        circuit.measure(range(4), reversed(range(4)))
        circuits.append(circuit)
    circuit = QuantumCircuit(QuantumRegister(2, "q"), ClassicalRegister(3, "a"), ClassicalRegister(2, "b"))
    circuit.x(0)
    circuit.measure(0, 4)
    circuit.measure(0, 1)
    circuit.measure(1, 1)
    circuits.append(circuit)
    circuit = QuantumCircuit(3, 5)
    circuit.ry(0.71, 0)
    circuit.rx(1.1e-5, 1)
    circuit.cx(0, 2)
    circuit.rz(-0.37, 2)
    circuit.measure([0, 1, 2], [4, 0, 2])
    circuits.append(circuit)
    for index, circuit in enumerate(circuits):
        source = qasm2.dumps(circuit)
        prepared = native({"source": source})["source"]
        result = LocalSimulator().run(Program(source=prepared), shots=4096).result()
        expected = BasicSimulator().run(circuit, shots=4096, seed_simulator=17).result().get_counts()
        expected = Counter({key.replace(" ", ""): value for key, value in expected.items()})
        for reverse in (False, True):
            order = list(range(len(result.measured_qubits)))
            if reverse:
                order.reverse()
            readout = {
                "measuredQubits": [result.measured_qubits[i] for i in order],
                "measurements": result.measurements[:, order].tolist(),
            }
            actual = Counter(native({"source": source, "shots": 4096, "result": readout})["shots"])
            distance = sum(abs(actual[key] - expected[key]) for key in actual.keys() | expected.keys()) / 8192
            assert distance < (0.04 if index == len(circuits) - 1 else 1e-12), (index, actual, expected)


if __name__ == "__main__":
    main()
