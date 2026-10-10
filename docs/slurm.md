# Amazon Braket on Slurm

Use
[MQT Core's Slurm integration](https://mqt.readthedocs.io/projects/core/en/latest/qdmi/slurm.html)
for cluster setup, device licenses, job environments, and a reusable Docker
cluster. Slurm schedules the job; the Amazon Braket QDMI device implementation
handles AWS authentication and quantum tasks.

## Install and configure the device

Install the [Python package](installation.md) in the workload environment on
each compute node:

```console
uv pip install 'amazon-braket-qdmi-device[qiskit]'
```

The package includes the device library and catalogue. Locate the catalogue for
native commands such as `mqt-core-qdmi-check`:

```bash
export MQT_CORE_QDMI_CONFIG_FILE=$(python -c 'from amazon.braket.qdmi import AMAZON_BRAKET_QDMI_CATALOG_PATH; print(AMAZON_BRAKET_QDMI_CATALOG_PATH)')
```

For a native installation, select its installed catalogue instead. Keep the
Python package available for framework adapters. Catalogue and library paths
must be readable on every participating compute node.

Choose a [catalogue ID](device_catalog.md), such as `amazon.braket.sv1`, and
register it as a Slurm license. For example, `Licenses=amazon.braket.sv1:2`
allows two concurrent Slurm allocations. This limit is separate from AWS quotas
and device availability.

## AWS access

The device implementation uses the [AWS credential chain](configuration.md).
Prefer short-lived credentials through a role or profile. Set `AWS_PROFILE` in
the job environment when selecting a profile. Make its configuration and
credential sources available on compute nodes.

Use [result-storage configuration](configuration.md) for S3 permissions and
optional destination overrides. Keep credentials out of Slurm configuration. Set
profile names and configuration paths in the job environment; Slurm exports the
submission environment to the workload.

## Run a job

Save this Qiskit workload as `bell.py`:

```python
from mqt.core.qdmi import slurm
from qiskit import QuantumCircuit
from amazon.braket.qdmi.qiskit import AmazonBraketBackend

backend = AmazonBraketBackend(device=slurm.open_device_from_license())
circuit = QuantumCircuit(2)
circuit.h(0)
circuit.cx(0, 1)
circuit.measure_all()
print(backend.run(circuit, shots=100).result().get_counts())
```

After activating the workload environment and setting the catalogue path:

```bash
export AWS_PROFILE=research
srun --licenses=amazon.braket.sv1 python bell.py
```

For PennyLane, pass the selected device to MQT Core's adapter:

```python
from mqt.core.plugins.pennylane import QDMIDevice
from mqt.core.qdmi import slurm

device = QDMIDevice(device=slurm.open_device_from_license(), wires=2)
```

See the [PennyLane guide](pennylane.md) for circuits and result handling. An
optional availability probe can run inside the allocation after environment
setup; follow MQT Core's job-script example. AWS charges apply to submitted
quantum tasks independently of Slurm accounting.

## Test locally

The
[Slurm smoke test](https://github.com/munich-quantum-software/amazon-braket-qdmi-device/tree/main/test/slurm)
uses SV1 and MQT Core's Docker cluster. It checks native and wheel installations
through Qiskit and PennyLane, submitting two eight-shot tasks per mode. It
requires AWS credentials, and AWS charges apply. Follow its README to run it.
