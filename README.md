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

### Supported AWS Braket Devices

This implementation supports all AWS Braket devices:

| Device Type | Examples |
|-------------|----------|
| **Simulators** | SV1 (State Vector), DM1 (Density Matrix), TN1 (Tensor Network) |
| **Gate-based QPUs** | IonQ Aria/Forte, IQM Garnet, Rigetti Ankaa |
| **Annealing QPUs** | D-Wave (via Braket Hybrid Jobs) |

## Quick Start

### Prerequisites

- **C++17** compatible compiler (GCC 8+, Clang 7+, MSVC 2019+)
- **CMake** 3.10+
- **AWS SDK for C++** with Braket component
- **QDMI** headers ([Munich-Quantum-Software-Stack/QDMI](https://github.com/Munich-Quantum-Software-Stack/QDMI))
- **AWS Credentials** configured (`~/.aws/credentials` or environment variables)

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
| `AWSSDK_ROOT` | - | Path to AWS SDK (if not in system path) |

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

## API Reference

### Lifecycle Functions

| Function | Description |
|----------|-------------|
| `AWS_QDMI_device_initialize()` | Initialize the library (call once at startup) |
| `AWS_QDMI_device_finalize()` | Cleanup resources (call once at shutdown) |

### Session Management

| Function | Description |
|----------|-------------|
| `AWS_QDMI_device_session_alloc()` | Allocate a new session |
| `AWS_QDMI_device_session_init()` | Initialize session and connect to device |
| `AWS_QDMI_device_session_free()` | Free session resources |
| `AWS_QDMI_device_session_set_parameter()` | Configure session (region, device ARN, S3) |
| `AWS_QDMI_device_session_query_device_property()` | Query device properties |
| `AWS_QDMI_device_session_query_site_property()` | Query qubit properties |
| `AWS_QDMI_device_session_query_operation_property()` | Query gate properties |

### Job Management

| Function | Description |
|----------|-------------|
| `AWS_QDMI_device_session_create_device_job()` | Create a new job |
| `AWS_QDMI_device_job_set_parameter()` | Set job parameters (circuit, shots, format) |
| `AWS_QDMI_device_job_submit()` | Submit job to AWS Braket |
| `AWS_QDMI_device_job_wait()` | Wait for job completion |
| `AWS_QDMI_device_job_check()` | Check job status |
| `AWS_QDMI_device_job_cancel()` | Cancel a running job |
| `AWS_QDMI_device_job_get_results()` | Retrieve measurement results |
| `AWS_QDMI_device_job_free()` | Free job resources |

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
├── CMakeLists.txt              # Build configuration
├── README.md                   # This file
├── include/
│   └── aws_qdmi/
│       ├── device.h            # Public API header
│       └── types.h             # AWS-specific types
├── src/
│   └── aws_qdmi_device_impl.cpp # Implementation
├── examples/
│   └── main.cpp                # Usage example
└── test/
    └── test_aws_qdmi_device_integration.cpp  # Integration tests
```

## Support

For issues related to:
- **This library**: Open an issue on this repository
- **QDMI specification**: See [QDMI repository](https://github.com/Munich-Quantum-Software-Stack/QDMI)
- **AWS Braket**: See [AWS Braket documentation](https://docs.aws.amazon.com/braket/)
