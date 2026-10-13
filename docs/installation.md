# Installation

## Prerequisites

A source build requires:

- a C++20-compatible compiler;
- CMake 3.24 or later; and
- network access during initial configuration to obtain the AWS SDK for C++ and
  QDMI dependencies.

For Slurm job submission and cluster setup, see {doc}`slurm`.

## Python package

The Python package contains the native device library, public headers, CMake
configuration, and the installed device catalogue:

```console
uv pip install amazon-braket-qdmi
```

Install the adapter extras needed by the environment. Released wheels provide
the native Amazon Braket device and MQT Core; neither needs a source checkout.

```console
uv pip install "amazon-braket-qdmi[qiskit,pennylane]"
```

Use only the `qiskit` or `pennylane` extra when the environment needs one
application stack. Both extras require MQT Core 4.1.

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

A node that uses the centrally installed provider needs the Runtime component.
Python wheel-only deployments do not. Install the Development component only
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

MQT Core implements the QDMI Client Interface and loads this device library
through its builtin driver. The installed CMake target identifies its device
manifest through `QDMI_MANIFEST_NAME`. The manifest contains the stable device
IDs, symbol prefix, and relative library paths. An application can copy the
device library and manifest beside its executable. This integration requires
CMake 3.28 or later and MQT Core 4.1:

```cmake
find_package(mqt-core 4.1.0 CONFIG REQUIRED)
find_package(amazon-braket-qdmi-device CONFIG REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE MQT::CoreQDMI)
mqt_copy_qdmi_runtime(my_app amazon-braket-qdmi-device)
```

This placement is discovered automatically when the MQT Core Driver is linked
statically into the executable. A dynamically linked Driver searches beside its
own shared library. In that case, place the generated manifest there or register
the definition explicitly.

Python discovers the installed catalogue automatically. Each definition contains
the device ARN and AWS Region; the AWS SDK resolves credentials when MQT Core
opens the device.

```python
from mqt.core.qdmi import builtin_driver as qdmi_driver

device = qdmi_driver.open_device("amazon.braket.sv1")
```

For a native installation outside the driver's search paths, set
`MQT_CORE_QDMI_CONFIG_FILE` to its catalogue before opening a device. Relative
library paths are resolved from the catalogue directory. Site configuration
augments the installed definitions and overrides entries with the same ID.

See {doc}`configuration` for AWS authentication and {doc}`slurm` for cluster
setup and job examples.

## CMake options

| Option                                    | Default | Description                                                   |
| ----------------------------------------- | ------- | ------------------------------------------------------------- |
| `BUILD_AMAZON_BRAKET_TESTS`               | `ON`    | Build and register the offline test suite                     |
| `BUILD_AMAZON_BRAKET_LIVE_TESTS`          | `OFF`   | Register tests that access AWS and may submit paid tasks      |
| `USE_INSTALLED_AMAZON_BRAKET_QDMI_DEVICE` | `OFF`   | Use an installed device library when building tests           |
| `CMAKE_PREFIX_PATH`                       | --      | Search prefix for installed dependencies                      |
