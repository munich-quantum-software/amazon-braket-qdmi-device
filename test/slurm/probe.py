# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This program is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
# Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program. If not, see <https://www.gnu.org/licenses/>.

"""Submit eight-shot SV1 circuits through both adapters."""

from __future__ import annotations

import json
import os
import sys
from pathlib import Path

import pennylane as qp
from mqt.core.plugins.pennylane import QDMIDevice
from mqt.core.qdmi import builtin_driver
from qiskit import QuantumCircuit

from amazon.braket.qdmi.qiskit import AmazonBraketBackend


def main() -> None:
    """Retrieve results through both adapters for one QDMI device."""
    device_id = sys.argv[1]
    device = builtin_driver.open_device(device_id)
    catalogue = json.loads(Path(os.environ["MQT_CORE_QDMI_CONFIG_FILE"]).read_text(encoding="utf-8"))
    library = next(item["library"] for item in catalogue["qdmi"]["devices"] if item["id"] == device_id)
    assert str(Path(library).resolve()) in Path("/proc/self/maps").read_text(encoding="utf-8")

    backend = AmazonBraketBackend(device=device)
    circuit = QuantumCircuit(2)
    circuit.h(0)
    circuit.cx(0, 1)
    circuit.measure_all()
    counts = backend.run(circuit, shots=8).result().get_counts()
    assert sum(counts.values()) == 8
    assert set(counts) <= {"00", "11"}

    adapter = QDMIDevice(device=device, wires=2)

    @qp.qnode(adapter, shots=8)
    def bell() -> qp.measurements.CountsMP:
        qp.Hadamard(0)
        qp.CNOT(wires=[0, 1])
        return qp.counts(wires=[0, 1])

    counts = bell()
    assert sum(counts.values()) == 8
    assert set(counts) <= {"00", "11"}


if __name__ == "__main__":
    main()
