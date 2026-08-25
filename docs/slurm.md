# Slurm and SPANK deployment

This is the authoritative guide for deploying Amazon Braket QDMI on a Slurm
cluster and running workloads through a Slurm license. A centrally managed
deployment has three parts:

1. The native Runtime installs the provider library and its adjacent device
   catalogue on nodes that execute QDMI workloads.
2. The optional SPANK plugin passes the system catalogue path and selected AWS
   configuration references into licensed jobs.
3. The Python package installs the Qiskit and PennyLane adapters together with
   the corresponding MQT Core extras. No MQT Core source build is required.

The Python wheel also contains a native provider so that it works outside a
managed cluster. When `MQT_CORE_QDMI_CONFIG_FILE` selects the system catalogue,
MQT Core loads the system provider beside that catalogue; the wheel's provider
is not loaded.

## Install the native components

Build against the exact Slurm headers used by the cluster. The default paths
cover conventional Slurm installations:

```console
git clone https://github.com/munich-quantum-software/amazon-braket-qdmi-device.git
cd amazon-braket-qdmi-device
cmake -S . -B build-spank \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_AMAZON_BRAKET_TESTS=OFF \
  -DBUILD_AMAZON_BRAKET_SPANK_PLUGIN=ON
cmake --build build-spank \
  --target amazon-braket-qdmi-device amazon-braket-qdmi-spank \
  --parallel 2
sudo cmake --install build-spank \
  --component amazon-braket-qdmi-device_Runtime
sudo cmake --install build-spank \
  --component amazon-braket-qdmi-spank-plugin
```

For a Slurm distribution with version-specific paths, add
`-DSLURM_SPANK_INCLUDE_DIR=/path/to/include` and
`-DAMAZON_BRAKET_QDMI_SLURM_CONF_DIR=/path/to/slurm/config` to the configure
command. Set `AMAZON_BRAKET_QDMI_SPANK_INSTALL_DIR` as well when the cluster
loads plugins from a nonstandard directory.

Install the Runtime on every node where a Python process opens the QDMI device.
Install the SPANK component wherever the cluster loads its plugstack module,
which commonly includes submission and compute nodes. Installing both in a
shared cluster image is the simplest deployment when those node roles use the
same software stack.

The Runtime contains the shared provider library and device catalogue. The CMake
package configuration is a development artifact; install it only on nodes that
must compile consumers:

```console
sudo cmake --install build-spank \
  --component amazon-braket-qdmi-device_Development
```

Create the Python environment on the nodes or on a shared filesystem available
to jobs. Install the Braket extras for the application stacks that workloads
need:

```console
uv venv /opt/amazon-braket-qdmi
uv pip install --python /opt/amazon-braket-qdmi/bin/python \
  "amazon-braket-qdmi[qiskit,pennylane]"
```

Use only the `qiskit` or `pennylane` extra when workloads need one application
stack. These extras install the matching MQT Core extra and the Braket-specific
adapter. On supported platforms, installation needs no MQT Core checkout or
local C++ build.

## Enable the plugin

Installation creates a disabled `plugstack.conf.d/amazon-braket-qdmi.conf`. The
generated directive contains the system catalogue installed by the Runtime.
Enable it wherever the cluster loads this plugstack configuration:

```text
required /usr/local/lib/slurm/amazon-braket-qdmi-spank.so amazon_braket_qdmi_config_file=/usr/local/lib/amazon-braket-qdmi-device.qdmi.json
```

The actual plugin directory follows the target Slurm installation. Keep
`required` when all nodes contain the module and a configuration failure must
reject the workload.

Configure only concrete catalogue IDs as Slurm licenses. The generic
`amazon.braket.default` device requires runtime configuration and is therefore
not a schedulable device license.

```ini
Licenses=amazon.braket.sv1:2
```

Restart or reconfigure Slurm as required by the cluster after changing the
license and plugstack configuration.

## Configure AWS access

The plugin carries paths and names into the job; it never carries raw access
keys or grants AWS permissions. Prefer an instance role, workload identity, or
an AWS profile backed by temporary credentials. AWS IAM remains the
authorization boundary for Braket, STS, and S3.

Administrator defaults can be appended to the plugstack directive:

```text
amazon_braket_profile=hpc-quantum
```

Jobs can override each value with a SPANK option. Precedence is SPANK option,
submitted job environment, then plugstack default.

