# Amazon Braket QDMI SPANK plugin

[![SPANK Plugin GPLv3 License](https://img.shields.io/static/v1?logo=gnu&label=License&message=GPLv3&color=informational&style=flat-square)](https://www.gnu.org/licenses/gpl-3.0.en.html)

This plugin validates a selected Amazon Braket device once per remote Slurm job
step and exposes the resulting configuration to QDMI applications through the
job environment.

## Build and install

Build the plugin on Linux against Slurm 20.02 or newer and the same SPANK ABI as
the target cluster:

```bash
cmake -S . -B build-spank \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_AMAZON_BRAKET_SPANK_PLUGIN=ON
cmake --build build-spank --target amazon-braket-qdmi-spank --parallel
sudo cmake --install build-spank --component amazon-braket-qdmi-spank-plugin
```

The Amazon Braket QDMI implementation is compiled directly into the plugin.
Deployment therefore does not require a separate device shared library,
`ldconfig`, or a custom runtime library path.

The deterministic defaults are `${CMAKE_INSTALL_FULL_LIBDIR}/slurm` for the
plugin and `/etc/slurm` for configuration. Override them when configuring CMake
if the target distribution uses different locations:

```bash
-DAMAZON_BRAKET_QDMI_SPANK_INSTALL_DIR=/usr/lib/x86_64-linux-gnu/slurm-wlm
-DAMAZON_BRAKET_QDMI_SLURM_CONF_DIR=/etc/slurm
```

Installation writes `amazon-braket-qdmi-spank.so` and a disabled
`plugstack.conf.d` template. Enable the plugin on clients that run `srun` or
`sbatch` and on compute nodes:

```text
required /usr/lib/x86_64-linux-gnu/slurm-wlm/amazon-braket-qdmi-spank.so
```

Administrator defaults can be added to the same line:

```text
required /usr/lib/x86_64-linux-gnu/slurm-wlm/amazon-braket-qdmi-spank.so amazon_braket_region=us-east-1
```

## Job example

Selecting a device ARN opts the job into Amazon Braket validation. Explicit
credentials are optional; without a credentials file, the AWS SDK default
credential provider chain is used.

```bash
#!/bin/bash
#SBATCH --amazon-braket-device-arn=arn:aws:braket:::device/quantum-simulator/amazon/sv1
#SBATCH --amazon-braket-region=us-east-1

exec ./submit_bell_state
```

The injected environment lets the application initialize its QDMI session
without repeating the scheduler configuration. This complete example submits a
two-qubit Bell circuit:

```cpp
#include <amazon-braket-qdmi-device/constants.hpp>
#include <amazon_braket_qdmi/device.h>

#include <cstring>

int main() {
  AMAZON_BRAKET_QDMI_device_initialize();

  AMAZON_BRAKET_QDMI_Device_Session session = nullptr;
  AMAZON_BRAKET_QDMI_device_session_alloc(&session);
  AMAZON_BRAKET_QDMI_device_session_init(session);

  AMAZON_BRAKET_QDMI_Device_Job job = nullptr;
  AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &job);

  const char* program = R"(OPENQASM 3.0;
include "stdgates.inc";
bit[2] c;
qubit[2] q;
h q[0];
cx q[0], q[1];
c = measure q;
)";
  const char* bucket = "my-amazon-braket-results";

  AMAZON_BRAKET_QDMI_device_job_set_parameter(
      job, QDMI_DEVICE_JOB_PARAMETER_PROGRAM, std::strlen(program) + 1,
      program);
  AMAZON_BRAKET_QDMI_device_job_set_parameter(
      job, AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3BUCKET,
      std::strlen(bucket) + 1, bucket);
  AMAZON_BRAKET_QDMI_device_job_submit(job);

  AMAZON_BRAKET_QDMI_device_job_free(job);
  AMAZON_BRAKET_QDMI_device_session_free(session);
  AMAZON_BRAKET_QDMI_device_finalize();
}
```

## Configuration

Values use the following precedence: job option, submitted environment, then the
`plugstack.conf` administrator default.

| Job option                         | Plugstack key                    | Job environment                 | QDMI session parameter                                        |
| ---------------------------------- | -------------------------------- | ------------------------------- | ------------------------------------------------------------- |
| `--amazon-braket-device-arn`       | `amazon_braket_device_arn`       | `AMAZON_BRAKET_DEVICE_ARN`      | `AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN`       |
| `--amazon-braket-region`           | `amazon_braket_region`           | `AWS_REGION`                    | `AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_REGION`          |
| `--amazon-braket-reservation-arn`  | `amazon_braket_reservation_arn`  | `AMAZON_BRAKET_RESERVATION_ARN` | `AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_RESERVATION_ARN` |
| `--amazon-braket-credentials-file` | `amazon_braket_credentials_file` | `AWS_SHARED_CREDENTIALS_FILE`   | `QDMI_DEVICE_SESSION_PARAMETER_AUTHFILE`                      |

The device ARN is the only required value. Region defaults to the ARN region or
`us-east-1`. Reservation and credentials file are optional. Standard AWS
credential sources such as `AWS_ACCESS_KEY_ID`, web identity, container
credentials, profiles, and instance roles remain available through the SDK
provider chain. During validation, the plugin mirrors these settings from the
submitted job environment instead of inheriting the Slurm daemon environment.

## Validation and failure behavior

1. `user_init` injects the effective configuration after privileges are dropped.
   A job without a device ARN leaves the plugin inactive.
2. Active jobs initialize a QDMI session and require the selected device status
   to be `IDLE` or `BUSY`.
3. The validation result is cached once per remote job step. `task_init` rejects
   a failed job immediately before execution without reporting a compute-node
   failure to Slurm.

The repository exercises these paths against a real single-node Slurm setup in
Docker:

```bash
docker build -t amazon-braket-spank-tests -f spank/Dockerfile .
docker run --rm amazon-braket-spank-tests
```

## License

Only this SPANK plugin is licensed under GPL-3.0-or-later because it links
against Slurm's GPL-licensed interface. The Amazon Braket QDMI core remains
licensed under Apache-2.0 with LLVM exceptions. The install component includes
both license texts in separate directories.
