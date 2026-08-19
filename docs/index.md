# Amazon Braket QDMI Device

The Amazon Braket QDMI Device implements the
[Quantum Device Management Interface (QDMI)](https://github.com/Munich-Quantum-Software-Stack/QDMI)
for gate-model devices available through Amazon Braket. QDMI applications use a
stable C interface to inspect device capabilities, configure OpenQASM programs,
manage their execution, and retrieve results. This device maps those operations
to Amazon Braket QuantumTasks and the corresponding AWS SDK clients.

QDMI and Amazon Braket use related terminology at different abstraction levels:

| QDMI term | Amazon Braket object                   | Role                                                 |
| --------- | -------------------------------------- | ---------------------------------------------------- |
| Device    | Device                                 | Quantum processor or simulator                       |
| Session   | Braket client and device configuration | Connection and capability context                    |
| Job       | QuantumTask                            | Execution of one circuit with a specified shot count |

Amazon Braket Hybrid Jobs, pulse-level programs, and non-gate-model devices are
not exposed by this implementation.

## Documentation structure

- {doc}`installation` describes package and source installation, CMake
  integration, and discovery through MQT Core.
- {doc}`configuration` specifies AWS authentication, device-session parameters,
  and S3 result destinations.
- {doc}`device_catalog` lists the installed device catalogue and explains the
  operation capability model.
- {doc}`pennylane` describes PennyLane execution and the Amazon Braket
  specialization.
- {doc}`usage` gives a complete direct-QDMI execution example.
<!-- rumdl-disable MD033 -->
- {doc}`api` records the supported QDMI properties, results, and lifecycle
  functions and links to the standalone <a href="cpp/index.html">native C++
  API</a> generated directly by Doxygen.
<!-- rumdl-enable MD033 -->
- {doc}`development` describes offline and explicitly enabled live tests.

<!-- rumdl-disable MD040 -->

```{toctree}
:maxdepth: 1
:caption: User guide

installation
configuration
device_catalog
pennylane
usage
api
```

```{toctree}
:maxdepth: 1
:caption: Project information

development
support
CHANGELOG
```

<!-- rumdl-enable MD040 -->
