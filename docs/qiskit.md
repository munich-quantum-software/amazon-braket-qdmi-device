---
file_format: mystnb
kernelspec:
  name: python3
  display_name: Python 3
---

# Qiskit execution through QDMI

Install the package with the Qiskit adapter and binary-only dependencies:

```console
uv pip install --only-binary=:all: "amazon-braket-qdmi[qiskit]"
```

The adapter exposes the selected Amazon Braket device as a Qiskit `BackendV2`.
It uses MQT Core to transpile to a format advertised by the QDMI device;
Amazon Braket devices use OpenQASM 3.

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

AWS credentials come from the AWS SDK default credential provider chain. Set
`AMZN_BRAKET_TASK_RESULTS_S3_URI` to an existing `s3://bucket/prefix` when the
standard Amazon Braket result bucket should not be used.

## Run a circuit on SV1

The following cell submits one 10-shot QuantumTask only on Read the Docs builds
that have both private AWS credential variables configured. Local and pull
request documentation builds skip the paid task.

```{code-cell} python
import os

live = os.environ.get("READTHEDOCS") == "True" and all(
    os.environ.get(name)
    for name in ("AWS_ACCESS_KEY_ID", "AWS_SECRET_ACCESS_KEY")
)

if live:
    from qiskit import QuantumCircuit, transpile

    from amazon.braket.qdmi.qiskit import AmazonBraketBackend

    circuit = QuantumCircuit(2)
    circuit.h(0)
    circuit.cx(0, 1)
    circuit.measure_all()

    backend = AmazonBraketBackend("amazon.braket.sv1")
    counts = backend.run(transpile(circuit, backend), shots=10).result().get_counts()
    assert sum(counts.values()) == 10
    counts
else:
    print("Live SV1 execution skipped; Read the Docs credentials are not configured.")
```

In **Admin → Environment variables** for the Read the Docs project, configure
these values as private:

| Name                    | Value                       |
| ----------------------- | --------------------------- |
| `AWS_ACCESS_KEY_ID`     | Amazon Braket access key ID |
| `AWS_SECRET_ACCESS_KEY` | Matching secret access key  |

Never mark the AWS values public. Private values are withheld from external
pull request builds. The device uses the standard regional result bucket when
`AMZN_BRAKET_TASK_RESULTS_S3_URI` is unset; see {doc}`configuration` for its
name and the required STS and S3 permissions. Every trusted documentation build
with both variables submits one paid SV1 QuantumTask.

## Use a Slurm-selected device

The Slurm adapter returns an already-open QDMI device. Pass it directly to MQT
Core's Qiskit backend; no second device lookup or provider-specific adapter is
needed.

```python
from mqt.core.plugins.qiskit.backend import QDMIBackend
from mqt.core.qdmi import slurm

backend = QDMIBackend(slurm.open_device_from_license())
```

See {doc}`slurm` for the complete AMI and job setup.
