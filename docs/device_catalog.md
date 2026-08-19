# Gate-model device catalogue

The installed catalogue contains the generic `amazon.braket.default` device and
stable QDMI IDs for Amazon Braket gate-model QPUs and on-demand simulators. The
generic device takes its device ARN and optional AWS Region from runtime
configuration. Each concrete definition stores both values.

| Stable QDMI ID                          | Region                |
| --------------------------------------- | --------------------- |
| `amazon.braket.default`                 | Configured at runtime |
| `amazon.braket.aqt.ibex-q1`             | `eu-north-1`          |
| `amazon.braket.ionq.forte-1`            | `us-east-1`           |
| `amazon.braket.ionq.forte-enterprise-1` | `us-east-1`           |
| `amazon.braket.iqm.garnet`              | `eu-north-1`          |
| `amazon.braket.iqm.emerald`             | `eu-north-1`          |
| `amazon.braket.rigetti.ankaa-3`         | `us-west-1`           |
| `amazon.braket.rigetti.cepheus-1-108q`  | `us-west-1`           |
| `amazon.braket.sv1`                     | `us-east-1`           |
| `amazon.braket.dm1`                     | `us-east-1`           |

The catalogue is an installation-time snapshot of known Amazon Braket devices.
Device capabilities are obtained lazily from Amazon Braket on the first device
property query.

## Operation sets

For a QPU, `QDMI_DEVICE_PROPERTY_OPERATIONS` contains the Amazon Braket
`nativeGateSet`. The custom property
`AMAZON_BRAKET_QDMI_DEVICE_PROPERTY_SUPPORTEDOPERATIONS` contains the broader
OpenQASM `supportedOperations` set. Equal names in both sets refer to the same
QDMI operation handle.

SV1 and DM1 do not publish a hardware-native gate set. For these simulators, the
standard QDMI operation property contains the executable OpenQASM operation set.

Each operation retains its fixed qubit and parameter arity when Amazon Braket
provides a representable signature. Applicability contains all sites for
single-qubit operations, connectivity edges for two-qubit operations, and
ordered tuples of distinct sites for fixed three-qubit operations. Reported
calibration data is associated with the corresponding site tuple.
