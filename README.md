# Amazon Braket QDMI Device

Amazon Braket implementation of the [Quantum Device Management Interface (QDMI)](https://github.com/Munich-Quantum-Software-Stack/QDMI) specification.

## Overview

This library enables **any QDMI-compliant quantum software** to run on Amazon Braket quantum devices without code changes. Simply link against this library instead of another QDMI implementation, and your OpenQASM circuits will execute on Amazon Braket simulators (and soon real quantum hardware).

### What is QDMI?

QDMI (Quantum Device Management Interface) is a standardized C API for quantum devices, developed among others by the [Munich Quantum Software Company](https://github.com/munich-quantum-software). It provides a vendor-neutral interface for:

- Querying device properties (e.g., qubit count, connectivity, gate sets)
- Submitting quantum circuits (e.g., OpenQASM 2.0/3.0)
- Managing QDMI job lifecycle (create, submit, monitor, retrieve results)
- Accessing qubit and gate information (e.g., T1/T2 times, fidelities)

### What is Amazon Braket?

TODO

### Terminology

**Important:** This library uses QDMI terminology, which differs from AWS Braket:

| QDMI Term | AWS Braket Equivalent | Description                                             |
| --------- | --------------------- | ------------------------------------------------------- |
| **Job**   | **QuantumTask**       | A single quantum circuit execution with specified shots |
| Device    | Device                | Quantum processor or simulator                          |
| Session   | BraketClient          | Connection to AWS Braket service                        |

**Not supported:** AWS Braket "Hybrid Jobs" (combined classical and quantum workflows)
are **not** supported by this library. This library only handles QuantumTasks
(pure quantum circuit execution).

### Supported Amazon Braket Devices

This implementation currently supports submitting jobs to simulator and gate-based Amazon Braket devices.
Additionally, support for querying properties (e.g., qubit count, gate set) is implemented for:

| Device Type         | Examples                                                                   |
| ------------------- | -------------------------------------------------------------------------- |
| **Simulators**      | AWS SV1 (State Vector), AWS DM1 (Density Matrix), AWS TN1 (Tensor Network) |
| **Gate-based QPUs** | IQM Garnet, IQM Emerald                                                    |

## Quick Start

### Prerequisites

- **C++20** compatible compiler
- **CMake** 3.24 or later
- **AWS Credentials** configured (see Configuration section below)

**Note**: Dependencies (AWS SDK for C++, QDMI) are automatically downloaded and built by CMake during the configuration step.

### Configuration

**AWS Credentials**

This library supports two methods for providing AWS credentials:

**Method 1: Credentials File (Recommended for Multi-User Scenarios)**

Use the QDMI `AUTHFILE` parameter to specify a credentials file path:

```c
#include <amazon-braket-qdmi-device/Constants.hpp>

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
    session, QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN,
    strlen(deviceArn) + 1, deviceArn);

AMAZON_BRAKET_QDMI_device_session_init(session);
```

**Credentials File Format** (standard AWS INI format):

```ini
[default]
aws_access_key_id=AKIAIOSFODNN7EXAMPLE
aws_secret_access_key=wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY
# Optional for temporary credentials
aws_session_token=IQoJb3JpZ2luX2VjEOT//////////...
```

**Note:** The credentials file should contain only one profile section. The parser reads the first profile found. This method allows different sessions to use different credentials within the same process.

**Method 2: Direct Parameters**

Use the QDMI custom parameters to specify credentials programmatically:

```c
#include <amazon-braket-qdmi-device/Constants.hpp>

// Set credentials directly via QDMI parameters
const char* accessKey = "AKIAIOSFODNN7EXAMPLE";
AMAZON_BRAKET_QDMI_device_session_set_parameter(
    session, QDMI_DEVICE_SESSION_PARAMETER_AWS_ACCESS_KEY_ID,
    strlen(accessKey) + 1, accessKey);

const char* secretKey = "wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY";
AMAZON_BRAKET_QDMI_device_session_set_parameter(
    session, QDMI_DEVICE_SESSION_PARAMETER_AWS_SECRET_ACCESS_KEY,
    strlen(secretKey) + 1, secretKey);

// Optional: session token for temporary credentials (STS, SSO)
const char* sessionToken = "IQoJb3JpZ2luX2VjEOT//////////...";
AMAZON_BRAKET_QDMI_device_session_set_parameter(
    session, QDMI_DEVICE_SESSION_PARAMETER_AWS_SESSION_TOKEN,
    strlen(sessionToken) + 1, sessionToken);
```

**Available Credential Parameters:**

| Parameter                                             | Type    | Required | Description                                   |
| ----------------------------------------------------- | ------- | -------- | --------------------------------------------- |
| `QDMI_DEVICE_SESSION_PARAMETER_AUTHFILE`              | `char*` | No       | Path to AWS credentials file (INI format)     |
| `QDMI_DEVICE_SESSION_PARAMETER_AWS_ACCESS_KEY_ID`     | `char*` | No       | AWS Access Key ID                             |
| `QDMI_DEVICE_SESSION_PARAMETER_AWS_SECRET_ACCESS_KEY` | `char*` | No       | AWS Secret Access Key                         |
| `QDMI_DEVICE_SESSION_PARAMETER_AWS_SESSION_TOKEN`     | `char*` | No       | AWS Session Token (for temporary credentials) |

**Device Configuration**

Configure the device using QDMI session parameters:

```cpp
#include <amazon-braket-qdmi-device/Constants.hpp>

// Configure session parameters before initialization
const char* deviceArn = "arn:aws:braket:eu-north-1::device/qpu/iqm/Garnet";
AMAZON_BRAKET_QDMI_device_session_set_parameter(
    session, QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN,
    strlen(deviceArn) + 1, deviceArn);
```

**Configuration Parameters**

| Parameter                                 | Type    | Required | Description                                         |
| ----------------------------------------- | ------- | -------- | --------------------------------------------------- |
| `QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN` | `char*` | Yes      | Amazon Braket device ARN                            |
| `QDMI_DEVICE_SESSION_PARAMETER_REGION`    | `char*` | No       | AWS region override (extracted from ARN by default) |

**Note**: AWS authentication is handled via:

- `QDMI_DEVICE_SESSION_PARAMETER_AUTHFILE` for credentials files (see AWS Credentials section)
- `QDMI_DEVICE_SESSION_PARAMETER_AWS_ACCESS_KEY_ID`, `QDMI_DEVICE_SESSION_PARAMETER_AWS_SECRET_ACCESS_KEY`, `QDMI_DEVICE_SESSION_PARAMETER_AWS_SESSION_TOKEN` for direct credentials

Special QDMI authentication parameters (`USERNAME`, `PASSWORD`, `TOKEN`, `AUTHURL`, `BASEURL`) are not supported - use the AWS-specific parameters above instead.

### Job Configuration

Each QDMI job (which becomes an Amazon Braket QuantumTask) requires S3 storage
configuration for results. Configure using job-level parameters:

```cpp
#include <amazon-braket-qdmi-device/Constants.hpp>

// Create and configure a job
AMAZON_BRAKET_QDMI_Device_Job job;
AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &job);

// Set S3 bucket (required)
const char* s3Bucket = "my-braket-results";
AMAZON_BRAKET_QDMI_device_job_set_parameter(
    job, QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3BUCKET,
    strlen(s3Bucket) + 1, s3Bucket);

// Set S3 prefix (optional - auto-generates timestamp-based prefix if not set)
const char* s3Prefix = "my-experiment/run-42/";
AMAZON_BRAKET_QDMI_device_job_set_parameter(
    job, QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3PREFIX,
    strlen(s3Prefix) + 1, s3Prefix);
```

**Job S3 Parameters**

| Parameter                                   | Type    | Required | Description                                                          |
| ------------------------------------------- | ------- | -------- | -------------------------------------------------------------------- |
| `QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3BUCKET`  | `char*` | Yes      | S3 bucket for quantum task results                                   |
| `QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3PREFIX`  | `char*` | No       | S3 prefix for results (defaults to timestamp: `<epoch-millis>`)      |
| `QDMI_DEVICE_JOB_PARAMETER_RESERVATION_ARN` | `char*` | No       | Braket reservation ARN to route the task into a reserved time window |

### Installation

The library uses CMake for building and installation. The workflow mirrors standard CMake practices:

**Step 1: Build the Library**

```bash
# Clone the repository
git clone https://github.com/munich-quantum-software/amazon-braket-qdmi-device.git
cd amazon-braket-qdmi-device

# Configure
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --config Release
```

**Step 2: Install the Library**

```bash
# Install to a prefix (e.g., ~/.local or /usr/local)
cmake --install build --prefix /path/to/install
```

**Step 3: Use in Your Project**

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

### CMake Options

| Option                                    | Default | Description                             |
| ----------------------------------------- | ------- | --------------------------------------- |
| `BUILD_AMAZON_BRAKET_TESTS`               | `ON`    | Build test suite (requires Google Test) |
| `USE_INSTALLED_AMAZON_BRAKET_QDMI_DEVICE` | `OFF`   | Use installed library instead of build  |
| `CMAKE_PREFIX_PATH`                       | -       | Path to dependencies (AWS SDK, QDMI)    |

## Usage

### Example Program

```cpp
#include <amazon_braket_qdmi/device.h>
#include <amazon-braket-qdmi-device/Constants.hpp>
#include <cstring>
#include <iostream>

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
        session, QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN,
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
    const char* circuit = R"(
        OPENQASM 3.0;
        qubit[2] q;
        h q[0];
        cnot q[0], q[1];
        bit[2] c = measure q;
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
        // Retrieve histogram results...
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

The library maps Amazon Braket device states to QDMI status values:

| Amazon Braket Status | QDMI Status                      | Description                                                                                 |
| -------------------- | -------------------------------- | ------------------------------------------------------------------------------------------- |
| `ONLINE` (queue < 5) | `QDMI_DEVICE_STATUS_IDLE`        | Device is operational and ready to accept quantum tasks.                                    |
| `ONLINE` (queue ≥ 5) | `QDMI_DEVICE_STATUS_BUSY`        | Device is operational but has a significant queue of pending tasks.                         |
| `OFFLINE`            | `QDMI_DEVICE_STATUS_MAINTENANCE` | Device is temporarily unavailable (maintenance/calibration). Tasks will queue until return. |
| `RETIRED`            | `QDMI_DEVICE_STATUS_OFFLINE`     | Device is permanently decommissioned. Task submission is blocked.                           |

**Note**: Amazon Braket does not distinguish between maintenance and calibration - both are reported as OFFLINE and mapped to the QDMI MAINTENANCE status.

Query the current status using:

```cpp
QDMI_Device_Status status;
AMAZON_BRAKET_QDMI_device_session_query_device_property(
    session, QDMI_DEVICE_PROPERTY_STATUS,
    sizeof(status), &status, nullptr);
```

### Lifecycle Functions

| Function                                 | AWS SDK Counterpart  | Description                                   |
| ---------------------------------------- | -------------------- | --------------------------------------------- |
| `AMAZON_BRAKET_QDMI_device_initialize()` | `Aws::InitAPI()`     | Initialize the library (call once at startup) |
| `AMAZON_BRAKET_QDMI_device_finalize()`   | `Aws::ShutdownAPI()` | Cleanup resources (call once at shutdown)     |

### Session Management

| Function                                                       | AWS SDK Counterpart            | Description                              |
| -------------------------------------------------------------- | ------------------------------ | ---------------------------------------- |
| `AMAZON_BRAKET_QDMI_device_session_alloc()`                    | (internal allocation)          | Allocate a new session                   |
| `AMAZON_BRAKET_QDMI_device_session_init()`                     | `BraketClient` + `GetDevice()` | Initialize session and connect to device |
| `AMAZON_BRAKET_QDMI_device_session_free()`                     | `BraketClient` destructor      | Free session resources                   |
| `AMAZON_BRAKET_QDMI_device_session_query_device_property()`    | (parse GetDevice JSON)         | Query device properties                  |
| `AMAZON_BRAKET_QDMI_device_session_query_site_property()`      | (parse GetDevice JSON)         | Query qubit properties                   |
| `AMAZON_BRAKET_QDMI_device_session_query_operation_property()` | (parse GetDevice JSON)         | Query gate properties                    |

### Job Management (QDMI Jobs → AWS QuantumTasks)

| Function                                                | AWS SDK Counterpart       | Description                                 |
| ------------------------------------------------------- | ------------------------- | ------------------------------------------- |
| `AMAZON_BRAKET_QDMI_device_session_create_device_job()` | (internal allocation)     | Create a new QDMI job                       |
| `AMAZON_BRAKET_QDMI_device_job_set_parameter()`         | (store job config)        | Set job parameters (circuit, shots, format) |
| `AMAZON_BRAKET_QDMI_device_job_query_property()`        | (return stored values)    | Query job properties (ID, taskArn)          |
| `AMAZON_BRAKET_QDMI_device_job_submit()`                | `CreateQuantumTask()`     | Submit QDMI job as AWS QuantumTask          |
| `AMAZON_BRAKET_QDMI_device_job_check()`                 | `GetQuantumTask()`        | Check quantum task status                   |
| `AMAZON_BRAKET_QDMI_device_job_wait()`                  | (poll `GetQuantumTask()`) | Wait for quantum task completion            |
| `AMAZON_BRAKET_QDMI_device_job_get_results()`           | `S3Client::GetObject()`   | Retrieve measurement results from S3        |
| `AMAZON_BRAKET_QDMI_device_job_cancel()`                | `CancelQuantumTask()`     | Cancel a running quantum task               |
| `AMAZON_BRAKET_QDMI_device_job_free()`                  | (internal cleanup)        | Free job resources                          |

## Testing

To run the test suite:

```bash
# Build with tests enabled (default)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Set required environment variables for credentials and S3 bucket
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

- **Device ARNs**: Hardcoded directly in test code (e.g., `arn:aws:braket:::device/quantum-simulator/amazon/sv1`)
  - Device ARNs are public identifiers and don't need to be secrets
  - Tests use various devices to verify functionality

- **Credentials**: Read from environment variables and passed to QDMI parameters
  - **Method 1:** `AWS_CREDENTIALS_FILE` → passed to `QDMI_DEVICE_SESSION_PARAMETER_AUTHFILE`
  - **Method 2:** `AWS_ACCESS_KEY_ID` + `AWS_SECRET_ACCESS_KEY` + `AWS_SESSION_TOKEN` (optional) → passed to respective QDMI parameters
  - Method 1 is recommended for local development
  - Method 2 is recommended for CI/CD pipelines where credentials are stored as secrets

- **S3 Bucket**: Read from `AWS_S3_BUCKET` environment variable
  - Passed to `QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3BUCKET` in job tests

**Note:** The library itself does **not** read environment variables directly. Tests read env vars for sensitive data (credentials, S3 bucket) and call QDMI set parameter functions, which is the same pattern your production code should use.

## Project Structure

```
amazon-braket-qdmi-device/
├── CMakeLists.txt                  # Build configuration
├── README.md                       # This file
├── include/
│   └── amazon-braket-qdmi-device/
│       └── Device.hpp              # Public API header (QDMI implementation)
├── src/
│   └── Device.cpp                  # Implementation (QDMI↔Amazon Braket)
└── test/
    └── test_device.cpp             # Integration tests
```

## Architecture

```
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
- **QDMI specification**: See [QDMI repository](https://github.com/Munich-Quantum-Software-Stack/QDMI)
- **Amazon Braket**: See [Amazon Braket documentation](https://docs.aws.amazon.com/braket/)
