# Amazon Braket on Slurm

[MQT Core's shared Slurm example](https://mqt.readthedocs.io/projects/core/en/latest/qdmi/slurm.html)
configures device licenses and availability monitoring. Cluster administrators
install Amazon Braket alongside the other QDMI device implementations in one
workload environment on login and quantum access nodes. Applications open a
catalogue ID through MQT Core's driver; the Amazon Braket implementation handles
AWS authentication and quantum tasks.

The shared `quantum` partition contains interchangeable quantum access nodes.
Each node can reach every configured device; the catalogue ID selects the device
independently of the node running the job.

## Configure AWS access

Install the [Python package](installation.md) with its Qiskit adapter:

```console
uv pip install 'amazon-braket-qdmi[qiskit]'
```

MQT Core discovers the installed device catalogue from the Python package. For
the unreleased QDMI 1.4 and MQT Core 4.1 interfaces, build the repositories'
current source revisions together as shown in the
[shared cluster example](https://mqt.readthedocs.io/projects/core/en/latest/qdmi/slurm_cluster.html).
Native installations need a readable catalogue and library on each compute node;
retain the Python package for framework adapters. Administrators can put shared
non-secret settings in `/etc/mqt-core/qdmi.json`, which MQT Core reads
automatically, and make the workload environment available through the site
defaults or a software module.

Choose a [catalogue ID](device_catalog.md), such as `amazon.braket.sv1`, and
register it as a Slurm license. `Licenses=amazon.braket.sv1:2` permits two
concurrent allocations. The cluster's availability monitor reserves the licenses
while this device is unavailable. Slurm license counts are separate from AWS
quotas and authorization.

The device implementation uses the [AWS credential chain](configuration.md).
Prefer short-lived credentials through a role or profile. Select your profile
with `AWS_PROFILE` in the submission environment when needed, and make its
configuration and credential sources available on compute nodes. Configure the
availability monitor with separate site-owned AWS credentials. Slurm exports the
submission environment; AWS credentials and other devices' credentials can
coexist in the same job environment.

See [result-storage configuration](configuration.md) for S3 permissions and
optional destination overrides. Keep credentials out of Slurm configuration and
committed scripts.

## Run a job

Save this Qiskit workload as `bell.py`:

```python
from mqt.core.qdmi import builtin_driver
from qiskit import QuantumCircuit
from amazon.braket.qdmi.qiskit import AmazonBraketBackend

backend = AmazonBraketBackend(device=builtin_driver.open_device("amazon.braket.sv1"))
circuit = QuantumCircuit(2)
circuit.h(0)
circuit.cx(0, 1)
circuit.measure_all()
print(backend.run(circuit, shots=100).result().get_counts())
```

With the site environment and your credentials available, submit the job to the
site's quantum partition (`quantum` in the shared cluster example). Request the
license matching the device ID opened by the application:

```console
srun --partition=quantum --licenses=amazon.braket.sv1 python bell.py
```

For PennyLane, pass the selected device to MQT Core's adapter:

```python
from mqt.core.plugins.pennylane import QDMIDevice
from mqt.core.qdmi import builtin_driver

device = QDMIDevice(device=builtin_driver.open_device("amazon.braket.sv1"), wires=2)
```

See the [PennyLane guide](pennylane.md) for circuits and result handling. AWS
charges apply to submitted quantum tasks independently of Slurm accounting.

## Exercise the integration

The
[Slurm smoke test](https://github.com/munich-quantum-software/amazon-braket-qdmi-device/tree/main/test/slurm)
runs Qiskit and PennyLane workloads on SV1 for both native and wheel
installations, submitting two eight-shot tasks per mode. It uses MQT Core's
shared cluster example, with real Slurm scheduling and AWS authentication. Its
README describes the credentials and commands; AWS charges apply.
