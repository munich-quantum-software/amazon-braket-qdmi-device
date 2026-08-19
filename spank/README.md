# Optional Amazon Braket SPANK plugin

[![SPANK Plugin GPLv3 License](https://img.shields.io/static/v1?logo=gnu&label=License&message=GPLv3&color=informational&style=flat-square)](https://www.gnu.org/licenses/gpl-3.0.en.html)

This optional plugin injects AWS configuration references into a Slurm job
environment. It does not load a QDMI provider, resolve credentials, or make a
network request in `slurmstepd`.

License inspection only determines whether the plugin applies. The job process
uses MQT Core to validate `SLURM_JOB_LICENSES`, select a persistent QDMI device
definition, create an authenticated session, and check the device status. AWS
IAM remains the authorization boundary.

Configure Slurm licenses for concrete catalogue IDs such as `amazon.braket.sv1`.
Do not configure `amazon.braket.default` as a license; that generic device
intentionally requires runtime configuration. The plugin leaves jobs without a
concrete `amazon.braket.*` license unchanged. MQT Core enforces the
exact-one-device, local-license, and unit-count contract.

## Build and install

Build the plugin on Linux against Slurm 23.02 or newer. CI tests Slurm 23.11 on
Ubuntu 24.04. Use the same SPANK ABI as the target cluster.

```bash
cmake -S . -B build-spank \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_AMAZON_BRAKET_SPANK_PLUGIN=ON
cmake --build build-spank --target amazon-braket-qdmi-spank --parallel
sudo cmake --install build-spank --component amazon-braket-qdmi-spank-plugin
```

The default plugin directory is `${CMAKE_INSTALL_FULL_LIBDIR}/slurm`. The
default configuration directory is `/etc/slurm`. Set these CMake options when
the cluster uses other paths:

```bash
-DAMAZON_BRAKET_QDMI_SPANK_INSTALL_DIR=/usr/lib/x86_64-linux-gnu/slurm-wlm
-DAMAZON_BRAKET_QDMI_SLURM_CONF_DIR=/etc/slurm
```

Installation writes `amazon-braket-qdmi-spank.so` and a disabled
`plugstack.conf.d` template. Enable the plugin on submission hosts and compute
nodes:

```text
optional /usr/lib/x86_64-linux-gnu/slurm-wlm/amazon-braket-qdmi-spank.so
```

Use `required` if all nodes have the plugin and a configuration failure must
reject the job.

## Configuration

The precedence is SPANK option, submitted job environment, then plugstack
default.

| SPANK option                                     | Plugstack key                                | Job environment                    |
| ------------------------------------------------ | -------------------------------------------- | ---------------------------------- |
| `--amazon-braket-profile`                        | `amazon_braket_profile`                      | `AWS_PROFILE`                      |
| `--amazon-braket-config-file`                    | `amazon_braket_config_file`                  | `AWS_CONFIG_FILE`                  |
| `--amazon-braket-shared-credentials-file`        | `amazon_braket_shared_credentials_file`      | `AWS_SHARED_CREDENTIALS_FILE`      |
| `--amazon-braket-task-results-s3-uri`            | `amazon_braket_task_results_s3_uri`          | `AMZN_BRAKET_TASK_RESULTS_S3_URI`  |
| `--amazon-braket-reservation-arn`                | `amazon_braket_reservation_arn`              | `AMAZON_BRAKET_RESERVATION_ARN`    |

The plugin calls only the SPANK environment API for these values. It does not
change the `slurmd` environment. It does not accept an access key, a secret key,
or a session token as an option. It does not log configuration values.

An administrator can define defaults on the plugstack line. The plugin applies a
default only to a job that requests one local Braket license.

```text
optional /usr/lib/x86_64-linux-gnu/slurm-wlm/amazon-braket-qdmi-spank.so amazon_braket_profile=hpc-quantum amazon_braket_task_results_s3_uri=s3://site-braket-results/tasks
```

A plugstack default or profile reference must not grant access that the job user
does not already have. Protect referenced files with operating-system access
controls. Apply least-privilege IAM policies to profiles, workload identities,
and node roles. The plugin transports names and paths; it is not a credential
broker.

Configure only the concrete devices that the cluster should schedule. Counts are
cluster admission policy, not device metadata:

```ini
Licenses=amazon.braket.sv1:2,amazon.braket.iqm.garnet:1
```

## Use without SPANK

Use this mode when the node already has a suitable instance role or workload
identity. An AWS profile and `sbatch --export` also work. The QDMI provider uses
the AWS SDK default credential provider chain.

```bash
sbatch \
  --licenses=amazon.braket.sv1:1 \
  --export=ALL,AWS_PROFILE=hpc-quantum,AMZN_BRAKET_TASK_RESULTS_S3_URI=s3://site-braket-results/tasks \
  run-sv1.sh
```

The MQT Core Slurm adapter uses the local license environment value to select
`amazon.braket.sv1`. The persistent catalogue definition supplies the device ARN
and AWS Region. This selection does not prove the allocation or authorize AWS
access.

## Use with SPANK

Use SPANK when an administrator wants consistent names for credential sources,
the result destination, or a reservation. The plugin transports references. It
does not distribute credentials.

```bash
sbatch \
  --licenses=amazon.braket.sv1:1 \
  --amazon-braket-profile=hpc-quantum \
  --amazon-braket-task-results-s3-uri=s3://site-braket-results/tasks \
  run-sv1.sh
```

The AWS profile can use `credential_process`, web identity, container
credentials, or another source that the AWS SDK supports. Prefer temporary
credentials for cluster jobs.

At application startup, open the licensed device through MQT Core before doing
other work:

```python
from mqt.core.qdmi import slurm

device = slurm.open_device_from_license()
```

This performs the authenticated Amazon Braket `GetDevice` request and rejects a
device that is unavailable. It is an application-start preflight after Slurm has
launched the job, not scheduler-side authorization.

## Validation

The repository test uses Slurm 23.11 and Munge in an Ubuntu 24.04 container. It
checks option precedence, temporary credentials from `credential_process`, an
MQT Core device preflight against a minimal local `GetDevice` endpoint, and the
absence of configuration references in daemon environments. The container needs
`--privileged` only so Slurm can create its required cgroup-v2 scope; it does
not run systemd or mount the host cgroup namespace.

```bash
docker build -t amazon-braket-spank-tests -f spank/Dockerfile .
docker run --rm --privileged amazon-braket-spank-tests
```

## License

Only this plugin is licensed under GPL-3.0-or-later because it links against
Slurm's GPL-licensed interface. The Amazon Braket QDMI provider remains licensed
under Apache-2.0 with LLVM exceptions.
