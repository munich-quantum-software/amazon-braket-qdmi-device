# QDMI SPANK Plugin

This plugin simplifies to use the Amazon Braket QDMI Device via Slurm.

## Build and install

An admin should compile against the target cluster's Slurm headers and matching SPANK ABI. The
plugin requires C++20 and `slurm/spank.h`.

```bash
cmake -S . -B build-spank -DBUILD_AMAZON_BRAKET_SPANK_PLUGIN=ON
cmake --build build-spank --target amazon-braket-qdmi-spank --parallel
sudo cmake --install build-spank --component amazon-braket-qdmi-device_Runtime
```

Install the resulting `amazon-braket-qdmi-spank.so` on the nodes that run the
Slurm allocation commands and on compute nodes. Configure it in the SPANK
plugstack:

```text
required /path/to/install/lib/slurm/amazon-braket-qdmi-spank.so
```

The plugin must be rebuilt when the target Slurm release changes.

## QDMI options

The plugin is opt-in. The current adapter requires the device ARN and AWS
credentials file:

```bash
sbatch \
  --qdmi-device-session-parameter-baseurl=arn:aws:braket:::device/quantum-simulator/amazon/sv1 \
  --qdmi-device-session-parameter-authfile=/path/to/aws/credentials \
  job.sh
```

Optional options are `--qdmi-device-session-parameter-region` and
`--qdmi-device-session-parameter-reservation-arn`.

The plugin injects these variables for the job:
 - `QDMI_DEVICE_SESSION_PARAMETER_BASEURL`
 - `QDMI_DEVICE_SESSION_PARAMETER_AUTHFILE`
 - `QDMI_DEVICE_SESSION_PARAMETER_REGION`
 - `QDMI_DEVICE_SESSION_PARAMETER_RESERVATION_ARN`

Applications may override these values through explicit QDMI session
parameters. AWS secret keys and session tokens are not injected.

## Usage

A user job script may use the plugin as follows:

```bash
#!/bin/bash
#SBATCH --job-name=amazon-braket-qdmi-device-job
#SBATCH --qdmi-device-session-parameter-baseurl=arn:aws:braket:::device/quantum-simulator/amazon/sv1
#SBATCH --qdmi-device-session-parameter-authfile=/path/to/aws/credentials

exec ./amazon_braket_qdmi_device_application
```

The application might look like this:

```cpp
#include <amazon_braket_qdmi/device.h>
#include <amazon-braket-qdmi-device/constants.hpp>
#include <cstring>
#include <iostream>
#include <vector>

int main() {
    // Initialize the library
    AMAZON_BRAKET_QDMI_device_initialize();

    // Create session
    AMAZON_BRAKET_QDMI_Device_Session session;
    AMAZON_BRAKET_QDMI_device_session_alloc(&session);

    // Configure credentials (required)
    const char* credsFile = "/path/to/credentials";
    AMAZON_BRAKET_QDMI_device_session_set_parameter(
        session, QDMI_DEVICE_SESSION_PARAMETER_AUTHFILE,
        strlen(credsFile) + 1, credsFile);

    // Configure device ARN (required)
    const char* deviceArn = "arn:aws:braket:::device/quantum-simulator/amazon/sv1";
    AMAZON_BRAKET_QDMI_device_session_set_parameter(
        session, QDMI_DEVICE_SESSION_PARAMETER_BASEURL,
        strlen(deviceArn) + 1, deviceArn);

    // Initialize session (connects to Amazon Braket)
    AMAZON_BRAKET_QDMI_device_session_init(session);

    // Query device properties
    size_t qubits;
    AMAZON_BRAKET_QDMI_device_session_query_device_property(
        session, QDMI_DEVICE_PROPERTY_QUBITSNUM,
        sizeof(qubits), &qubits, nullptr);
    std::cout << "Device has " << qubits << " qubits\n";

    // Create a quantum job
    AMAZON_BRAKET_QDMI_Device_Job job;
    AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &job);

    // Configure S3 bucket for results (required)
    const char* s3Bucket = "my-amazon-braket-bucket";
    AMAZON_BRAKET_QDMI_device_job_set_parameter(
        job, QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3BUCKET,
        strlen(s3Bucket) + 1, s3Bucket);

    // Configure job parameters
    size_t shots = 1000;
    AMAZON_BRAKET_QDMI_device_job_set_parameter(
        job, QDMI_DEVICE_JOB_PARAMETER_SHOTSNUM,
        sizeof(shots), &shots);

    QDMI_Program_Format format = QDMI_PROGRAM_FORMAT_QASM3;
    AMAZON_BRAKET_QDMI_device_job_set_parameter(
        job, QDMI_DEVICE_JOB_PARAMETER_PROGRAMFORMAT,
        sizeof(format), &format);

    // Submit a Bell state circuit
    const char* circuit = R"(OPENQASM 3.0;
        qubit[2] q;
        bit[2] c;
        h q[0];
        cnot q[0], q[1];
        c[0] = measure q[0];
        c[1] = measure q[1];
    )";
    AMAZON_BRAKET_QDMI_device_job_set_parameter(
        job, QDMI_DEVICE_JOB_PARAMETER_PROGRAM,
        strlen(circuit) + 1, circuit);

    AMAZON_BRAKET_QDMI_device_job_submit(job);
    AMAZON_BRAKET_QDMI_device_job_wait(job, 60000);  // 60s timeout

    // Check results
    QDMI_Job_Status status;
    AMAZON_BRAKET_QDMI_device_job_check(job, &status);
    if (status == QDMI_JOB_STATUS_DONE) {
        std::cout << "Job completed successfully\n";

        size_t keysSize = 0;
        size_t valuesSize = 0;
        AMAZON_BRAKET_QDMI_device_job_get_results(
            job, QDMI_JOB_RESULT_HIST_KEYS, 0, nullptr, &keysSize);
        AMAZON_BRAKET_QDMI_device_job_get_results(
            job, QDMI_JOB_RESULT_HIST_VALUES, 0, nullptr, &valuesSize);

        std::vector<char> keys(keysSize);
        std::vector<size_t> counts(valuesSize / sizeof(size_t));
        AMAZON_BRAKET_QDMI_device_job_get_results(
            job, QDMI_JOB_RESULT_HIST_KEYS, keysSize, keys.data(), nullptr);
        AMAZON_BRAKET_QDMI_device_job_get_results(
            job, QDMI_JOB_RESULT_HIST_VALUES, valuesSize, counts.data(), nullptr);

        std::cout << "Shot counts: {";
        const char* key = keys.data();
        for (size_t i = 0; i < counts.size(); ++i) {
            std::cout << (i == 0 ? "" : ", ") << '"' << key << "\": " << counts[i];
            key += std::strlen(key) + 1;
        }
        std::cout << "}\n";
    }

    // Cleanup
    AMAZON_BRAKET_QDMI_device_job_free(job);
    AMAZON_BRAKET_QDMI_device_session_free(session);
    AMAZON_BRAKET_QDMI_device_finalize();

    return 0;
}
```

## Validation and failure behavior

1. The `--qdmi-device-session-parameter-baseurl` opts into validation. When
   opted in, a missing `--qdmi-device-session-parameter-authfile` marks the
   job as invalid.
2. `user_init` creates a QDMI session after privileges are dropped, verifies
   the device status is `IDLE` or `BUSY`, and injects the variables above.
3. `task_init`, immediately before task execution, rejects the job if session
   validation or environment injection failed. This way a user/device failure is reported as a
   job failure instead of being treated by Slurm as a compute-node failure.
