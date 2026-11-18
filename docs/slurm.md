# Amazon Braket on Slurm

Run a Braket workload in your site's QDMI environment with AWS credentials
available to the job. Request the Slurm resource for the QDMI device that your
application opens.

## Submit a job

Save this Qiskit workload as `bell.py`. This example requests and opens the
fixed catalogue ID `amazon.braket.sv1`:

```{code-block} python
:caption: bell.py

from qiskit import QuantumCircuit

from amazon.braket.qdmi.qiskit import AmazonBraketBackend

backend = AmazonBraketBackend("amazon.braket.sv1")
circuit = QuantumCircuit(2)
circuit.h(0)
circuit.cx(0, 1)
circuit.measure_all()
print(backend.run(backend.compile(circuit), shots=100).result().get_counts())
```

Submit it to your site's quantum access partition, named `quantum` here:

```bash
srun --partition=quantum --licenses=amazon.braket.sv1 python bell.py
```

For requests with alternative devices, use the granted resource from Slurm's
allocation record to select the QDMI device. See
[device selection in MQSC Orchestrator](https://github.com/munich-quantum-software/orchestrator/blob/main/docs/deployment.md#from-allocation-to-device).

For PennyLane, pass the selected device to the adapter:

```python
from mqt.core.plugins.pennylane import QDMIDevice
from mqt.core.qdmi import builtin_driver as qdmi_driver

device = QDMIDevice(device=qdmi_driver.open_device("amazon.braket.sv1"), wires=2)
```

See the [PennyLane guide](pennylane.md) for circuits and result handling. AWS
quotas, permissions, and charges apply independently of Slurm allocations.

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

```bash
uv pip install 'amazon-braket-qdmi[qiskit,pennylane]'
```

The Python package advertises its installed device catalogue to MQT Core. Native
installations need a readable catalogue and library on each compute node; retain
the Python package for framework adapters. Administrators can put shared
non-secret settings in `/etc/mqt-core/qdmi.json`, which the builtin driver reads
automatically, and expose the workload environment through site defaults or a
software module.

Use the [catalogue ID](device_catalog.md) as the Slurm resource name.

## Deploy the shared cluster

Follow
[MQSC Orchestrator's deployment guide](https://github.com/munich-quantum-software/orchestrator/blob/main/docs/deployment.md)
for cluster installation and scheduler configuration.
