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

import pennylane as qp
from mqt.core.plugins.pennylane import QDMIDevice
from provider_probe import open_device_from_license  # ty: ignore[unresolved-import]
from qiskit import QuantumCircuit

from amazon.braket.qdmi.qiskit import AmazonBraketBackend


def main() -> None:
    """Reuse the licensed handle and retrieve results through each adapter."""
    device = open_device_from_license()

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
