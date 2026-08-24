---
file_format: mystnb
kernelspec:
  name: python3
  display_name: Python 3
---

# PennyLane execution through QDMI

The optional PennyLane integration specializes the gate-based QDMI device from
MQT Core for Amazon Braket. PennyLane programs are preprocessed by PennyLane,
converted to OpenQASM 3 according to the native operations advertised by the
selected Amazon Braket device, submitted as QDMI jobs, and reconstructed as
PennyLane results. The integration uses the packaged native device directly; it
does not depend on the Amazon Braket Python SDK or the direct PennyLane-Braket
plugin.

```console
uv pip install --only-binary=:all: "amazon-braket-qdmi[pennylane]"
```

## Local execution of the QAOA program

The following MaxCut calculation uses MQT Core's DD-based QDMI simulator. The
same circuit construction is accepted by the Amazon Braket specialization below,
which changes only the device configuration.

```{code-cell} python
import time

import networkx as nx
import numpy as np
import pennylane as qp

graph = nx.Graph([(0, 1), (0, 2), (1, 2), (2, 3)])
cost_hamiltonian, mixer_hamiltonian = qp.qaoa.maxcut(graph)

device = qp.device("mqt.ddsim.default", wires=4, shots=200)
```

One QAOA layer is evaluated and differentiated using the parameter-shift rule.
Each shifted circuit is submitted as a separate QDMI job; job submission is
sequential.

```{code-cell} python
def ansatz(parameters):
    for wire in graph.nodes:
        qp.Hadamard(wire)
    qp.qaoa.cost_layer(parameters[0], cost_hamiltonian)
    qp.qaoa.mixer_layer(parameters[1], mixer_hamiltonian)


@qp.qnode(device, diff_method="parameter-shift")
def cost(parameters):
    ansatz(parameters)
    return qp.expval(cost_hamiltonian)


@qp.qnode(device)
def sample(parameters):
    ansatz(parameters)
    return qp.sample(wires=range(4))
```

The cost estimates are stochastic. The test below checks only finite results and
a well-defined parameter update; it does not assume that one noisy update must
improve the sampled objective.

```{code-cell} python
parameters = qp.numpy.array([0.5, 0.5], requires_grad=True)
jobs_before = device.submitted_jobs
started = time.monotonic()

initial_cost = float(cost(parameters))
gradient = qp.grad(cost)(parameters)
parameters -= 0.15 * gradient
final_cost = float(cost(parameters))
samples = np.asarray(sample(parameters), dtype=np.int8)

elapsed = time.monotonic() - started
submitted_jobs = device.submitted_jobs - jobs_before

assert np.isfinite(initial_cost)
assert np.isfinite(gradient).all()
assert np.isfinite(final_cost)
assert submitted_jobs > 3

initial_cost, np.asarray(gradient), final_cost, submitted_jobs, elapsed
```

The sampled bit strings determine candidate graph partitions. The maximum cut
observed in the sample is extracted directly from the measurement records.

```{code-cell} python
def cut_value(bitstring):
    return sum(bitstring[first] != bitstring[second] for first, second in graph.edges)


observed = {"".join(str(int(bit)) for bit in row) for row in samples}
best_bitstring = max(observed, key=lambda bitstring: (cut_value(bitstring), bitstring))
best_bitstring, cut_value(best_bitstring)
```

## Amazon Braket device configuration

Every installed catalogue ID is also a PennyLane device name. Concrete entries
such as `amazon.braket.sv1` use the catalogue's ARN and Region, while still
allowing explicit Region, reservation, device ARN, and S3 overrides.

```python
remote_device = qp.device(
    "amazon.braket.sv1",
    wires=4,
    shots=1_000,
    s3_destination_folder=("my-results-bucket", "experiments/maxcut"),
)
```

`amazon.braket.default` remains the generic entry point for arbitrary or newly
introduced Amazon Braket devices. It requires a device ARN. AWS credentials are
resolved by the AWS SDK default credential provider chain.

```python
other_device = qp.device(
    "amazon.braket.default",
    device_arn="arn:aws:braket:::device/quantum-simulator/amazon/sv1",
    wires=4,
    shots=1_000,
    s3_destination_folder=("my-results-bucket", "experiments/maxcut"),
    region="us-east-1",
)
```

The S3 tuple is converted to the complete QDMI job URI
`s3://my-results-bucket/experiments/maxcut`. If it is omitted, the native device
uses `AMZN_BRAKET_TASK_RESULTS_S3_URI` and then the standard regional Amazon
Braket bucket. See {doc}`configuration` for the complete authentication and S3
resolution contract.

The concrete PennyLane names are generated from release-time package metadata.
New catalogue entries become available as PennyLane names with the release that
contains them; the generic entry point can address them immediately by ARN.

Substituting `remote_device` for `device` in the QNodes above executes the same
program on SV1. Remote execution creates paid Amazon Braket QuantumTasks. Shot
counts, parameter-shift evaluations, and optimizer iterations should therefore
be selected explicitly before execution.

The PennyLane adapter currently opens a device by catalogue ID. MQT Core 3.9
does not yet let a PennyLane device reuse the QDMI handle selected from a Slurm
license. Use the {doc}`slurm` Qiskit workflow for license-selected jobs; direct
PennyLane jobs remain supported through the catalogue names shown above.

## Execution boundary

MQT Core prefers OpenQASM 3 whenever the QDMI device advertises it. A failed
OpenQASM 3 conversion is reported directly and is not retried through OpenQASM
2. The device's native gate set is authoritative: PennyLane decomposes only to
native operations that MQT Core can express directly.

MQT Core does not yet provide general native-gate synthesis and routing for
Braket targets such as IQM's `prx` or IonQ's `gpi` and `gpi2`. Those QPUs are
therefore not supported by this integration until native target compilation is
available in MQT Core; Amazon Braket's broader OpenQASM operation set is not
substituted for the native device view. Analytic execution, pulse programs,
non-gate-model devices, circuit routing, and parallel job submission are also
not supported.
