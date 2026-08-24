# Installation

## Prerequisites

A source build requires:

- a C++20-compatible compiler;
- CMake 3.24 or later; and
- network access during initial configuration to obtain the AWS SDK for C++ and
  QDMI dependencies.

Slurm 23.02 or later is required only for the optional SPANK plugin. CI tests
the plugin against Slurm 23.11 on Ubuntu 24.04. Its build and deployment are
documented in the authoritative {doc}`slurm` guide.

## Python package

The Python package contains the native device library, public headers, CMake
configuration, and the installed device catalogue:

```console
uv pip install amazon-braket-qdmi
```

Install an adapter extra when the environment will execute Qiskit or PennyLane
payloads. Released wheels provide the native Amazon Braket device and MQT Core;
neither needs a source checkout.

```console
uv pip install "amazon-braket-qdmi[qiskit]"
uv pip install "amazon-braket-qdmi[pennylane]"
```

The command-line entry point reports the installed catalogue path:

```console
amazon-braket-qdmi --catalog_path
```

## Source build

Clone, configure, and build the library with:

```console
git clone https://github.com/munich-quantum-software/amazon-braket-qdmi-device.git
cd amazon-braket-qdmi-device
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Install the resulting library, headers, device catalogue, and package
configuration into a selected prefix:

```console
cmake --install build --prefix /path/to/install
```

The full installation contains runtime and development files. Component-based
installations split them as follows:

| Component                                      | Contents                                      |
| ---------------------------------------------- | --------------------------------------------- |
| `amazon-braket-qdmi-device_Runtime`            | Shared library and QDMI device catalogue      |
| `amazon-braket-qdmi-device_Development`        | Headers, library link, and CMake package files|
| `amazon-braket-qdmi-spank-plugin`              | Optional SPANK module and plugstack template  |

A job node needs the Runtime component. Install the Development component only
when other software must compile against the provider. In particular,
`amazon-braket-qdmi-device-config.cmake` belongs to the Development component;
its absence after a Runtime-only installation is expected.

A consuming CMake project can import the installed target:

```cmake
cmake_minimum_required(VERSION 3.24)
project(MyQuantumApp)

find_package(amazon-braket-qdmi-device REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE amazon-braket-qdmi-device)
```

Configure the consuming project with the installation prefix:

```console
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/install
cmake --build build
```

## MQT Core integration

The installed CMake target exports the `AMAZON_BRAKET` symbol prefix and a
relocatable catalogue with all stable device definitions. An application using
MQT Core 3.9.1 or newer can copy the device library and catalogue beside its
executable:

```cmake
find_package(mqt-core 3.9.1 CONFIG REQUIRED)
find_package(amazon-braket-qdmi-device CONFIG REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE MQT::CoreFoMaC)
mqt_copy_qdmi_runtime(my_app amazon-braket-qdmi-device)
```

This placement is discovered automatically when the MQT Core Driver is linked
statically into the executable. A dynamically linked Driver searches beside its
own shared library. In that case, place the generated manifest there or register
the definition explicitly.

Python consumers can select the installed catalogue before the first QDMI driver
operation. Each definition contains the exact device ARN and AWS Region; the AWS
SDK resolves credentials when MQT Core opens the device.

```python
import os

from amazon.braket.qdmi import AMAZON_BRAKET_QDMI_CATALOG_PATH
from mqt.core.qdmi import driver

os.environ["MQT_CORE_QDMI_CONFIG_FILE"] = str(AMAZON_BRAKET_QDMI_CATALOG_PATH)
device = driver.open_device("amazon.braket.sv1")
```

Cluster administrators may configure local Slurm licenses for concrete catalogue
IDs such as `amazon.braket.sv1`. Do not configure `amazon.braket.default` as a
license because that generic device still requires runtime configuration.
License counts are cluster-admission limits, not Amazon Braket QuantumTask
quotas.

## Slurm admission and AWS authorization

Slurm licenses and AWS IAM are independent controls. Slurm uses the local
license count to limit admitted jobs and to account for that shared resource.
MQT Core uses the process-mutable `SLURM_JOB_LICENSES` value to select the
persistent QDMI definition. This value does not attest the Slurm allocation and
does not authorize AWS access.

AWS IAM authorizes calls to Amazon Braket, STS, and S3. Apply least-privilege
policies to IAM users and groups, and to workload or node roles. In particular,
`braket:CreateQuantumTask` supports resource-level permissions for a Braket
device ARN. Also restrict task inspection and result-bucket access to the
required resources. See the [Amazon Braket service authorization reference] and
the [device access guide].

The optional SPANK plugin injects the installed QDMI catalogue path and optional
AWS configuration references. It does not distribute credentials or grant AWS
permissions. A profile, file, workload identity, or node role that it references
must already be available to the job user. See {doc}`slurm` for the complete
installation and job workflow.

## CMake options

| Option                                    | Default | Description                                                   |
| ----------------------------------------- | ------- | ------------------------------------------------------------- |
| `BUILD_AMAZON_BRAKET_TESTS`               | `ON`    | Build and register the offline test suite                     |
| `BUILD_AMAZON_BRAKET_LIVE_TESTS`          | `OFF`   | Register tests that access AWS and may submit paid tasks      |
| `BUILD_AMAZON_BRAKET_SPANK_PLUGIN`        | `OFF`   | Build the optional Slurm SPANK plugin                         |
| `USE_INSTALLED_AMAZON_BRAKET_QDMI_DEVICE` | `OFF`   | Use an installed device library when building tests           |
| `CMAKE_PREFIX_PATH`                       | --      | Search prefix for installed dependencies                      |

[Amazon Braket service authorization reference]: https://docs.aws.amazon.com/service-authorization/latest/reference/list_amazonbraket.html
[device access guide]: https://docs.aws.amazon.com/braket/latest/developerguide/restrict-access.html
