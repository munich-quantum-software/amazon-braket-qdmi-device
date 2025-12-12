# AWS QDMI Device

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/std/the-standard)

AWS Braket implementation of the [Quantum Device Management Interface (QDMI)](https://github.com/Munich-Quantum-Software-Stack/QDMI) specification.

## Overview

This library enables **any QDMI-compliant quantum software** to run on AWS Braket quantum devices without code changes. Simply link against this library instead of another QDMI implementation, and your OpenQASM circuits will execute on AWS Braket simulators or real quantum hardware.

### What is QDMI?

QDMI (Quantum Device Management Interface) is a standardized C API for quantum devices, developed by the [Munich Quantum Software Company](https://github.com/munich-quantum-software). It provides a vendor-neutral interface for:

- Querying device properties (qubit count, connectivity, gate sets)
- Submitting quantum circuits (OpenQASM 2.0/3.0)
- Managing job lifecycle (submit, cancel, wait, get results)
- Accessing qubit and gate information (T1/T2 times, fidelities)

### QDMI to AWS Braket Mapping

| QDMI Function | AWS SDK Counterpart |
|---------------|---------------------|
| `AWS_QDMI_device_initialize()` | `Aws::InitAPI()` |
| `AWS_QDMI_device_session_init()` | `BraketClient` + `GetDevice()` |
| `AWS_QDMI_device_session_query_*()` | Parse `GetDevice` JSON response |
| `AWS_QDMI_device_job_submit()` | `BraketClient::CreateQuantumTask()` |
| `AWS_QDMI_device_job_check()` | `BraketClient::GetQuantumTask()` |
| `AWS_QDMI_device_job_cancel()` | `BraketClient::CancelQuantumTask()` |
| `AWS_QDMI_device_job_get_results()` | `S3Client::GetObject()` (results.json) |
| `AWS_QDMI_device_finalize()` | `Aws::ShutdownAPI()` |

### Supported AWS Braket Devices

This implementation supports all AWS Braket devices:

| Device Type | Examples |
|-------------|----------|
| **Simulators** | SV1 (State Vector), DM1 (Density Matrix), TN1 (Tensor Network) |
| **Gate-based QPUs** | IonQ Aria/Forte, IQM Garnet, Rigetti Ankaa |

## Quick Start

### Prerequisites

- **C++17** compatible compiler (GCC 8+, Clang 7+, MSVC 2019+)
- **CMake** 3.10+
- **AWS SDK for C++** with Braket and S3 components
- **QDMI** headers ([Munich-Quantum-Software-Stack/QDMI](https://github.com/Munich-Quantum-Software-Stack/QDMI))
- **AWS Credentials** configured (`~/.aws/credentials` or environment variables)

### Setup & Credentials

You can provide AWS credentials using environment variables (recommended for quick testing).

```bash
export AWS_ACCESS_KEY_ID="your_access_key_id"
export AWS_SECRET_ACCESS_KEY="your_secret_access_key"
# Optional: temporary session token
export AWS_SESSION_TOKEN="your_session_token"
# Default region used by the SDK if not provided via session/device ARN
export AWS_REGION="us-east-1"
```

### Building

```bash
# Clone the repository
git clone https://github.com/munich-quantum-software/aws-qdmi.git
cd aws-qdmi

# Create build directory
mkdir build && cd build

# Configure (adjust paths as needed)
cmake .. \
  -DQDMI_DIR=/path/to/QDMI \
  -DCMAKE_PREFIX_PATH=/path/to/aws-sdk-cpp/install

# Build
make -j$(nproc)

# Run example
./aws_qdmi_example
```

### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_EXAMPLES` | `ON` | Build example executable |
| `BUILD_TESTS` | `ON` | Build test suite (requires Google Test) |
| `QDMI_DIR` | `../QDMI` | Path to QDMI project |
| `CMAKE_PREFIX_PATH` | - | Path to AWS SDK installation |

## Usage

### Basic Example

```cpp
#include <aws_qdmi/device.h>
#include <cstring>
#include <iostream>

int main() {
    // Initialize
    AWS_QDMI_device_initialize();
    
    // Create and configure session
    AWS_QDMI_Device_Session session;
    AWS_QDMI_device_session_alloc(&session);
    
    // Set device ARN - region is automatically extracted from the ARN
    // Format: arn:aws:braket:<region>::device/... (regional QPUs)
    //     or: arn:aws:braket:::<device> (global simulators, defaults to us-east-1)
    const char* deviceArn = "arn:aws:braket:::device/quantum-simulator/amazon/sv1";
    AWS_QDMI_device_session_set_parameter(
        session, QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN,
        strlen(deviceArn) + 1, deviceArn);
    
    AWS_QDMI_device_session_init(session);
    
    // Query device properties
    size_t qubits;
    AWS_QDMI_device_session_query_device_property(
        session, QDMI_DEVICE_PROPERTY_QUBITSNUM,
        sizeof(qubits), &qubits, nullptr);
    std::cout << "Qubits: " << qubits << "\n";
    
    // Create and submit a job
    AWS_QDMI_Device_Job job;
    AWS_QDMI_device_session_create_device_job(session, &job);
    
    size_t shots = 1000;
    AWS_QDMI_device_job_set_parameter(
        job, QDMI_DEVICE_JOB_PARAMETER_SHOTSNUM,
        sizeof(shots), &shots);
    
    QDMI_Program_Format format = QDMI_PROGRAM_FORMAT_QASM3;
    AWS_QDMI_device_job_set_parameter(
        job, QDMI_DEVICE_JOB_PARAMETER_PROGRAMFORMAT,
        sizeof(format), &format);
    
    const char* circuit = R"(
        OPENQASM 3.0;
        qubit[2] q;
        h q[0];
        cnot q[0], q[1];
        bit[2] c = measure q;
    )";
    AWS_QDMI_device_job_set_parameter(
        job, QDMI_DEVICE_JOB_PARAMETER_PROGRAM,
        strlen(circuit) + 1, circuit);
    
    AWS_QDMI_device_job_submit(job);
    AWS_QDMI_device_job_wait(job, 60000);  // 60 second timeout
    
    // Get results
    QDMI_Job_Status status;
    AWS_QDMI_device_job_check(job, &status);
    if (status == QDMI_JOB_STATUS_DONE) {
        // Process histogram results...
    }
    
    // Cleanup
    AWS_QDMI_device_job_free(job);
    AWS_QDMI_device_session_free(session);
    AWS_QDMI_device_finalize();
    
    return 0;
}
```

### AWS-Specific Session Parameters

| Parameter | Required | Description |
|-----------|----------|-------------|
| `QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN` | **Yes** | Device ARN (region is auto-extracted) |
| `QDMI_DEVICE_SESSION_PARAMETER_S3BUCKET` | For jobs | S3 bucket for storing job results |
| `QDMI_DEVICE_SESSION_PARAMETER_REGION` | No | AWS region override (optional, auto-extracted from ARN) |

### Variables Used

| Category | Variables |
|----------|-----------|
| **Session Config** | `DEVICEARN`, `S3BUCKET`, `REGION` |
| **Device Info** | `deviceName`, `deviceStatus`, `qubitCount`, `sites[]`, `operations[]`, `couplingMap` |
| **Job Config** | `shotsNum`, `programFormat`, `program` |
| **Job State** | `jobId`, `taskArn`, `taskStatus` |
| **Results** | `outputS3Bucket`, `outputS3Directory`, `measurements[]`, `histogram` |

## API Reference

### Lifecycle Functions

| Function | AWS SDK Counterpart | Description |
|----------|---------------------|-------------|
| `AWS_QDMI_device_initialize()` | `Aws::InitAPI()` | Initialize the library (call once at startup) |
| `AWS_QDMI_device_finalize()` | `Aws::ShutdownAPI()` | Cleanup resources (call once at shutdown) |

### Session Management

| Function | AWS SDK Counterpart | Description |
|----------|---------------------|-------------|
| `AWS_QDMI_device_session_alloc()` | (internal allocation) | Allocate a new session |
| `AWS_QDMI_device_session_set_parameter()` | (store config) | Configure session (DEVICEARN, S3BUCKET, REGION) |
| `AWS_QDMI_device_session_init()` | `BraketClient` + `GetDevice()` | Initialize session and connect to device |
| `AWS_QDMI_device_session_free()` | (destroy BraketClient) | Free session resources |
| `AWS_QDMI_device_session_query_device_property()` | (parse GetDevice JSON) | Query device properties |
| `AWS_QDMI_device_session_query_site_property()` | (parse GetDevice JSON) | Query qubit properties |
| `AWS_QDMI_device_session_query_operation_property()` | (parse GetDevice JSON) | Query gate properties |

### Job Management

| Function | AWS SDK Counterpart | Description |
|----------|---------------------|-------------|
| `AWS_QDMI_device_session_create_device_job()` | (internal allocation) | Create a new job |
| `AWS_QDMI_device_job_set_parameter()` | (store job config) | Set job parameters (circuit, shots, format) |
| `AWS_QDMI_device_job_query_property()` | (return stored values) | Query job properties (ID, taskArn) |
| `AWS_QDMI_device_job_submit()` | `CreateQuantumTask()` | Submit job to AWS Braket |
| `AWS_QDMI_device_job_check()` | `GetQuantumTask()` | Check job status |
| `AWS_QDMI_device_job_wait()` | (poll `GetQuantumTask()`) | Wait for job completion |
| `AWS_QDMI_device_job_get_results()` | `S3Client::GetObject()` | Retrieve measurement results from S3 |
| `AWS_QDMI_device_job_cancel()` | `CancelQuantumTask()` | Cancel a running job |
| `AWS_QDMI_device_job_free()` | (internal cleanup) | Free job resources |

### AWS SDK Components Used

| SDK Component | Purpose |
|---------------|---------|
| `aws-cpp-sdk-core` | `Aws::InitAPI`, `ShutdownAPI`, `ClientConfiguration` |
| `aws-cpp-sdk-braket` | `BraketClient`, `GetDevice`, `CreateQuantumTask`, `GetQuantumTask`, `CancelQuantumTask` |
| `aws-cpp-sdk-s3` | `S3Client`, `GetObject` (download results.json) |

## Testing

```bash
# Set environment variables for testing
export AWS_REGION=us-east-1
export AWS_DEVICE_ARN=arn:aws:braket:::device/quantum-simulator/amazon/sv1
export AWS_S3_BUCKET=my-braket-results-bucket

# Run tests
cd build
ctest --output-on-failure
```

## Project Structure

```
aws-qdmi/
├── CMakeLists.txt                  # Build configuration
├── README.md                       # This file
├── include/
│   ├── aws_qdmi/
│   │   └── device.h                # Public API header (QDMI implementation)
│   └── aws_qdmi_device_impl.hpp    # Internal implementation header
├── src/
│   └── aws_qdmi_device_impl.cpp    # Implementation (QDMI↔AWS Braket)
├── examples/
│   └── main.cpp                    # Verbose example showing all mappings
└── test/
    └── test_aws_qdmi_device_integration.cpp  # Integration tests
```

## Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                        Your Application                             │
│                    (QDMI-compliant code)                           │
└─────────────────────────────────────────────────────────────────────┘
                                  │
                                  ▼
┌─────────────────────────────────────────────────────────────────────┐
│                        AWS QDMI Device                              │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │ QDMI Functions              │ AWS SDK Calls                 │   │
│  ├─────────────────────────────┼───────────────────────────────┤   │
│  │ device_session_init()       │ BraketClient::GetDevice()     │   │
│  │ device_job_submit()         │ CreateQuantumTask()           │   │
│  │ device_job_check()          │ GetQuantumTask()              │   │
│  │ device_job_get_results()    │ S3Client::GetObject()         │   │
│  └─────────────────────────────┴───────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────┘
                                  │
                                  ▼
┌─────────────────────────────────────────────────────────────────────┐
│                          AWS Braket                                 │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐              │
│  │ SV1 Simulator│  │ IonQ Aria    │  │ Rigetti Ankaa│  ...         │
│  └──────────────┘  └──────────────┘  └──────────────┘              │
└─────────────────────────────────────────────────────────────────────┘
```

## Support

For issues related to:
- **This library**: Open an issue on this repository
- **QDMI specification**: See [QDMI repository](https://github.com/Munich-Quantum-Software-Stack/QDMI)
- **AWS Braket**: See [AWS Braket documentation](https://docs.aws.amazon.com/braket/)
