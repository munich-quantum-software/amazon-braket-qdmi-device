[![PyPI](https://img.shields.io/pypi/v/amazon-braket-qdmi?logo=pypi&style=flat-square)](https://pypi.org/project/amazon-braket-qdmi/)
![OS](https://img.shields.io/badge/os-linux%20%7C%20macos%20%7C%20windows-blue?style=flat-square)
[![License](https://img.shields.io/badge/license-Apache--2.0%20WITH%20LLVM--exception-blue.svg?style=flat-square)](LICENSE)
[![CI](https://img.shields.io/github/actions/workflow/status/munich-quantum-software/amazon-braket-qdmi-device/ci.yml?branch=main&style=flat-square&logo=github&label=ci)](https://github.com/munich-quantum-software/amazon-braket-qdmi-device/actions/workflows/ci.yml)
[![Documentation](https://img.shields.io/readthedocs/amazon-braket-qdmi-device?logo=readthedocs&style=flat-square)](https://amazon-braket-qdmi-device.readthedocs.io/)
[![codecov](https://img.shields.io/codecov/c/github/munich-quantum-software/amazon-braket-qdmi-device?style=flat-square&logo=codecov)](https://codecov.io/gh/munich-quantum-software/amazon-braket-qdmi-device)

# Amazon Braket QDMI Device

An implementation of the
[Quantum Device Management Interface (QDMI)](https://github.com/Munich-Quantum-Software-Stack/QDMI)
for Amazon Braket gate-model quantum processors and simulators.

The library exposes Amazon Braket devices through the vendor-neutral QDMI C
interface. It maps QDMI sessions and jobs to AWS SDK clients and QuantumTasks,
derives capabilities from the Amazon Braket device schema, and retrieves results
from S3.

<!-- rumdl-disable MD033 -->
<p align="center">
  <a href="https://amazon-braket-qdmi-device.readthedocs.io/">
  <img width="30%" src="https://img.shields.io/badge/documentation-blue?style=for-the-badge&logo=read%20the%20docs" alt="Documentation" />
  </a>
</p>
<!-- rumdl-enable MD033 -->

## Key Features

- A stable generic device, `amazon.braket.default`, for runtime configuration.
- An installed catalogue of stable IDs for supported Amazon Braket devices.
- Native gate sets, connectivity, calibration data, queue information, and job
  results through QDMI.
- AWS SDK credential-provider support, automatic S3 result buckets with optional
  overrides, and optional Slurm integration.
- Qiskit and PennyLane adapters backed by MQT Core.
- Concurrent QuantumTask submission and result prefetch through bounded AWS
  request pools.

## Getting Started

Install the Python package and packaged native library from PyPI:

```console
uv pip install amazon-braket-qdmi
```

Building from source requires a C++20 compiler and CMake 3.24 or newer:

```console
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
cmake --install build --prefix /path/to/install
```

## Where to Start

| I want to...                               | Read...                                                                                                        |
| ------------------------------------------ | -------------------------------------------------------------------------------------------------------------- |
| install the package or build from source   | [Installation](https://amazon-braket-qdmi-device.readthedocs.io/en/latest/installation.html)                   |
| configure AWS, devices, and S3             | [Configuration](https://amazon-braket-qdmi-device.readthedocs.io/en/latest/configuration.html)                 |
| inspect the installed device catalogue     | [Device catalogue](https://amazon-braket-qdmi-device.readthedocs.io/en/latest/device_catalog.html)             |
| execute Qiskit circuits                    | [Qiskit](https://amazon-braket-qdmi-device.readthedocs.io/en/latest/qiskit.html)                               |
| execute PennyLane programs                 | [PennyLane](https://amazon-braket-qdmi-device.readthedocs.io/en/latest/pennylane.html)                         |
| use the QDMI API and retrieve results      | [Usage](https://amazon-braket-qdmi-device.readthedocs.io/en/latest/usage.html)                                 |
| run through Slurm and the SPANK plugin     | [Slurm and SPANK](https://amazon-braket-qdmi-device.readthedocs.io/en/latest/slurm.html)                       |
| develop and test the provider              | [Development](https://amazon-braket-qdmi-device.readthedocs.io/en/latest/development.html)                     |

## Contributors and Support

The Amazon Braket QDMI Device is developed by [MQSC](https://mq.sc).

Please use
[GitHub Issues](https://github.com/munich-quantum-software/amazon-braket-qdmi-device/issues)
for bug reports and feature requests. See the
[support guide](https://amazon-braket-qdmi-device.readthedocs.io/en/latest/support.html)
for security and support contacts.

## License

The provider is licensed under the Apache License 2.0 with LLVM exceptions; see
[LICENSE](LICENSE). The optional Slurm SPANK plugin under [spank/](spank/) is
licensed separately under GPL-3.0-or-later; see
[spank/LICENSE.md](spank/LICENSE.md).
