# Amazon Braket QDMI Device

Amazon Braket implementation of the [Quantum Device Management Interface (QDMI)](https://github.com/Munich-Quantum-Software-Stack/QDMI) specification.

## Overview

This library enables **any QDMI-compliant quantum software** to run on Amazon Braket quantum devices without code changes. Simply link against this library instead of another QDMI implementation, and your OpenQASM circuits will execute on Amazon Braket simulators (and soon real quantum hardware).

### What is QDMI?

QDMI (Quantum Device Management Interface) is a standardized C API for quantum devices, developed among others by the [Munich Quantum Software Company](https://github.com/munich-quantum-software). It provides a vendor-neutral interface for:

- Querying device properties (e.g., qubit count, connectivity, gate sets)
- Submitting quantum circuits (e.g., OpenQASM 2.0/3.0)
- Managing job lifecycle (e.g., submit, cancel, get results)
- Accessing qubit and gate information (e.g., T1/T2 times, fidelities)

### What is Amazon Braket?

TODO

### Supported Amazon Braket Devices

This implementation currently only supports simulator Amazon Braket devices:

| Device Type         | Examples                                                       |
| ------------------- | -------------------------------------------------------------- |
| **Simulators**      | SV1 (State Vector), DM1 (Density Matrix), TN1 (Tensor Network) |
| **Gate-based QPUs** | TODO                                                           |

## Quick Start

### Prerequisites

- **C++20** compatible compiler
- **CMake** 3.24 or later
- **AWS Credentials** configured (see Configuration section below)

**Note**: Dependencies (AWS SDK for C++, QDMI) are automatically downloaded and built by CMake during the configuration step.

### Configuration

**AWS Credentials**

Configure your AWS credentials using either:

1. **AWS Configuration Files** (recommended):

   ```bash
   # ~/.aws/credentials
   [default]
   aws_access_key_id = YOUR_ACCESS_KEY
   aws_secret_access_key = YOUR_SECRET_KEY
   ```

2. **Environment Variables**:
   ```bash
   export AWS_ACCESS_KEY_ID="your_access_key_id"
   export AWS_SECRET_ACCESS_KEY="your_secret_access_key"
   # Optional: for temporary credentials
   export AWS_SESSION_TOKEN="your_session_token"
   ```

**Device Configuration**

Set the target device and S3 bucket (required for job execution):

```bash
export AWS_DEVICE_ARN="arn:aws:braket:::device/quantum-simulator/amazon/sv1"
export AWS_S3_BUCKET="your-results-bucket"
```

| Variable         | Required | Description                                |
| ---------------- | -------- | ------------------------------------------ |
| `AWS_DEVICE_ARN` | Yes      | Amazon Braket device ARN to target         |
| `AWS_S3_BUCKET`  | Yes      | S3 bucket for storing quantum job results  |
| `AWS_REGION`     | No       | AWS region (auto-detected from device ARN) |

### Installation

The library uses CMake for building and installation. The workflow mirrors standard CMake practices:

**Step 1: Build the Library**

```bash
# Clone the repository
git clone https://github.com/munich-quantum-software/aws-qdmi-device.git
cd aws-qdmi-device

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
#include <cstring>
#include <iostream>

int main() {
    // Initialize the library
    AMAZON_BRAKET_QDMI_device_initialize();

    // Create and initialize session
    AMAZON_BRAKET_QDMI_Device_Session session;
    AMAZON_BRAKET_QDMI_device_session_alloc(&session);
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

A device can be in one of the following states:

| Status                       | Description                                                                              |
| ---------------------------- | ---------------------------------------------------------------------------------------- |
| `QDMI_DEVICE_STATUS_IDLE`    | Device is online and ready to accept jobs. No QDMI jobs are currently running.           |
| `QDMI_DEVICE_STATUS_BUSY`    | Device is online but currently executing one or more QDMI jobs. New jobs will be queued. |
| `QDMI_DEVICE_STATUS_OFFLINE` | Device is not available (maintenance, calibration, or permanently unavailable).          |

The device status is automatically managed:

- Starts as `IDLE` after successful session initialization (otherwise `OFFLINE`)
- Transitions to `BUSY` when QDMI jobs are submitted
- Returns to `IDLE` when all QDMI jobs complete
- Amazon Braket device availability determines `OFFLINE` state

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

### Job Management

| Function                                                | AWS SDK Counterpart       | Description                                 |
| ------------------------------------------------------- | ------------------------- | ------------------------------------------- |
| `AMAZON_BRAKET_QDMI_device_session_create_device_job()` | (internal allocation)     | Create a new job                            |
| `AMAZON_BRAKET_QDMI_device_job_set_parameter()`         | (store job config)        | Set job parameters (circuit, shots, format) |
| `AMAZON_BRAKET_QDMI_device_job_query_property()`        | (return stored values)    | Query job properties (ID, taskArn)          |
| `AMAZON_BRAKET_QDMI_device_job_submit()`                | `CreateQuantumTask()`     | Submit job to AWS Braket                    |
| `AMAZON_BRAKET_QDMI_device_job_check()`                 | `GetQuantumTask()`        | Check job status                            |
| `AMAZON_BRAKET_QDMI_device_job_wait()`                  | (poll `GetQuantumTask()`) | Wait for job completion                     |
| `AMAZON_BRAKET_QDMI_device_job_get_results()`           | `S3Client::GetObject()`   | Retrieve measurement results from S3        |
| `AMAZON_BRAKET_QDMI_device_job_cancel()`                | `CancelQuantumTask()`     | Cancel a running job                        |
| `AMAZON_BRAKET_QDMI_device_job_free()`                  | (internal cleanup)        | Free job resources                          |

## Testing

To run the test suite:

```bash
# Build with tests enabled (default)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Set required environment variables
export AWS_DEVICE_ARN="arn:aws:braket:::device/quantum-simulator/amazon/sv1"
export AWS_S3_BUCKET="my-braket-results-bucket"

# Run tests
ctest --test-dir build --output-on-failure
```

## Project Structure

```
aws-qdmi/
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
│  │ SV1 Simulator│  │ DM1 Simulator│  │ TN1 Simulator│  ...          │
│  └──────────────┘  └──────────────┘  └──────────────┘               │
└─────────────────────────────────────────────────────────────────────┘
```

## Support

For issues related to:

- **This library**: Open an issue on this repository
- **QDMI specification**: See [QDMI repository](https://github.com/Munich-Quantum-Software-Stack/QDMI)
- **Amazon Braket**: See [Amazon Braket documentation](https://docs.aws.amazon.com/braket/)
