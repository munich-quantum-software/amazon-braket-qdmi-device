# Qiskit execution through QDMI

Install the package with the Qiskit adapter:

```console
uv pip install "amazon-braket-qdmi[qiskit]"
```

This development branch requires MQT Core's unreleased serializer API. Use
`uv sync --extra qiskit` from this checkout to install its pinned Core revision.

The adapter exposes the selected Amazon Braket device as a Qiskit `BackendV2`.
Follow MQT Core's
[compile-and-run guide](https://mqt.readthedocs.io/projects/core/en/latest/qdmi/qdmi_backend.html)
for native compilation and logical result reconstruction. The Braket device
adapts Core's output to virtual qubits and full measurement for simulators, and
adds a verbatim box for supported QPUs. Other clients' OpenQASM programs are
forwarded without automatic wrapping. Readout must be terminal.

A circuit list uses a native ProgramSet when supported, otherwise separate tasks
in input order. [Native results](api.md#job-results) retain measured-site order.

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
from qiskit import QuantumCircuit

from amazon.braket.qdmi.qiskit import AmazonBraketBackend

backend = AmazonBraketBackend("amazon.braket.sv1")
circuit = QuantumCircuit(2)
circuit.h(0)
circuit.cx(0, 1)
circuit.measure_all()

native = backend.compile(circuit)
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
