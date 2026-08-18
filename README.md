# Amazon Braket QDMI Device

Amazon Braket implementation of the
[Quantum Device Management Interface (QDMI)](https://github.com/Munich-Quantum-Software-Stack/QDMI)
specification.

## Overview

This library enables **any QDMI-compliant quantum software** to run on Amazon
Braket quantum devices without code changes. Simply link against this library
instead of another QDMI implementation, and your OpenQASM circuits will execute
on supported Amazon Braket gate-model simulators and QPUs.

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

This implementation supports Amazon Braket gate-model QPUs and on-demand
gate-model simulators. The generic QDMI ID `amazon.braket.default` must be
configured with a device ARN and, when needed, an AWS Region before
initialization. The installed catalogue also provides these preconfigured IDs:

| Stable QDMI ID                                  | Region       |
| ----------------------------------------------- | ------------ |
| `amazon.braket.aqt.ibex-q1`                     | `eu-north-1` |
| `amazon.braket.ionq.forte-1`                    | `us-east-1`  |
| `amazon.braket.ionq.forte-enterprise-1`         | `us-east-1`  |
| `amazon.braket.iqm.garnet`                      | `eu-north-1` |
| `amazon.braket.iqm.emerald`                     | `eu-north-1` |
| `amazon.braket.rigetti.ankaa-3`                 | `us-west-1`  |
| `amazon.braket.rigetti.cepheus-1-108q`          | `us-west-1`  |
| `amazon.braket.sv1`                             | `us-east-1`  |
| `amazon.braket.dm1`                             | `us-east-1`  |

For a QPU, `QDMI_DEVICE_PROPERTY_OPERATIONS` contains only the Braket
`nativeGateSet`. The custom property
`AMAZON_BRAKET_QDMI_DEVICE_PROPERTY_SUPPORTEDOPERATIONS` contains the broader
OpenQASM `supportedOperations` set. Equal names in both sets have the same QDMI
operation handle. SV1 and DM1 do not publish a hardware-native gate set. For
these simulators, the standard property contains their executable OpenQASM
operation set.

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

The provider uses the
[AWS SDK for C++ default credential provider chain](https://docs.aws.amazon.com/sdk-for-cpp/v1/developer-guide/credproviders.html)
for Amazon Braket, S3, and STS. The chain supports environment credentials,
shared AWS profiles, `credential_process`, web identity, container credentials,
and instance roles. It also refreshes temporary credentials. Do not store access
keys or session tokens in a QDMI device definition.

The generic QDMI `AUTHFILE`, `USERNAME`, `PASSWORD`, and `TOKEN` session
parameters return `QDMI_ERROR_NOTSUPPORTED`. Select a profile or another
credential source before you start the process. For example:

```bash
export AWS_PROFILE=hpc-quantum
```

Configure the device using QDMI session parameters:

```cpp
#include <amazon-braket-qdmi-device/constants.hpp>

// Configure session parameters before initialization
const char* deviceArn = "arn:aws:braket:eu-north-1::device/qpu/iqm/Garnet";
AMAZON_BRAKET_QDMI_device_session_set_parameter(
    session, AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN,
    strlen(deviceArn) + 1, deviceArn);
```

| Parameter                                                     | Type    | Required | Description                                                                                         |
| ------------------------------------------------------------- | ------- | -------- | --------------------------------------------------------------------------------------------------- |
| `AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN`       | `char*` | Yes      | Amazon Braket device ARN                                                                            |
| `AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_REGION`          | `char*` | No       | AWS region override (extracted from ARN by default)                                                 |
| `AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_RESERVATION_ARN` | `char*` | No       | Braket reservation ARN used for status reporting and inherited by jobs unless a job override is set |

### Job Configuration

Amazon Braket requires an S3 destination for every quantum task. The provider
resolves the destination in this order:

1. `AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3URI` on the job.
2. `AMZN_BRAKET_TASK_RESULTS_S3_URI` in the process environment.
3. The standard bucket `amazon-braket-<region>-<account-id>` and prefix `tasks`.

The first two forms must contain a complete URI such as
`s3://my-results/experiments/run-42`. They do not call STS or any S3 bucket
management API. The automatic form resolves the account with STS. It creates the
standard bucket when needed and blocks public access. This work starts only when
the first job is submitted. Opening a device and querying properties do not
require STS or S3 permissions.

```cpp
#include <amazon-braket-qdmi-device/constants.hpp>

// Create and configure a job
AMAZON_BRAKET_QDMI_Device_Job job = nullptr;
AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &job);

// This is optional when the environment or automatic default is suitable.
const char* s3Uri = "s3://my-braket-results/experiments/run-42";
AMAZON_BRAKET_QDMI_device_job_set_parameter(
    job, AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3URI,
    strlen(s3Uri) + 1, s3Uri);
```

For a restricted HPC role, provision the bucket in advance and set the job URI
or `AMZN_BRAKET_TASK_RESULTS_S3_URI`. This path needs object access but does not
need STS, `CreateBucket`, or `PutPublicAccessBlock`.

## Job Parameters

| Parameter                                                 | Type                  | Required | Description                                       |
| --------------------------------------------------------- | --------------------- | -------- | ------------------------------------------------- |
| `QDMI_DEVICE_JOB_PARAMETER_PROGRAM`                       | `char*`               | Yes      | OpenQASM circuit source                           |
| `QDMI_DEVICE_JOB_PARAMETER_PROGRAMFORMAT`                 | `QDMI_Program_Format` | No       | QASM2 or QASM3; default QASM3                     |
| `QDMI_DEVICE_JOB_PARAMETER_SHOTSNUM`                      | `size_t`              | No       | Number of shots; defaults to 100                  |
| `AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3URI`     | `char*`               | No       | Complete S3 URI for quantum-task results          |
| `AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_RESERVATION_ARN` | `char*`               | No       | Braket reservation ARN for a reserved time window |

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

The installed CMake target exports the generic device ID
`amazon.braket.default`, the `AMAZON_BRAKET` symbol prefix, and the relocatable
catalogue. An application using MQT Core can copy the library and catalogue
beside its executable:

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

Python consumers can point MQT Core at the complete installed catalogue before
the first driver call:

```python
import os

from amazon.braket.qdmi import AMAZON_BRAKET_QDMI_CATALOG_PATH

os.environ["MQT_CORE_QDMI_CONFIG_FILE"] = str(AMAZON_BRAKET_QDMI_CATALOG_PATH)

from mqt.core.qdmi.driver import open_device

device = open_device("amazon.braket.sv1")
```

The command `amazon-braket-qdmi --catalog_path` prints the same catalogue path.
Alternatively, an application can register only the generic device and configure
each fresh session with the desired device ARN and Region:

```python
from amazon.braket.qdmi import (
    AMAZON_BRAKET_QDMI_DEVICE_ID,
    AMAZON_BRAKET_QDMI_LIBRARY_PATH,
    AMAZON_BRAKET_QDMI_PREFIX,
)
from mqt.core.qdmi import driver

driver.register_device_if_absent(
    driver.DeviceDefinition(
        AMAZON_BRAKET_QDMI_DEVICE_ID,
        AMAZON_BRAKET_QDMI_LIBRARY_PATH,
        AMAZON_BRAKET_QDMI_PREFIX,
    )
)
device = driver.open_device(
    AMAZON_BRAKET_QDMI_DEVICE_ID,
    base_url="arn:aws:braket:::device/quantum-simulator/amazon/sv1",
    custom2="us-east-1",  # AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_REGION
)
```

`register_device_if_absent` preserves a higher-precedence definition already
registered under the stable ID. Explicit arguments to `open_device` configure
only the newly opened session.

### CMake Options

| Option                                    | Default | Description                            |
| ----------------------------------------- | ------- | -------------------------------------- |
| `BUILD_AMAZON_BRAKET_TESTS`               | `ON`    | Build the offline test suite           |
| `BUILD_AMAZON_BRAKET_LIVE_TESTS`          | `OFF`   | Build opt-in tests that access AWS     |
| `BUILD_AMAZON_BRAKET_SPANK_PLUGIN`        | `OFF`   | Build the optional Slurm SPANK plugin  |
| `USE_INSTALLED_AMAZON_BRAKET_QDMI_DEVICE` | `OFF`   | Use installed library instead of build |
| `CMAKE_PREFIX_PATH`                       | -       | Path to dependencies (AWS SDK, QDMI)   |

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

    // An explicit URI is optional. Without it, the environment or standard
    // Amazon Braket default bucket is used.
    const char* s3Uri = "s3://my-amazon-braket-bucket/tasks";
    AMAZON_BRAKET_QDMI_device_job_set_parameter(
        job, AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3URI,
        strlen(s3Uri) + 1, s3Uri);

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
| `QDMI_OPERATION_PROPERTY_NAME`          | Operation accepted by the device's OpenQASM action            |
| `QDMI_OPERATION_PROPERTY_QUBITSNUM`     | Fixed gate arity, when known                                  |
| `QDMI_OPERATION_PROPERTY_PARAMETERSNUM` | Number of scalar OpenQASM gate arguments, when representable  |
| `QDMI_OPERATION_PROPERTY_SITES`         | All sites, connectivity edges, or ordered three-site tuples   |
| `QDMI_OPERATION_PROPERTY_FIDELITY`      | Site-dependent gate fidelity when Braket reports one          |

Individual properties return `QDMI_ERROR_NOTSUPPORTED` when Braket advertises an
operation with a variable or otherwise unrepresentable signature. The zero-qubit
`gphase` operation has no site tuples. Fixed three-qubit gates are applicable to
all ordered tuples of distinct sites, independent of two-qubit connectivity.

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
| `AMAZON_BRAKET_QDMI_device_session_set_parameter()`            | (store session config)         | Set device ARN, region, and reservation  |
| `AMAZON_BRAKET_QDMI_device_session_init()`                     | `BraketClient` construction    | Configure the session's AWS clients      |
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

The default test build and CTest registry are offline and require no AWS
credentials:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Run the offline tests. CTest disables EC2 metadata lookup for this target.
ctest --test-dir build --output-on-failure
```

Live tests can query devices, submit SV1 tasks, write S3 objects, and incur
charges. Enable their separate CTest registration explicitly, provide AWS SDK
credentials and a pre-provisioned result bucket, then select the live label:

```bash
cmake -S . -B build-live -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_AMAZON_BRAKET_LIVE_TESTS=ON
cmake --build build-live

export AWS_S3_BUCKET="my-braket-results-bucket"
export AMAZON_BRAKET_QDMI_RUN_LIVE_CATALOG=1
ctest --test-dir build-live -L amazon-braket-live --output-on-failure
```

**Test Configuration:**

- **Device ARNs**: Hardcoded directly in test code (e.g.,
  `arn:aws:braket:::device/quantum-simulator/amazon/sv1`)
  - Device ARNs are public identifiers and don't need to be secrets
  - Tests use various devices to verify functionality
- **Credentials**: The AWS SDK resolves and refreshes credentials. The tests do
  not pass credentials through QDMI parameters.
- **S3 destination**: Quantum-task tests use `AMZN_BRAKET_TASK_RESULTS_S3_URI`
  so that they do not create AWS resources.
- **Catalog test**: Set `AMAZON_BRAKET_QDMI_RUN_LIVE_CATALOG=1` to query all
  nine installed devices. This test is serial and does not submit paid QPU
  tasks.
- **Automatic bucket test**: Set
  `AMAZON_BRAKET_QDMI_TEST_ALLOW_BUCKET_CREATION=1` and unset
  `AMZN_BRAKET_TASK_RESULTS_S3_URI`. This separately authorized test submits an
  SV1 task and can create the standard regional bucket.

The offline tests need no AWS credentials or network access. All tests that
access AWS are excluded from the default build and CTest registration.
The coverage workflow enables the live registry and catalogue check only when
its AWS access key, secret key, and S3 bucket secrets are all available.

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
