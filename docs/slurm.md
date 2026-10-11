# Amazon Braket on Slurm

Run a Braket workload in your site's QDMI environment with AWS credentials
available to the job. Request the Slurm license for the device you intend to
use, then open that device through the QDMI Client Interface.

## Submit a job

Save this Qiskit workload as `bell.py`. The Python example uses MQT Core's
builtin QDMI driver and the Amazon Braket adapter:

```python
import os

from mqt.core.qdmi import builtin_driver as qdmi_driver
from qiskit import QuantumCircuit

from amazon.braket.qdmi.qiskit import AmazonBraketBackend

device_id = os.environ["SLURM_JOB_LICENSES"].removesuffix(":1")
backend = AmazonBraketBackend(device=qdmi_driver.open_device(device_id))
circuit = QuantumCircuit(2)
circuit.h(0)
circuit.cx(0, 1)
circuit.measure_all()
print(backend.run(circuit, shots=100).result().get_counts())
```

Submit it to your site's quantum access partition, named `quantum` here:

```console
srun --partition=quantum --licenses=amazon.braket.sv1 python bell.py
```

This example requests one device license. Slurm exposes its ID, optionally with
the count `:1`, in `SLURM_JOB_LICENSES`. Applications using several devices
should receive their catalogue IDs explicitly.

For PennyLane, pass the selected device to the adapter:

```python
from mqt.core.plugins.pennylane import QDMIDevice
from mqt.core.qdmi import builtin_driver as qdmi_driver

device = QDMIDevice(device=qdmi_driver.open_device(device_id), wires=2)
```

See the [PennyLane guide](pennylane.md) for circuits and result handling. AWS
charges apply to submitted quantum tasks independently of Slurm accounting.

## Configure AWS access

The device implementation uses the [AWS credential chain](configuration.md).
Prefer short-lived credentials through a role or profile. Select your profile
with `AWS_PROFILE` in the submission environment when needed, and make its
configuration and credential sources available on compute nodes. Slurm exports
the submission environment; credentials for several device implementations can
coexist in the same job environment.

See [result-storage configuration](configuration.md) for S3 permissions and
optional destination overrides. Keep credentials out of Slurm configuration and
committed scripts.

## Configure the device catalogue

Administrators install the [Python package](installation.md) and adapters in the
workload environment available on login and quantum access nodes:

```console
uv pip install 'amazon-braket-qdmi[qiskit,pennylane]'
```

The Python package advertises its installed device catalogue to MQT Core. Native
installations need a readable catalogue and library on each compute node; retain
the Python package for framework adapters. Administrators can put shared
non-secret settings in `/etc/mqt-core/qdmi.json`, which the builtin driver reads
automatically, and expose the workload environment through site defaults or a
software module.

Choose a [catalogue ID](device_catalog.md), such as `amazon.braket.sv1`, and
register it as a Slurm license. `Licenses=amazon.braket.sv1:2` permits two
concurrent allocations. Slurm license counts are independent of AWS quotas and
authorization. An availability monitor can reserve these licenses while the
device is unavailable; configure it with site-owned AWS credentials separate
from user credentials.

## Try the shared cluster example

[MQT Core's Slurm cluster example](https://mqt.readthedocs.io/projects/core/en/latest/qdmi/slurm_cluster.html)
is a demonstrator with one `quantum` partition and one workload environment for
all QDMI device implementations. Its quantum access nodes can reach every
configured device, and its availability monitor updates Slurm reservations. The
guide describes installation from the current QDMI and MQT Core sources.

The
[Braket Slurm smoke test](https://github.com/munich-quantum-software/amazon-braket-qdmi-device/tree/main/test/slurm)
uses that cluster to run Qiskit and PennyLane workloads on SV1 for native and
wheel installations. It submits two eight-shot tasks per mode with real AWS
authentication. Its README gives the credentials and commands; AWS charges
apply.
