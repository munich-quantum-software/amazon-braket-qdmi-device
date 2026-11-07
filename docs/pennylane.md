# PennyLane integration

The Amazon Braket adapter configures MQT Core's QDMI device with the selected
device ARN, AWS Region, and optional reservation and result destination. MQT
Core compiles circuits for that device and returns measurements in the order
requested by the QNode.

## Install

```console
uv pip install "amazon-braket-qdmi[pennylane]"
```

## Configure a device

Every installed [catalogue ID](device_catalog.md) is also a PennyLane device
name. Concrete entries supply the device ARN and Region:

```python
import pennylane as qp

device = qp.device("amazon.braket.sv1", wires=2)
```

Use the generic entry point for a device not yet listed in the catalogue:

```python
device = qp.device(
    "amazon.braket.default",
    device_arn="arn:aws:braket:::device/quantum-simulator/amazon/sv1",
    region="us-east-1",
    wires=2,
)
```

Both forms accept explicit Region, reservation, device ARN, and S3 overrides.
AWS credentials come from the AWS SDK credential provider chain. Jobs use the
standard regional result bucket unless
`s3_destination_folder=("bucket", "prefix")` supplies a pre-provisioned
destination. See {doc}`configuration` for AWS authentication and S3 permissions.

## Run a QNode

This finite-shot QNode submits one 1000-shot SV1 task. AWS charges apply:

```python
@qp.qnode(device)
def circuit():
    qp.X(0)
    return qp.counts(wires=[1, 0])


print(qp.set_shots(circuit, shots=1000)())  # {"01": 1000}
```

## Supported scope

The integration supports gate-model devices with a compiler-supported native
basis and terminal readout. Analytic execution, pulse programs, and adaptive
mid-circuit measurement are unsupported.

See
[MQT Core's PennyLane guide](https://mqt.readthedocs.io/projects/core/en/latest/qdmi/pennylane_device.html)
for QNodes, gradients, batching, open QDMI handles, and the complete MaxCut QAOA
example. Shared cluster setup and job submission live in
[MQT Core's Slurm playground](https://mqt.readthedocs.io/projects/core/en/latest/qdmi/slurm.html).
