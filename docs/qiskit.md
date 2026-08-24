---
file_format: mystnb
kernelspec:
  name: python3
  display_name: Python 3
---

# Qiskit execution through QDMI

Install the package with the Qiskit adapter:

```console
uv pip install "amazon-braket-qdmi[qiskit]"
```

The adapter exposes the selected Amazon Braket device as a Qiskit `BackendV2`.
It serializes circuits to self-contained OpenQASM 3 and maps Qiskit operations
to the names accepted by Amazon Braket.

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

Running the following example submits one 10-shot QuantumTask to SV1. The AWS
SDK uses any credentials available through its default provider chain. Without
permission to access Amazon Braket, the example reports that live execution is
unavailable.

```{code-cell} python
:tags: [remove-stderr]

from qiskit import QuantumCircuit, transpile

from amazon.braket.qdmi.qiskit import AmazonBraketBackend

try:
    backend = AmazonBraketBackend("amazon.braket.sv1")
    circuit = QuantumCircuit(2)
    circuit.h(0)
    circuit.cx(0, 1)
    circuit.measure_all()

    counts = backend.run(transpile(circuit, backend), shots=10).result().get_counts()
    assert sum(counts.values()) == 10
    print(counts)
except RuntimeError as error:
    if "Permission denied" not in str(error):
        raise
    print("Live SV1 execution requires AWS credentials with Amazon Braket access.")
```

The device uses the standard regional result bucket when submitting the task.
See {doc}`configuration` for its name and the required Braket, STS, and S3
permissions.

## Use Qiskit primitives

MQT Core exposes the Qiskit `SamplerV2` and `EstimatorV2` interfaces directly
from the backend:

```python
from qiskit import QuantumCircuit
from qiskit.quantum_info import SparsePauliOp

sample_circuit = QuantumCircuit(2)
sample_circuit.h(0)
sample_circuit.cx(0, 1)
sample_circuit.measure_all()

sampler = backend.sampler(default_shots=100)
sampled = sampler.run([sample_circuit]).result()
counts = sampled[0].data.meas.get_counts()

estimator_circuit = QuantumCircuit(2)
estimator_circuit.h(0)
estimator_circuit.cx(0, 1)

estimator = backend.estimator(default_shots=100)
estimated = estimator.run([(estimator_circuit, SparsePauliOp("ZZ"))]).result()
expectation_value = estimated[0].data.evs
```

Both primitives submit Amazon Braket QuantumTasks. Choose shot counts
deliberately because each primitive run can submit multiple circuits.

## Override the result destination

Jobs use the standard regional result bucket automatically. Set
`AMZN_BRAKET_TASK_RESULTS_S3_URI` to an existing `s3://bucket/prefix` only when
a pre-provisioned destination is required.

## Use a Slurm-selected device

The Slurm adapter returns an already-open QDMI device. Pass it directly to the
Amazon Braket backend; no second device lookup is needed.

```python
from amazon.braket.qdmi.qiskit import AmazonBraketBackend
from mqt.core.qdmi import slurm

backend = AmazonBraketBackend(device=slurm.open_device_from_license())
```

See {doc}`slurm` for the complete cluster and job setup.
