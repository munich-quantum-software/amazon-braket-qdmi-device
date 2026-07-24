# Amazon Braket QDMI SPANK Plugin

[![SPANK Plugin GPLv3 License](https://img.shields.io/static/v1?logo=gnu&label=License&message=GPLv3&color=informational&style=flat-square)](https://www.gnu.org/licenses/gpl-3.0.en.html)

This plugin validates a user-selected Amazon Braket device once per remote job
step and passes the resulting session configuration to the job.

## Build and install

Compile against Slurm 20.02 or later, using the target cluster's headers and
matching SPANK ABI. The plugin requires C++20 and must be built together with
the Amazon Braket QDMI source tree.

```bash
cmake -S . -B build-spank -DBUILD_AMAZON_BRAKET_SPANK_PLUGIN=ON
cmake --build build-spank --target amazon-braket-qdmi-spank --parallel
sudo cmake --install build-spank --component amazon-braket-qdmi-spank-plugin
```

The build detects Slurm's `PluginDir` and `PlugStackConfig` through `scontrol`
when available. The Amazon Braket QDMI implementation is compiled directly into
the plugin, so deployment does not require a separate device shared library or
runtime loader configuration. Override distro-specific Slurm locations with
`AMAZON_BRAKET_QDMI_SPANK_INSTALL_DIR` and `AMAZON_BRAKET_QDMI_SLURM_CONF_DIR`.

Installation places `amazon-braket-qdmi-spank.so` in the configured plugin
directory and installs a disabled template in `plugstack.conf.d`. Deploy both on
the nodes running Slurm allocation commands and on compute nodes, then uncomment
the generated plugstack directive:

```text
required /configured/slurm/plugin/directory/amazon-braket-qdmi-spank.so
```

The plugin must be rebuilt when the target Slurm release changes.

## License

Only the SPANK plugin in this directory is licensed under GPL-3.0-or-later
because it links against Slurm's GPL-licensed interface. The core Amazon Braket
QDMI library remains licensed under Apache-2.0 with LLVM exceptions. The install
component places the plugin license at
`share/licenses/amazon-braket-qdmi-spank/LICENSE.md` and the core library
license at `share/licenses/amazon-braket-qdmi-device/LICENSE`. See
[LICENSE.md](LICENSE.md) for the plugin license text.

## Minimal job

```bash
#!/bin/bash
#SBATCH --qdmi-device-session-parameter-baseurl=arn:aws:braket:::device/quantum-simulator/amazon/sv1
#SBATCH --qdmi-device-session-parameter-authfile=/path/to/aws/credentials

exec ./submit_qdmi_job
```

The application only needs to initialize a session from the injected parameters,
set a program and S3 output bucket, and submit:

```cpp
#include <amazon-braket-qdmi-device/constants.hpp>
#include <amazon_braket_qdmi/device.h>
#include <cstring>

int main() {
    // Initialize the library
    AMAZON_BRAKET_QDMI_device_initialize();

    // Create a session (using SPANK injected parameters)
    AMAZON_BRAKET_QDMI_Device_Session session = nullptr;
    AMAZON_BRAKET_QDMI_device_session_alloc(&session);
    AMAZON_BRAKET_QDMI_device_session_init(session);

    // Create a quantum job
    AMAZON_BRAKET_QDMI_Device_Job job = nullptr;
    AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &job);

    // Configure and submit the OpenQASM program
    const char* program =
        "OPENQASM 3.0; include \"stdgates.inc\"; qubit[1] q;";
    const char* bucket = "my-amazon-braket-bucket";
    AMAZON_BRAKET_QDMI_device_job_set_parameter(
        job, QDMI_DEVICE_JOB_PARAMETER_PROGRAM, strlen(program) + 1, program);
    AMAZON_BRAKET_QDMI_device_job_set_parameter(
        job, QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3BUCKET, strlen(bucket) + 1,
        bucket);
    AMAZON_BRAKET_QDMI_device_job_submit(job);

    // Cleanup
    AMAZON_BRAKET_QDMI_device_job_free(job);
    AMAZON_BRAKET_QDMI_device_session_free(session);
    AMAZON_BRAKET_QDMI_device_finalize();

    return 0;
}
```

## Options and environment

The plugin is opt-in. The current adapter requires both the base URL and auth
file. Optional options are `--qdmi-device-session-parameter-region` and
`--qdmi-device-session-parameter-reservation-arn`.

The plugin sets the following QDMI session-parameters; applications may still
overwrite them:

- `QDMI_DEVICE_SESSION_PARAMETER_BASEURL`
- `QDMI_DEVICE_SESSION_PARAMETER_AUTHFILE`
- `QDMI_DEVICE_SESSION_PARAMETER_REGION`
- `QDMI_DEVICE_SESSION_PARAMETER_RESERVATION_ARN`

## Validation and failure behavior

1. In the `sbatch` allocator context, providing either the base URL or auth file
   opts into validation. The other missing option fails submission before
   scheduling; providing neither leaves the plugin inactive.
2. `user_init` creates a QDMI session after privileges are dropped, verifies the
   device status is `IDLE` or `BUSY`, and injects the variables above.
3. `task_init`, immediately before task execution, rejects the job if remote
   validation or environment injection failed. This reports a user/device
   failure as a job failure instead of a compute-node failure.