| SPANK option                                | Plugstack key                           | Job environment                   |
| ------------------------------------------- | --------------------------------------- | --------------------------------- |
| `--amazon-braket-qdmi-config-file`          | `amazon_braket_qdmi_config_file`        | `MQT_CORE_QDMI_CONFIG_FILE`       |
| `--amazon-braket-profile`                   | `amazon_braket_profile`                 | `AWS_PROFILE`                     |
| `--amazon-braket-config-file`               | `amazon_braket_config_file`             | `AWS_CONFIG_FILE`                 |
| `--amazon-braket-shared-credentials-file`   | `amazon_braket_shared_credentials_file` | `AWS_SHARED_CREDENTIALS_FILE`     |
| `--amazon-braket-task-results-s3-uri`       | `amazon_braket_task_results_s3_uri`     | `AMZN_BRAKET_TASK_RESULTS_S3_URI` |
| `--amazon-braket-reservation-arn`           | `amazon_braket_reservation_arn`         | `AMAZON_BRAKET_RESERVATION_ARN`   |

Protect referenced files with operating-system access controls. Apply
least-privilege policies to profiles and roles. The plugin does not accept or
log an access key, secret key, or session token.

The device uses the standard regional result bucket automatically. Set the S3
option, plugstack key, or environment variable only when a job must use a
pre-provisioned destination.

## Run a minimal Qiskit job

Save the following as `bell.py` on a shared filesystem available to the job:

```python
from amazon.braket.qdmi.qiskit import AmazonBraketBackend
from mqt.core.qdmi import slurm
from qiskit import QuantumCircuit

backend = AmazonBraketBackend(device=slurm.open_device_from_license())
circuit = QuantumCircuit(2)
circuit.h(0)
circuit.cx(0, 1)
circuit.measure_all()

result = backend.run(circuit, shots=100).result()
print(result.get_counts())
```

Save the batch script as `bell.sbatch`:

```bash
#!/bin/bash
#SBATCH --licenses=amazon.braket.sv1:1
#SBATCH --output=braket-%j.out

set -euo pipefail
/opt/amazon-braket-qdmi/bin/python /path/to/bell.py
```

With an instance or workload role, submit it directly:

```console
sbatch bell.sbatch
```

Otherwise pass references that are already accessible to the job user:

```console
sbatch \
  --amazon-braket-profile=hpc-quantum \
  bell.sbatch
```

MQT Core validates that exactly one concrete local QDMI device license was
requested, opens the catalogue entry, and performs the authenticated Amazon
Braket device check in the job process. The Slurm license controls local
admission; it neither proves allocation to AWS nor authorizes a QuantumTask.

## Run without SPANK

SPANK is convenient when administrators want to expose the system catalogue or
AWS configuration references only to matching licensed jobs. It is not required
by the provider or by MQT Core. A cluster environment module, container, or
batch script can expose the same catalogue instead:

```bash
export MQT_CORE_QDMI_CONFIG_FILE=/usr/local/lib/amazon-braket-qdmi-device.qdmi.json
```

AWS credentials remain available through the standard AWS credential provider
chain. The batch job still requests a concrete Slurm license and calls
`slurm.open_device_from_license()` exactly as shown above. Without SPANK, the
administrator or job environment is responsible for exporting the catalogue path
before the Python process first uses MQT Core.

## Run PennyLane instead

MQT Core lets PennyLane reuse the same Slurm-selected handle. Replace `bell.py`
with the following program and submit the same batch script:

```python
import pennylane as qp
from mqt.core.plugins.pennylane import QDMIDevice
from mqt.core.qdmi import slurm

device = QDMIDevice(
    device=slurm.open_device_from_license(),
    wires=2,
    shots=100,
)


@qp.qnode(device)
def bell():
    qp.Hadamard(0)
    qp.CNOT(wires=[0, 1])
    return qp.counts(wires=[0, 1])


print(bell())
```

The {doc}`pennylane` guide also documents direct catalogue-ID execution outside
Slurm.

## Validate a custom build

The repository's isolated test runs real Slurm and Munge services with a local
Amazon Braket endpoint. It does not contact AWS:

```console
docker build -t amazon-braket-spank-tests -f spank/Dockerfile .
docker run --rm --privileged amazon-braket-spank-tests
```

The test checks option precedence, temporary `credential_process` credentials,
MQT Core device opening through the system catalogue, and the absence of
configuration references in Slurm daemon environments. The plugin is
GPL-3.0-or-later because it links against Slurm; the provider remains Apache-2.0
WITH LLVM-exception.
