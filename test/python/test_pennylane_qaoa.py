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

"""End-to-end checks for sampled MaxCut QAOA through QDMI."""

# ruff: file-ignore[missing-return-type-private-function]

from __future__ import annotations

import os
import platform
import sys
import time
from dataclasses import dataclass

import pytest

try:
    import pennylane as qp
except ImportError:
    pytest.skip("Install the PennyLane extra to run these tests.", allow_module_level=True)

import networkx as nx
import numpy as np
from mqt.core.plugins.pennylane import QDMIDevice

GRAPH = nx.Graph([(0, 1), (0, 2), (1, 2), (2, 3)])
COST_HAMILTONIAN, MIXER_HAMILTONIAN = qp.qaoa.maxcut(GRAPH)
QAOA_JOB_BUDGET = 20


@dataclass(frozen=True)
class QAOAResult:
    """Summary of one sampled QAOA run."""

    initial_cost: float
    initial_gradient: tuple[float, float]
    final_cost: float
    parameters: tuple[float, float]
    best_bitstring: str
    best_cut: int
    jobs: int
    elapsed: float


def _cut_value(bitstring: str) -> int:
    """Return the number of graph edges cut by a bit string."""
    return sum(bitstring[first] != bitstring[second] for first, second in GRAPH.edges)


def _ansatz(parameters: np.ndarray) -> None:
    """Prepare one QAOA layer."""
    for wire in GRAPH.nodes:
        qp.Hadamard(wire)
    qp.qaoa.cost_layer(parameters[0], COST_HAMILTONIAN)
    qp.qaoa.mixer_layer(parameters[1], MIXER_HAMILTONIAN)


def _run_qaoa(device: QDMIDevice) -> QAOAResult:
    """Evaluate, differentiate, update, and sample one QAOA layer.

    Returns:
        Sampled objective, gradient, sample, and execution statistics.
    """

    @qp.qnode(device, diff_method="parameter-shift")
    def cost(parameters: np.ndarray):
        _ansatz(parameters)
        return qp.expval(COST_HAMILTONIAN)

    @qp.qnode(device)
    def sample(parameters: np.ndarray):
        _ansatz(parameters)
        return qp.sample(wires=range(4))

    parameters = qp.numpy.array([0.5, 0.5], requires_grad=True)
    jobs_before = device.submitted_jobs
    started = time.monotonic()

    initial_cost = float(cost(parameters))
    gradient = qp.grad(cost)(parameters)
    initial_gradient_array = np.asarray(gradient, dtype=float)
    parameters -= 0.15 * gradient
    final_cost = float(cost(parameters))

    samples = np.asarray(sample(parameters), dtype=np.int8)
    observed = {"".join(str(int(bit)) for bit in row) for row in samples}
    best_bitstring = max(observed, key=lambda bitstring: (_cut_value(bitstring), bitstring))
    return QAOAResult(
        initial_cost=initial_cost,
        initial_gradient=(float(initial_gradient_array[0]), float(initial_gradient_array[1])),
        final_cost=final_cost,
        parameters=(float(parameters[0]), float(parameters[1])),
        best_bitstring=best_bitstring,
        best_cut=_cut_value(best_bitstring),
        jobs=device.submitted_jobs - jobs_before,
        elapsed=time.monotonic() - started,
    )


def _assert_valid_result(result: QAOAResult) -> None:
    """Validate stochastic invariants without assuming monotonic improvement."""
    assert np.isfinite(result.initial_cost)
    assert np.isfinite(result.initial_gradient).all()
    assert np.isfinite(result.final_cost)
    assert not np.allclose(result.parameters, (0.5, 0.5))
    assert len(result.best_bitstring) == 4
    assert set(result.best_bitstring) <= {"0", "1"}
    assert 0 <= result.best_cut <= 4
    # This fixed one-layer workflow currently needs 20 QDMI jobs for its
    # Hamiltonian evaluations and parameter-shift gradient. Keep the bound
    # explicit so a future increase cannot silently raise the SV1 cost.
    assert 3 < result.jobs <= QAOA_JOB_BUDGET
    assert result.elapsed >= 0.0


def test_qaoa_end_to_end_on_ddsim() -> None:
    """Run the sampled workflow on the local QDMI simulator."""
    device = qp.device("mqt.ddsim.default", wires=4, shots=200)
    assert isinstance(device, QDMIDevice)

    _assert_valid_result(_run_qaoa(device))


def test_qaoa_cost_capped_on_sv1() -> None:
    """Run one low-shot optimizer step only in the secret-enabled release lane."""
    required_environment = ("AWS_ACCESS_KEY_ID", "AWS_SECRET_ACCESS_KEY")
    live_lane = (
        os.environ.get("AMAZON_BRAKET_PENNYLANE_LIVE") == "1"
        and sys.version_info[:2] == (3, 14)
        and platform.system() == "Linux"
        and platform.machine() in {"AMD64", "x86_64"}
        and all(os.environ.get(name) for name in required_environment)
    )
    if not live_lane:
        pytest.skip("SV1 smoke test is restricted to CPython 3.14 manylinux x86-64 with AWS secrets.")

    device = qp.device(
        "amazon.braket.sv1",
        wires=4,
        shots=100,
    )
    assert isinstance(device, QDMIDevice)

    result = _run_qaoa(device)
    _assert_valid_result(result)
