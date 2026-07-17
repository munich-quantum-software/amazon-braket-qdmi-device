# Amazon Braket QDMI SPANK Plugin

This plugin validates a user-selected Amazon Braket device once per remote job step and
passes the resulting session configuration to the job.

## Build and install

Compile against the target cluster's Slurm headers and matching SPANK ABI. The
plugin requires C++20 and `slurm/spank.h`.

```bash
cmake -S . -B build-spank -DBUILD_AMAZON_BRAKET_SPANK_PLUGIN=ON
cmake --build build-spank --target amazon-braket-qdmi-spank --parallel
sudo cmake --install build-spank --component amazon-braket-qdmi-device_Runtime
```

Install `amazon-braket-qdmi-spank.so` on the nodes running Slurm allocation
commands and on compute nodes, then configure it in the SPANK plugstack:

```text
required /path/to/install/lib/slurm/amazon-braket-qdmi-spank.so
```

The plugin must be rebuilt when the target Slurm release changes.

## Minimal job

```bash
#!/bin/bash
#SBATCH --qdmi-device-session-parameter-baseurl=arn:aws:braket:::device/quantum-simulator/amazon/sv1
#SBATCH --qdmi-device-session-parameter-authfile=/path/to/aws/credentials

exec ./submit_qdmi_job
```

The application only needs to initialize a session from the injected
parameters, set a program and S3 output bucket, and submit:

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

    // Configure job parameters and submit
    AMAZON_BRAKET_QDMI_device_job_set_parameter(...);
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
file.
Optional options are `--qdmi-device-session-parameter-region` and
`--qdmi-device-session-parameter-reservation-arn`.

The plugin sets the following QDMI session-parameters; applications may still overwrite them:

- `QDMI_DEVICE_SESSION_PARAMETER_BASEURL`
- `QDMI_DEVICE_SESSION_PARAMETER_AUTHFILE`
- `QDMI_DEVICE_SESSION_PARAMETER_REGION`
- `QDMI_DEVICE_SESSION_PARAMETER_RESERVATION_ARN`

## Validation and failure behavior

1. In the `sbatch` allocator context, providing either the base URL or auth
   file opts into validation. The other missing option fails submission before
   scheduling; providing neither leaves the plugin inactive.
2. `user_init` creates a QDMI session after privileges are dropped, verifies
   the device status is `IDLE` or `BUSY`, and injects the variables above.
3. `task_init`, immediately before task execution, rejects the job if remote
   validation or environment injection failed. This reports a user/device
   failure as a job failure instead of a compute-node failure.
