# Amazon Braket QDMI Device

Amazon Braket implementation of the
[Quantum Device Management Interface (QDMI)](https://github.com/Munich-Quantum-Software-Stack/QDMI)
specification.

## Overview

This library enables **any QDMI-compliant quantum software** to run on Amazon
Braket quantum devices without code changes. Simply link against this library
instead of another QDMI implementation, and your OpenQASM circuits will execute
on Amazon Braket simulators (and soon real quantum hardware).

### What is QDMI?

QDMI (Quantum Device Management Interface) is a standardized C API for quantum
devices, developed among others by [MQSC](https://mq.sc). It provides a
vendor-neutral interface for:

- Querying device properties (e.g., qubit count, connectivity, gate sets)
- Submitting quantum circuits (e.g., OpenQASM 2.0/3.0)
- Managing QDMI job lifecycle (create, submit, monitor, retrieve results)
- Accessing qubit and gate information (e.g., T1/T2 times, fidelities)

### What is Amazon Braket?

TODO

### Terminology

**Important:** This library uses QDMI terminology, which differs from AWS
Braket:

| QDMI Term | AWS Braket Equivalent | Description                                             |
| --------- | --------------------- | ------------------------------------------------------- |
| **Job**   | **QuantumTask**       | A single quantum circuit execution with specified shots |
| Device    | Device                | Quantum processor or simulator                          |
| Session   | BraketClient          | Connection to AWS Braket service                        |

**Not supported:** AWS Braket "Hybrid Jobs" (combined classical and quantum
workflows) are **not** supported by this library. This library only handles
QuantumTasks (pure quantum circuit execution).

### Supported Amazon Braket Devices

This implementation currently supports submitting jobs to simulator and
gate-based Amazon Braket devices. Additionally, support for querying properties
(e.g., qubit count, gate set) is implemented for:

| Device Type         | Examples                                                                   |
| ------------------- | -------------------------------------------------------------------------- |
| **Simulators**      | AWS SV1 (State Vector), AWS DM1 (Density Matrix), AWS TN1 (Tensor Network) |
| **Gate-based QPUs** | IQM Garnet, IQM Emerald                                                    |

## Quick Start

### Prerequisites

- **C++20** compatible compiler
- **CMake** 3.24 or later
- **AWS credentials** available to the AWS SDK (see Configuration below)
- **Slurm** 20.02 or later (only for optional SPANK plugin)

**Note**: Dependencies (AWS SDK for C++, QDMI) are automatically downloaded and
built by CMake during the configuration step. Further information on the SPANK
plugin is available in the [SPANK README](spank/README.md).

### Session Configuration

**AWS Credentials:**

Explicit QDMI credential parameters are optional. If none are set, the session
uses the AWS SDK default credential provider chain. Depending on the runtime,
the chain can load credentials from environment variables, shared AWS profile
files, web identity, container credentials, or an EC2 instance role. The same
refreshable provider is used for Amazon Braket requests and S3 result retrieval.

Alternatively, this library supports two methods for providing explicit
session-specific credentials:

**Method 1: Credentials File (Recommended for Multi-User Scenarios):**

Use the QDMI `AUTHFILE` parameter to specify a credentials file path:

```cpp
#include <amazon-braket-qdmi-device/constants.hpp>

AMAZON_BRAKET_QDMI_Device_Session session;
AMAZON_BRAKET_QDMI_device_session_alloc(&session);

// Set credentials file before initialization
const char* credsFile = "/path/to/credentials";
AMAZON_BRAKET_QDMI_device_session_set_parameter(
    session, QDMI_DEVICE_SESSION_PARAMETER_AUTHFILE,
    strlen(credsFile) + 1, credsFile);

// Configure device and initialize
const char* deviceArn = "arn:aws:braket:::device/quantum-simulator/amazon/sv1";
AMAZON_BRAKET_QDMI_device_session_set_parameter(
    session, AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN,
    strlen(deviceArn) + 1, deviceArn);

AMAZON_BRAKET_QDMI_device_session_init(session);
```

**Credentials File Format (Standard AWS INI Format):**

```ini
[default]
aws_access_key_id=AKIAIOSFODNN7EXAMPLE
aws_secret_access_key=wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY
# Optional for temporary credentials
aws_session_token=IQoJb3JpZ2luX2VjEOT//////////...
```

**Note:** The credentials file should contain only one profile section. The
parser reads the first profile found. This method allows different sessions to
use different credentials within the same process.

**Method 2: Direct Parameters:**

Use QDMI session parameters to specify credentials programmatically:

```cpp
#include <amazon-braket-qdmi-device/constants.hpp>

// Set credentials directly via QDMI parameters
const char* accessKey = "AKIAIOSFODNN7EXAMPLE";
AMAZON_BRAKET_QDMI_device_session_set_parameter(
    session, QDMI_DEVICE_SESSION_PARAMETER_USERNAME,
    strlen(accessKey) + 1, accessKey);

const char* secretKey = "wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY";
AMAZON_BRAKET_QDMI_device_session_set_parameter(
    session, QDMI_DEVICE_SESSION_PARAMETER_PASSWORD,
    strlen(secretKey) + 1, secretKey);

// Optional: session token for temporary credentials (STS, SSO)
const char* sessionToken = "IQoJb3JpZ2luX2VjEOT//////////...";
AMAZON_BRAKET_QDMI_device_session_set_parameter(
    session, QDMI_DEVICE_SESSION_PARAMETER_TOKEN,
    strlen(sessionToken) + 1, sessionToken);
```

The access key and secret key must be provided together. A session token is only
valid with a complete access/secret key pair. `AUTHFILE` takes precedence over
direct parameters when both are set.

**Available Credential Parameters:**

| Parameter                                | Type    | Required | Description                                   |
| ---------------------------------------- | ------- | -------- | --------------------------------------------- |
| `QDMI_DEVICE_SESSION_PARAMETER_AUTHFILE` | `char*` | No       | Path to AWS credentials file (INI format)     |
| `QDMI_DEVICE_SESSION_PARAMETER_USERNAME` | `char*` | No       | AWS Access Key ID                             |
| `QDMI_DEVICE_SESSION_PARAMETER_PASSWORD` | `char*` | No       | AWS Secret Access Key                         |
| `QDMI_DEVICE_SESSION_PARAMETER_TOKEN`    | `char*` | No       | AWS Session Token (for temporary credentials) |

**Device Configuration:**

Configure the device using QDMI session parameters:

```cpp
#include <amazon-braket-qdmi-device/constants.hpp>

// Configure session parameters before initialization
const char* deviceArn = "arn:aws:braket:eu-north-1::device/qpu/iqm/Garnet";
AMAZON_BRAKET_QDMI_device_session_set_parameter(
    session, AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN,
    strlen(deviceArn) + 1, deviceArn);
```

**Configuration Parameters:**

| Parameter                                                     | Type    | Required | Description                                                                                         |
| ------------------------------------------------------------- | ------- | -------- | --------------------------------------------------------------------------------------------------- |
| `AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN`       | `char*` | Yes      | Amazon Braket device ARN                                                                            |
| `AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_REGION`          | `char*` | No       | AWS region override (extracted from ARN by default)                                                 |
| `AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_RESERVATION_ARN` | `char*` | No       | Braket reservation ARN used for status reporting and inherited by jobs unless a job override is set |

**Note**: AWS authentication is handled via:

- The AWS SDK default credential provider chain when no explicit credential
  parameters are set
- `QDMI_DEVICE_SESSION_PARAMETER_AUTHFILE` for credentials files (see AWS
  Credentials section)
- `QDMI_DEVICE_SESSION_PARAMETER_USERNAME`,
  `QDMI_DEVICE_SESSION_PARAMETER_PASSWORD`,
  `QDMI_DEVICE_SESSION_PARAMETER_TOKEN` for direct credentials

### Job Configuration

Each QDMI job (which becomes an Amazon Braket QuantumTask) requires S3 storage
configuration for results. Configure using job-level parameters:

```cpp
#include <amazon-braket-qdmi-device/constants.hpp>

// Create and configure a job
AMAZON_BRAKET_QDMI_Device_Job job = nullptr;
AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &job);

// Set S3 bucket (required)
const char* s3Bucket = "my-braket-results";
AMAZON_BRAKET_QDMI_device_job_set_parameter(
    job, AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3BUCKET,
    strlen(s3Bucket) + 1, s3Bucket);

// Set S3 prefix (optional - auto-generates timestamp-based prefix if not set)
const char* s3Prefix = "my-experiment/run-42/";
AMAZON_BRAKET_QDMI_device_job_set_parameter(
    job, AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3PREFIX,
    strlen(s3Prefix) + 1, s3Prefix);
```

## Job Parameters

| Parameter                                                 | Type                  | Required | Description                            |
| --------------------------------------------------------- | --------------------- | -------- | -------------------------------------- |
| `QDMI_DEVICE_JOB_PARAMETER_PROGRAM`                       | `char*`               | Yes      | OpenQASM circuit source                |
| `QDMI_DEVICE_JOB_PARAMETER_PROGRAMFORMAT`                 | `QDMI_Program_Format` | No       | QASM2 or QASM3; default QASM3          |
| `QDMI_DEVICE_JOB_PARAMETER_SHOTSNUM`                      | `size_t`              | No       | Number of shots; defaults to 100       |
| `AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3BUCKET`  | `char*`               | Yes      | S3 bucket for quantum task results     |
| `AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3PREFIX`  | `char*`               | No       | S3 prefix for results                  |
| `AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_RESERVATION_ARN` | `char*`               | No       | Braket reservation ARN for time window |

### Installation

The library uses CMake for building and installation. The workflow mirrors
standard CMake practices:

**Step 1: Build the Library:**

```bash
# Clone the repository
git clone https://github.com/munich-quantum-software/amazon-braket-qdmi-device.git
cd amazon-braket-qdmi-device

# Configure
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --config Release
```

**Step 2: Install the Library:**

```bash
# Install to a prefix (e.g., ~/.local or /usr/local)
cmake --install build --prefix /path/to/install
```

**Step 3: Use in Your Project:**

In your application's `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.24)
project(MyQuantumApp)

# Find the installed library
find_package(amazon-braket-qdmi-device REQUIRED)

# Link against your executable
add_executable(my_app main.cpp)
target_link_libraries(my_app amazon-braket-qdmi-device)
```

Configure your project with:

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/install
cmake --build build
```

### Using the Device with MQT Core

The installed CMake target exports the stable device ID `amazon.braket.default`
and the `AMAZON_BRAKET` symbol prefix. An application using MQT Core 3.8 or
newer can copy the device library and a relocatable manifest beside its
executable:

```cmake
find_package(mqt-core 3.8 CONFIG REQUIRED)
find_package(amazon-braket-qdmi-device CONFIG REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE MQT::CoreFoMaC)
mqt_copy_qdmi_runtime(my_app amazon-braket-qdmi-device)
```

This placement is discovered automatically when the MQT Core Driver is linked
statically into the executable. A dynamically linked Driver searches beside its
own shared library instead; in that case, place the generated manifest there or
register the definition explicitly as shown below.

Python consumers can use the same metadata to preserve any existing
`amazon.braket.default` configuration and apply credentials or a device ARN to
each fresh session:

```python
from pathlib import Path

from amazon.braket.qdmi import (
    AMAZON_BRAKET_QDMI_DEVICE_ID,
    AMAZON_BRAKET_QDMI_LIBRARY_PATH,
    AMAZON_BRAKET_QDMI_PREFIX,
)
from mqt.core.fomac import DeviceDefinition, open_device, register_device_if_absent

register_device_if_absent(
    DeviceDefinition(
        AMAZON_BRAKET_QDMI_DEVICE_ID,
        AMAZON_BRAKET_QDMI_LIBRARY_PATH,
        AMAZON_BRAKET_QDMI_PREFIX,
    )
)
device = open_device(
    AMAZON_BRAKET_QDMI_DEVICE_ID,
    base_url="arn:aws:braket:::device/quantum-simulator/amazon/sv1",
    auth_file=Path("/path/to/credentials"),
    custom2="us-east-1",  # AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_REGION
)
```

`register_device_if_absent` does not replace definitions from `qdmi.json`,
`[tool.qdmi]`, or another higher-precedence configuration source. The explicit
arguments to `open_device` override only that newly opened session.

### CMake Options

| Option                                    | Default | Description                             |
| ----------------------------------------- | ------- | --------------------------------------- |
| `BUILD_AMAZON_BRAKET_TESTS`               | `ON`    | Build test suite (requires Google Test) |
| `BUILD_AMAZON_BRAKET_SPANK_PLUGIN`        | `OFF`   | Build the optional Slurm SPANK plugin   |
| `USE_INSTALLED_AMAZON_BRAKET_QDMI_DEVICE` | `OFF`   | Use installed library instead of build  |
| `CMAKE_PREFIX_PATH`                       | -       | Path to dependencies (AWS SDK, QDMI)    |

## Usage

### Example Program

```cpp
#include <amazon-braket-qdmi-device/constants.hpp>
#include <amazon_braket_qdmi/device.h>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

int main() {
    // Initialize the library
    AMAZON_BRAKET_QDMI_device_initialize();

    AMAZON_BRAKET_QDMI_Device_Session session = nullptr;
    AMAZON_BRAKET_QDMI_device_session_alloc(&session);

    // No credential parameters are needed when the AWS SDK default credential
    // provider chain is configured.

    // Configure device ARN (required)
    const char* deviceArn = "arn:aws:braket:::device/quantum-simulator/amazon/sv1";
    AMAZON_BRAKET_QDMI_device_session_set_parameter(
        session, AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN,
        strlen(deviceArn) + 1, deviceArn);

    // Initialize session (connects to Amazon Braket)
    AMAZON_BRAKET_QDMI_device_session_init(session);

    // Query device properties
    size_t qubits = 0;
    AMAZON_BRAKET_QDMI_device_session_query_device_property(
        session, QDMI_DEVICE_PROPERTY_QUBITSNUM, sizeof(qubits), &qubits,
        nullptr);
    std::cout << "Device has " << qubits << " qubits\n";

    // Create a quantum job
    AMAZON_BRAKET_QDMI_Device_Job job = nullptr;
    AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &job);

    // Configure S3 bucket for results (required)
    const char* s3Bucket = "my-amazon-braket-bucket";
    AMAZON_BRAKET_QDMI_device_job_set_parameter(
        job, AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3BUCKET,
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
    AMAZON_BRAKET_QDMI_device_job_wait(job, 60);  // 60s timeout

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
        std::stringstream keyStream(keys.data());
        std::string key;
        for (size_t i = 0; i < counts.size(); ++i) {
            std::getline(keyStream, key, ',');
            std::cout << (i == 0 ? "" : ", ") << '"' << key << "\": " << counts[i];
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

## API Reference

### Device Status

The library maps Amazon Braket device states to QDMI status values. The current
UTC time must be inside a window for the device to be reported as idle. If the
session has `AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_RESERVATION_ARN` set,
public execution windows are ignored because the session targets a reserved
window.

| Amazon Braket Status                   | QDMI Status                      | Description                                                                                                                        |
| -------------------------------------- | -------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------- |
| `ONLINE` (in window, queue < 5)        | `QDMI_DEVICE_STATUS_IDLE`        | Device is available and ready to accept quantum tasks.                                                                             |
| `ONLINE` (outside window or queue ≥ 5) | `QDMI_DEVICE_STATUS_BUSY`        | Device accepts queued work but is not currently idle.                                                                              |
| `ONLINE` (reservation set, queue < 5)  | `QDMI_DEVICE_STATUS_IDLE`        | Device is treated as available through the reservation rather than the public execution window.                                    |
| `OFFLINE`                              | `QDMI_DEVICE_STATUS_MAINTENANCE` | Device is temporarily unavailable. For example, it could be offline due to scheduled maintenance, upgrades, or operational issues. |
| `RETIRED`                              | `QDMI_DEVICE_STATUS_OFFLINE`     | Device is permanently decommissioned. Task submission is blocked.                                                                  |

Query the current status using:

```cpp
QDMI_Device_Status status;
AMAZON_BRAKET_QDMI_device_session_query_device_property(
    session, QDMI_DEVICE_PROPERTY_STATUS,
    sizeof(status), &status, nullptr);
```

### Supported QDMI Values

Other standard QDMI values return `QDMI_ERROR_NOTSUPPORTED`.

## Device Properties

| Property                              | Notes                         |
| ------------------------------------- | ----------------------------- |
| `QDMI_DEVICE_PROPERTY_NAME`           | Braket device name            |
| `QDMI_DEVICE_PROPERTY_VERSION`        | Library device version        |
| `QDMI_DEVICE_PROPERTY_STATUS`         | Current Braket device status  |
| `QDMI_DEVICE_PROPERTY_LIBRARYVERSION` | QDMI version                  |
| `QDMI_DEVICE_PROPERTY_QUBITSNUM`      | Number of qubits              |
| `QDMI_DEVICE_PROPERTY_QUEUELENGTH`    | Current queued task count     |
| `QDMI_DEVICE_PROPERTY_SITES`          | Qubit handles                 |
| `QDMI_DEVICE_PROPERTY_OPERATIONS`     | Gate handles                  |
| `QDMI_DEVICE_PROPERTY_COUPLINGMAP`    | Flat source/target site pairs |

## Site Properties

| Property                   | Notes                  |
| -------------------------- | ---------------------- |
| `QDMI_SITE_PROPERTY_INDEX` | Site id                |
| `QDMI_SITE_PROPERTY_NAME`  | Site name              |
| `QDMI_SITE_PROPERTY_T1`    | When Braket reports it |
| `QDMI_SITE_PROPERTY_T2`    | When Braket reports it |

## Operation Properties

| Property                                | Notes                                                         |
| --------------------------------------- | ------------------------------------------------------------- |
| `QDMI_OPERATION_PROPERTY_NAME`          | Native gate name for QPUs; supported gate name for simulators |
| `QDMI_OPERATION_PROPERTY_QUBITSNUM`     | Exact fixed gate arity                                        |
| `QDMI_OPERATION_PROPERTY_PARAMETERSNUM` | Number of scalar OpenQASM gate arguments                      |
| `QDMI_OPERATION_PROPERTY_SITES`         | Applicable physical site tuples                               |
| `QDMI_OPERATION_PROPERTY_FIDELITY`      | Site-dependent gate fidelity when Braket reports one          |

## Job Properties

| Property                                 | Notes                                |
| ---------------------------------------- | ------------------------------------ |
| `QDMI_DEVICE_JOB_PROPERTY_ID`            | AWS QuantumTask ARN after submission |
| `QDMI_DEVICE_JOB_PROPERTY_PROGRAMFORMAT` | Current program format               |
| `QDMI_DEVICE_JOB_PROPERTY_PROGRAM`       | Current program source               |
| `QDMI_DEVICE_JOB_PROPERTY_SHOTSNUM`      | Current shot count                   |
| `QDMI_DEVICE_JOB_PROPERTY_QUEUEPOSITION` | Jobs ahead while the task is queued  |

Querying `QDMI_DEVICE_JOB_PROPERTY_QUEUEPOSITION` performs a fresh
`GetQuantumTask` request with the `QueueInfo` additional attribute. The query
returns `QDMI_ERROR_BADSTATE` unless the refreshed task status is `QUEUED`, and
`QDMI_ERROR_NOTSUPPORTED` if AWS does not provide a trustworthy position.

An existing QuantumTask can be opened from its ARN. The opened handle exposes
the ARN and shot count, can be checked, waited for, canceled, and used to
retrieve results, but cannot be reconfigured or submitted again.

## Job Results

| Result                        | Notes                                   |
| ----------------------------- | --------------------------------------- |
| `QDMI_JOB_RESULT_SHOTS`       | Comma-separated shot bitstrings         |
| `QDMI_JOB_RESULT_HIST_KEYS`   | Comma-separated histogram keys          |
| `QDMI_JOB_RESULT_HIST_VALUES` | `size_t` counts matching histogram keys |

### Lifecycle Functions

| Function                                 | AWS SDK Counterpart  | Description                                   |
| ---------------------------------------- | -------------------- | --------------------------------------------- |
| `AMAZON_BRAKET_QDMI_device_initialize()` | `Aws::InitAPI()`     | Initialize the library (call once at startup) |
| `AMAZON_BRAKET_QDMI_device_finalize()`   | `Aws::ShutdownAPI()` | Cleanup resources (call once at shutdown)     |

### Session Management

| Function                                                       | AWS SDK Counterpart            | Description                              |
| -------------------------------------------------------------- | ------------------------------ | ---------------------------------------- |
| `AMAZON_BRAKET_QDMI_device_session_alloc()`                    | (internal allocation)          | Allocate a new session                   |
| `AMAZON_BRAKET_QDMI_device_session_set_parameter()`            | (store session config)         | Set credentials, device ARN, and region  |
| `AMAZON_BRAKET_QDMI_device_session_init()`                     | `BraketClient` + `GetDevice()` | Initialize session and connect to device |
| `AMAZON_BRAKET_QDMI_device_session_free()`                     | `BraketClient` destructor      | Free session resources                   |
| `AMAZON_BRAKET_QDMI_device_session_query_device_property()`    | (parse GetDevice JSON)         | Query device properties                  |
| `AMAZON_BRAKET_QDMI_device_session_query_site_property()`      | (parse GetDevice JSON)         | Query qubit properties                   |
| `AMAZON_BRAKET_QDMI_device_session_query_operation_property()` | (parse GetDevice JSON)         | Query gate properties                    |

### Job Management (QDMI Jobs → AWS QuantumTasks)

| Function                                                        | AWS SDK Counterpart       | Description                                 |
| --------------------------------------------------------------- | ------------------------- | ------------------------------------------- |
| `AMAZON_BRAKET_QDMI_device_session_create_device_job()`         | (internal allocation)     | Create a new QDMI job                       |
| `AMAZON_BRAKET_QDMI_device_session_retrieve_device_job_by_id()` | `GetQuantumTask()`        | Retrieve an existing task by ARN            |
| `AMAZON_BRAKET_QDMI_device_job_set_parameter()`                 | (store job config)        | Set job parameters (circuit, shots, format) |
| `AMAZON_BRAKET_QDMI_device_job_query_property()`                | (return stored values)    | Query job format, program, and shots        |
| `AMAZON_BRAKET_QDMI_device_job_submit()`                        | `CreateQuantumTask()`     | Submit QDMI job as AWS QuantumTask          |
| `AMAZON_BRAKET_QDMI_device_job_check()`                         | `GetQuantumTask()`        | Check quantum task status                   |
| `AMAZON_BRAKET_QDMI_device_job_wait()`                          | (poll `GetQuantumTask()`) | Wait for quantum task completion            |
| `AMAZON_BRAKET_QDMI_device_job_get_results()`                   | `S3Client::GetObject()`   | Retrieve measurement results from S3        |
| `AMAZON_BRAKET_QDMI_device_job_cancel()`                        | `CancelQuantumTask()`     | Cancel a running quantum task               |
| `AMAZON_BRAKET_QDMI_device_job_free()`                          | (internal cleanup)        | Free job resources                          |

## Testing

To run the test suite:

```bash
# Build with tests enabled (default)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Configure credentials and S3 storage for online/device tests
export AWS_S3_BUCKET="my-braket-results-bucket"

# Set credentials - choose one method:

# Method 1: Credentials file (recommended for local development)
export AWS_CREDENTIALS_FILE="/path/to/credentials"

# Method 2: Direct credentials (recommended for CI/CD with secrets)
export AWS_ACCESS_KEY_ID="your_access_key_id"
export AWS_SECRET_ACCESS_KEY="your_secret_access_key"
export AWS_SESSION_TOKEN="your_session_token"  # Optional

# Run tests
ctest --test-dir build --output-on-failure
```

**Test Configuration:**

- **Device ARNs**: Hardcoded directly in test code (e.g.,
  `arn:aws:braket:::device/quantum-simulator/amazon/sv1`)
  - Device ARNs are public identifiers and don't need to be secrets
  - Tests use various devices to verify functionality

- **Credentials**: Read from environment variables and passed to QDMI parameters
  - **Method 1:** `AWS_CREDENTIALS_FILE` → passed to
    `QDMI_DEVICE_SESSION_PARAMETER_AUTHFILE`
  - **Method 2:** `AWS_ACCESS_KEY_ID` + `AWS_SECRET_ACCESS_KEY` +
    `AWS_SESSION_TOKEN` (optional) → passed to respective QDMI parameters
  - Method 1 is recommended for local development
  - Method 2 is recommended for CI/CD pipelines where credentials are stored as
    secrets

- **S3 Bucket**: Read from `AWS_S3_BUCKET` environment variable
  - Passed to `AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3BUCKET` in job
    tests

**Note:** The offline tests require no AWS credentials. The online tests read
the variables above and pass explicit credentials through QDMI parameters. In
normal use, omitting those parameters delegates credential discovery and refresh
to the AWS SDK default provider chain. Session initialization also accepts the
service-specific environment fallbacks documented in the
[SPANK guide](spank/README.md) when the corresponding API parameters have not
been set explicitly.

## Project Structure

```text
amazon-braket-qdmi-device/
├── CMakeLists.txt                  # Build configuration
├── README.md                       # This file
├── include/
│   └── amazon-braket-qdmi-device/
│       └── device.hpp              # Public API header (QDMI implementation)
├── src/
│   └── device.cpp                  # Implementation (QDMI↔Amazon Braket)
└── test/
    └── test_device.cpp             # Integration tests
```

## Architecture

```text
┌─────────────────────────────────────────────────────────────────────┐
│                        Your Application                             │
│                    (QDMI-compliant code)                            │
└─────────────────────────────────────────────────────────────────────┘
                                  │
                                  ▼
┌─────────────────────────────────────────────────────────────────────┐
│                        Amazon Braket QDMI Device                    │
│  ┌─────────────────────────────────────────────────────────────┐    │
│  │ QDMI Functions              │ AWS SDK Calls                 │    │
│  ├─────────────────────────────┼───────────────────────────────┤    │
│  │ device_session_init()       │ BraketClient::GetDevice()     │    │
│  │ device_job_submit()         │ CreateQuantumTask()           │    │
│  │ device_job_check()          │ GetQuantumTask()              │    │
│  │ device_job_get_results()    │ S3Client::GetObject()         │    │
│  └─────────────────────────────┴───────────────────────────────┘    │
└─────────────────────────────────────────────────────────────────────┘
                                  │
                                  ▼
┌─────────────────────────────────────────────────────────────────────┐
│                          Amazon Braket                              │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐               │
│  │   AWS SV1    │  |   AWS DM1    │  |   AWS TN1    |   ...         │
│  └──────────────┘  └──────────────┘  └──────────────┘               │
└─────────────────────────────────────────────────────────────────────┘
```

## Support

For issues related to:

- **This library**: Open an issue on this repository
- **QDMI specification**: See
  [QDMI repository](https://github.com/Munich-Quantum-Software-Stack/QDMI)
- **Amazon Braket**: See
  [Amazon Braket documentation](https://docs.aws.amazon.com/braket/)

## License

The core Amazon Braket QDMI library is licensed under the Apache License 2.0
with LLVM exceptions; see [LICENSE](LICENSE). The optional Slurm SPANK plugin
under [spank/](spank/) is licensed separately under GPL-3.0-or-later; see
[spank/LICENSE.md](spank/LICENSE.md).
