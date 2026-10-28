# Qiskit execution through QDMI

Install the package with the Qiskit adapter:

```console
uv pip install "amazon-braket-qdmi[qiskit]"
```

This development branch requires MQT Core's unreleased serializer API. Use
`uv sync --extra qiskit` from this checkout to install its pinned Core revision.

The adapter exposes the selected Amazon Braket device as a Qiskit `BackendV2`.
Compile a circuit for that backend before submitting it. `run` serializes the
native circuit with MQT Core; it does not compile or reroute it. Device metadata
selects virtual-qubit OpenQASM for simulators or physical-qubit verbatim
execution for supported hardware. The native QDMI device adds the
[Braket submission wrapper](api.md#job-properties).

MQT Core retains a result layout with each program to reconstruct classical-bit
positions, including permuted measurement destinations and unused bits. Readout
must be terminal; adaptive and mid-circuit measurement are unsupported. A
circuit list uses one native ProgramSet when supported, otherwise separate tasks
in input order. [Low-level native results](api.md#job-results) retain Braket's
measured-qubit convention.

For Rigetti, operation metadata supplies the native RX angles ±π and ±π/2. MQT
Core uses these constraints for both Qiskit and direct QDMI compilation;
unsupported angles are rejected before submission.

## Select a catalogue device

Concrete IDs use the packaged device ARN and Region. Optional arguments override
those session defaults.

```python
from amazon.braket.qdmi.qiskit import AmazonBraketBackend

backend = AmazonBraketBackend("amazon.braket.sv1")
```

Use the generic entry for a device that is not in the catalogue:

```python
backend = AmazonBraketBackend(
    device_arn="arn:aws:braket:::device/quantum-simulator/amazon/sv1",
    region="us-east-1",
)
```

AWS credentials come from the AWS SDK default credential provider chain.

## Run a circuit on SV1

Running the following example submits one 10-shot QuantumTask to SV1. Configure
AWS credentials with Braket and S3 access before running it.

```python
from mqt.core.mlir import compile_program
from qiskit import QuantumCircuit

from amazon.braket.qdmi.qiskit import AmazonBraketBackend

backend = AmazonBraketBackend("amazon.braket.sv1")
circuit = QuantumCircuit(2)
circuit.h(0)
circuit.cx(0, 1)
circuit.measure_all()

native = compile_program(circuit, target=backend)
counts = backend.run(native, shots=10).result().get_counts()
print(counts)
```

The device uses the standard regional result bucket when submitting the task.
See {doc}`configuration` for its name and the required Braket, STS, and S3
permissions.

## Use Qiskit primitives

MQT Core supplies the sampler and estimator interfaces. Compile measured
circuits before passing them to `backend.sampler()`; submission does not perform
compilation. See
[MQT Core's Qiskit integration](https://mqt.readthedocs.io/projects/core/en/latest/qdmi/qdmi_backend.html#qiskit-primitives)
for primitive usage and result handling.

## Override the result destination

Jobs use the standard regional result bucket automatically. Set
`AMZN_BRAKET_TASK_RESULTS_S3_URI` to an existing `s3://bucket/prefix` only when
a pre-provisioned destination is required.

## Use an open QDMI device

Open a catalogue ID through the QDMI Client Interface and pass the device to the
Amazon Braket backend:

```python
from amazon.braket.qdmi.qiskit import AmazonBraketBackend
from mqt.core.qdmi import builtin_driver as qdmi_driver

backend = AmazonBraketBackend(device=qdmi_driver.open_device("amazon.braket.sv1"))
```

See
[MQT Core's Slurm playground](https://mqt.readthedocs.io/projects/core/en/latest/qdmi/slurm.html)
for cluster setup and job submission.
