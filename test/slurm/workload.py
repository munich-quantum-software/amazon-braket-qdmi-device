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

"""Run eight-shot Bell circuits through both SDKs in one Slurm allocation."""

from __future__ import annotations

import json
import os
from pathlib import Path

import pennylane as qp
from mqt.core.plugins.pennylane import QDMIDevice
from mqt.core.qdmi import builtin_driver as qdmi_driver
from qiskit import QuantumCircuit, transpile

from amazon.braket.qdmi.qiskit import AmazonBraketBackend

# This workload requests one local Slurm license with count one.
device_id = os.environ["SLURM_JOB_LICENSES"].removesuffix(":1")
device = qdmi_driver.open_device(device_id)
catalogue = json.loads(Path(os.environ["MQT_CORE_QDMI_CONFIG_FILE"]).read_text(encoding="utf-8"))
library = next(item["library"] for item in catalogue["qdmi"]["devices"] if item["id"] == device_id)
assert str(Path(library).resolve()) in Path("/proc/self/maps").read_text(encoding="utf-8")

backend = AmazonBraketBackend(device=device)
circuit = QuantumCircuit(2)
circuit.h(0)
circuit.cx(0, 1)
circuit.measure_all()
qiskit_counts = backend.run(transpile(circuit, backend), shots=8).result().get_counts()

adapter = QDMIDevice(device=device, wires=2)


@qp.qnode(adapter, shots=8)
def bell() -> qp.measurements.CountsMP:
    """Prepare a Bell state.

    Returns:
        Computational-basis counts.
    """
    qp.Hadamard(0)
    qp.CNOT(wires=[0, 1])
    return qp.counts(wires=[0, 1])


for counts in (qiskit_counts, bell()):
    assert sum(counts.values()) == 8
    assert set(counts) <= {"00", "11"}
