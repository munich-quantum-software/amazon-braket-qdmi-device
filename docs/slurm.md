# Amazon Braket on Slurm

Use the
[MQT Core Slurm guide](https://mqt.readthedocs.io/projects/core/en/latest/qdmi/slurm.html)
for static licenses, the optional shared SPANK injector, scheduler operations,
and the common Dockerized test setup. This page covers the Braket runtime,
catalogue, credentials, and application adapters. The shared setup requires
Slurm 25.11 or newer. Deploy it from a released Core version before replacing an
existing provider plugin.

## Choose the provider runtime

For Python jobs, the wheel supplies the native provider and catalogue:

```console
uv venv /opt/braket
uv pip install --python /opt/braket/bin/python 'amazon-braket-qdmi[qiskit,pennylane]'
```

Locate its catalogue using that same environment:

```console
/opt/braket/bin/python -c 'from amazon.braket.qdmi import AMAZON_BRAKET_QDMI_CATALOG_PATH; print(AMAZON_BRAKET_QDMI_CATALOG_PATH)'
```

Use this path as `MQT_CORE_QDMI_CONFIG_FILE` or the shared injector's
`qdmi_config_file` default. The environment and referenced files must be
available at the same paths on compute nodes. A separate system provider is
unnecessary for this workflow.

For a site-managed native runtime, build and install the provider instead:

```console
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_AMAZON_BRAKET_TESTS=OFF
cmake --build build --target amazon-braket-qdmi-device --parallel 2
sudo cmake --install build --component amazon-braket-qdmi-device_Runtime
```

Select the installed `amazon-braket-qdmi-device.qdmi.json` beside the native
library, normally `/usr/local/lib/amazon-braket-qdmi-device.qdmi.json`. Python
adapters still come from the wheel; selecting this catalogue makes Core load the
system library named there. It does not load the wheel's provider as a second
runtime. Install the Development component only for C++ consumers.

## Catalogue and AWS access

Request a concrete catalogue ID, such as `amazon.braket.sv1`. The generic
`amazon.braket.default` entry requires runtime device selection and must not be
configured as a Slurm license. ARN and Region are fixed by the selected
catalogue entry.

AWS credentials remain the provider's responsibility. Prefer an instance role,
workload identity, or a profile using temporary credentials. The shared injector
may carry these non-secret references when an administrator allows them:

| Reference                         | Purpose                             |
| --------------------------------- | ----------------------------------- |
| `AWS_PROFILE`                     | Named AWS profile                   |
| `AWS_CONFIG_FILE`                 | AWS configuration file              |
| `AWS_SHARED_CREDENTIALS_FILE`     | Shared credentials file path        |
| `AMZN_BRAKET_TASK_RESULTS_S3_URI` | Pre-provisioned results destination |
| `AMAZON_BRAKET_RESERVATION_ARN`   | Reservation reference               |

For example, `reference=AWS_PROFILE:amazon.braket.sv1:hpc-quantum` supplies a
profile default for that concrete license. An allowed override is
`--qdmi-ref-AWS_PROFILE=research`. The standard regional results bucket remains
the default when no S3 destination is supplied. Referenced files must already be
readable by the job user. Never put raw credentials in plugstack arguments.

Follow Core's documented option, job-environment, and administrator-default
precedence. AWS IAM authorizes AWS operations; a Slurm license only accounts for
local concurrency.

## Run a Qiskit job

Save `bell.py` in the job's environment:

```python
from amazon.braket.qdmi.qiskit import AmazonBraketBackend
from mqt.core.qdmi import slurm
from qiskit import QuantumCircuit

backend = AmazonBraketBackend(device=slurm.open_device_from_license())
circuit = QuantumCircuit(2)
circuit.h(0)
circuit.cx(0, 1)
circuit.measure_all()
print(backend.run(circuit, shots=100).result().get_counts())
```

Without SPANK, export the selected catalogue and any AWS configuration
references before starting Python:

```console
export MQT_CORE_QDMI_CONFIG_FILE=/path/to/amazon-braket-qdmi-device.qdmi.json
srun --licenses=amazon.braket.sv1 /opt/braket/bin/python bell.py
```

With shared injection configured, the same job uses the administrator's defaults
or an allowed override:

```console
srun --licenses=amazon.braket.sv1 --qdmi-ref-AWS_PROFILE=research /opt/braket/bin/python bell.py
```

Core opens and checks the licensed device in the application process. The
injector itself makes no AWS requests.

## Run a PennyLane job

The same licensed handle works with Core's PennyLane adapter:

```python
import pennylane as qp
from mqt.core.plugins.pennylane import QDMIDevice
from mqt.core.qdmi import slurm

device = QDMIDevice(device=slurm.open_device_from_license(), wires=2)


@qp.qnode(device, shots=100)
def bell():
    qp.Hadamard(0)
    qp.CNOT(wires=[0, 1])
    return qp.counts(wires=[0, 1])


print(bell())
```

## Migrate the provider plugin

Build and validate Core's shared component first, then replace the old plugstack
directive. Remove `amazon-braket-qdmi-spank.so`; do not load both plugins. The
provider's `BUILD_AMAZON_BRAKET_SPANK_PLUGIN` option and plugin install
component have been removed.

| Old option                                     | Replacement                                      |
| ---------------------------------------------- | ------------------------------------------------ |
| `--amazon-braket-qdmi-config-file=PATH`        | `--qdmi-config-file=PATH`                        |
| `--amazon-braket-profile=NAME`                 | `--qdmi-ref-AWS_PROFILE=NAME`                    |
| `--amazon-braket-config-file=PATH`             | `--qdmi-ref-AWS_CONFIG_FILE=PATH`                |
| `--amazon-braket-shared-credentials-file=PATH` | `--qdmi-ref-AWS_SHARED_CREDENTIALS_FILE=PATH`    |
| `--amazon-braket-task-results-s3-uri=URI`      | `--qdmi-ref-AMZN_BRAKET_TASK_RESULTS_S3_URI=URI` |
| `--amazon-braket-reservation-arn=ARN`          | `--qdmi-ref-AMAZON_BRAKET_RESERVATION_ARN=ARN`   |

Translate old plugstack defaults into `qdmi_config_file` and corresponding
`reference` entries, and list the same concrete IDs under `licenses`. Preserve
the existing catalogue and AWS references during migration. Shared injection
does not add early device validation: application opening continues to
authenticate and check status. The optional generic Core launch checker is a
separate deployment choice.

## Validate the migration

The provider's local mock endpoint and probes are in `test/slurm`. Use Core's
common runner with this repository's Compose overlay and setup script; it owns
Slurm installation, daemon startup, admission, transport tests, and teardown.
See `test/slurm/README.md` for native and wheel test commands. These tests do
not contact AWS.
