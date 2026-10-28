# Supported QDMI interface

The Amazon Braket implementation exposes the standard QDMI lifecycle, session,
site, operation, job, and result interfaces. Standard values not listed on this
page return `QDMI_ERROR_NOTSUPPORTED`.

## Device status

The current UTC time must be inside a public execution window for an online
device to be reported as idle. When the session has
`AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_RESERVATION_ARN` set, public
execution windows are ignored because the session targets a reserved window.

| Amazon Braket status                         | QDMI status                      | Meaning                                                  |
| -------------------------------------------- | -------------------------------- | -------------------------------------------------------- |
| `ONLINE`, in window, queue below 5           | `QDMI_DEVICE_STATUS_IDLE`        | Device is available for submission                       |
| `ONLINE`, outside window or queue at least 5 | `QDMI_DEVICE_STATUS_BUSY`        | Device accepts queued work but is not idle               |
| `ONLINE`, reservation set, queue below 5     | `QDMI_DEVICE_STATUS_IDLE`        | Reserved access is treated as available                  |
| `OFFLINE`                                    | `QDMI_DEVICE_STATUS_MAINTENANCE` | Device is temporarily unavailable                        |
| `RETIRED`                                    | `QDMI_DEVICE_STATUS_OFFLINE`     | Device is permanently unavailable; submission is blocked |

```cpp
QDMI_Device_Status status;
AMAZON_BRAKET_QDMI_device_session_query_device_property(
    session, QDMI_DEVICE_PROPERTY_STATUS, sizeof(status), &status, nullptr);
```

## Device properties

| Property                              | Meaning                           |
| ------------------------------------- | --------------------------------- |
| `QDMI_DEVICE_PROPERTY_NAME`           | Amazon Braket device name         |
| `QDMI_DEVICE_PROPERTY_VERSION`        | Device-library version            |
| `QDMI_DEVICE_PROPERTY_STATUS`         | Current mapped device status      |
| `QDMI_DEVICE_PROPERTY_LIBRARYVERSION` | QDMI version                      |
| `QDMI_DEVICE_PROPERTY_QUBITSNUM`      | Number of qubits                  |
| `QDMI_DEVICE_PROPERTY_QUEUELENGTH`    | Current queued-task count         |
| `QDMI_DEVICE_PROPERTY_SITES`          | Qubit handles                     |
| `QDMI_DEVICE_PROPERTY_OPERATIONS`     | Native gate handles               |
| `QDMI_DEVICE_PROPERTY_COUPLINGMAP`    | Flat source-target site pairs     |

`AMAZON_BRAKET_QDMI_DEVICE_PROPERTY_OPENQASM3PROFILE` (`CUSTOM2`) returns a
null-terminated UTF-8 string: `simulator` for virtual-qubit OpenQASM, or
`native` for physical-qubit OpenQASM with verbatim execution. The latter
requires both capabilities in the device's metadata. An unavailable profile
returns `QDMI_ERROR_NOTSUPPORTED`. This query uses the cached device metadata.

## Site properties

| Property                   | Meaning                                         |
| -------------------------- | ----------------------------------------------- |
| `QDMI_SITE_PROPERTY_INDEX` | Site index                                      |
| `QDMI_SITE_PROPERTY_NAME`  | Site name                                       |
| `QDMI_SITE_PROPERTY_T1`    | Relaxation time, when reported by Amazon Braket |
| `QDMI_SITE_PROPERTY_T2`    | Dephasing time, when reported by Amazon Braket  |

## Operation properties

`AMAZON_BRAKET_QDMI_OPERATION_PROPERTY_PARAMETERS` (`CUSTOM2`) returns a
NUL-terminated JSON string using the temporary `mqt.operation-parameters.v1`
contract. Rigetti RX advertises four `alternatives`, each with a one-element
`fixed` array containing an allowed angle in radians (±π or ±π/2). Other
operations return `QDMI_ERROR_NOTSUPPORTED`. Core consumes this property for
both compiler targets and Qiskit targets; it is not a standardized QDMI
property.

| Property                                | Meaning                                                     |
| --------------------------------------- | ----------------------------------------------------------- |
| `QDMI_OPERATION_PROPERTY_NAME`          | Operation accepted by the device's OpenQASM action          |
| `QDMI_OPERATION_PROPERTY_QUBITSNUM`     | Fixed gate arity, when known                                |
| `QDMI_OPERATION_PROPERTY_PARAMETERSNUM` | Number of scalar OpenQASM arguments, when representable     |
| `QDMI_OPERATION_PROPERTY_SITES`         | All sites, connectivity edges, or ordered three-site tuples |
| `QDMI_OPERATION_PROPERTY_FIDELITY`      | Site-dependent gate fidelity, when reported                 |

An individual property returns `QDMI_ERROR_NOTSUPPORTED` when Amazon Braket
advertises an operation with a variable or otherwise unrepresentable signature.
The zero-qubit `gphase` operation has no site tuples. Fixed three-qubit gates
are applicable to all ordered tuples of distinct sites, independent of two-qubit
connectivity.

## Job properties

| Property                                             | Meaning                              |
| ---------------------------------------------------- | ------------------------------------ |
| `QDMI_DEVICE_JOB_PROPERTY_ID`                        | QuantumTask ARN after submission     |
| `QDMI_DEVICE_JOB_PROPERTY_PROGRAMFORMAT`             | Current program format               |
| `QDMI_DEVICE_JOB_PROPERTY_PROGRAMSNUM`               | Number of programs in input order    |
| `QDMI_DEVICE_JOB_PROPERTY_SHOTSNUM`                  | Current shot count                   |
| `QDMI_DEVICE_JOB_PROPERTY_QUEUEPOSITION`             | Jobs ahead while the task is queued  |
| `AMAZON_BRAKET_QDMI_DEVICE_JOB_PROPERTY_OUTPUTS3URI` | Resolved S3 result directory         |

`AMAZON_BRAKET_QDMI_device_job_get_program_status` queries a ProgramSet outcome
by input index.

Querying `QDMI_DEVICE_JOB_PROPERTY_QUEUEPOSITION` performs a fresh
`GetQuantumTask` request with the `QueueInfo` additional attribute. The query
returns `QDMI_ERROR_BADSTATE` unless the refreshed task status is `QUEUED`, and
`QDMI_ERROR_NOTSUPPORTED` if AWS does not provide a trustworthy position.

Querying `AMAZON_BRAKET_QDMI_DEVICE_JOB_PROPERTY_OUTPUTS3URI` refreshes the task
metadata and lets external tooling find its recorded `s3://bucket/directory`,
including for the automatic standard regional bucket. It returns
`QDMI_ERROR_BADSTATE` until Amazon Braket provides the result location.

An existing QuantumTask can be opened from its ARN. The opened handle exposes
the ARN and shot count, can be checked, waited for, canceled, and used to
retrieve results, but cannot be reconfigured or submitted again.

## Job status

| Event or Amazon Braket status             | QDMI job status             |
| ----------------------------------------- | --------------------------- |
| Local configurable job                    | `QDMI_JOB_STATUS_CREATED`   |
| Local submission pending or AWS `CREATED` | `QDMI_JOB_STATUS_SUBMITTED` |
| AWS `QUEUED`                              | `QDMI_JOB_STATUS_QUEUED`    |
| AWS `RUNNING`                             | `QDMI_JOB_STATUS_RUNNING`   |
| AWS `COMPLETED`                           | `QDMI_JOB_STATUS_DONE`      |
| AWS `FAILED`                              | `QDMI_JOB_STATUS_FAILED`    |
| Local cancellation or AWS `CANCELLED`     | `QDMI_JOB_STATUS_CANCELED`  |

The job becomes `SUBMITTED` when its request is queued locally, before AWS
acceptance. Reconfiguration and repeated submission return
`QDMI_ERROR_BADSTATE`. See
[Concurrent submission](usage.md#concurrent-submission) for cancellation and
job-lifetime behavior.

## Job results

| Result                        | Representation                              |
| ----------------------------- | ------------------------------------------- |
| `QDMI_JOB_RESULT_SHOTS`       | Comma-separated shot bitstrings             |
| `QDMI_JOB_RESULT_HIST_KEYS`   | Comma-separated histogram keys              |
| `QDMI_JOB_RESULT_HIST_VALUES` | `size_t` counts matching the histogram keys |

Shot strings and histogram keys use descending measured-qubit order: the lowest
measured qubit ID is the rightmost bit. The native device uses Braket's
`measuredQubits` metadata to locate response columns; it does not reconstruct
classical OpenQASM variables. The [Qiskit backend](qiskit.md) retains MQT Core's
result layout alongside each submitted program to restore classical-bit
positions.

## Lifecycle functions

| Function                                 | AWS SDK counterpart  | Purpose                        |
| ---------------------------------------- | -------------------- | ------------------------------ |
| `AMAZON_BRAKET_QDMI_device_initialize()` | `Aws::InitAPI()`     | Initialize the library once    |
| `AMAZON_BRAKET_QDMI_device_finalize()`   | `Aws::ShutdownAPI()` | Release library resources once |

## Session functions

| Function                                                       | AWS SDK counterpart         | Purpose                           |
| -------------------------------------------------------------- | --------------------------- | --------------------------------- |
| `AMAZON_BRAKET_QDMI_device_session_alloc()`                    | Internal allocation         | Allocate a session                |
| `AMAZON_BRAKET_QDMI_device_session_set_parameter()`            | Store session configuration | Set ARN, Region, and reservation  |
| `AMAZON_BRAKET_QDMI_device_session_init()`                     | AWS client construction     | Initialize the session            |
| `AMAZON_BRAKET_QDMI_device_session_free()`                     | Client destruction          | Release session resources         |
| `AMAZON_BRAKET_QDMI_device_session_query_device_property()`    | Lazy `GetDevice` schema     | Fetch and query device properties |
| `AMAZON_BRAKET_QDMI_device_session_query_site_property()`      | Cached `GetDevice` schema   | Query site properties             |
| `AMAZON_BRAKET_QDMI_device_session_query_operation_property()` | Cached `GetDevice` schema   | Query operation properties        |

## Job functions

| Function                                                        | AWS SDK counterpart             | Purpose                                     |
| --------------------------------------------------------------- | ------------------------------- | ------------------------------------------- |
| `AMAZON_BRAKET_QDMI_device_session_create_device_job()`         | Internal allocation             | Create a new QDMI job                       |
| `AMAZON_BRAKET_QDMI_device_session_retrieve_device_job_by_id()` | `GetQuantumTask()`              | Open an existing task by ARN                |
| `AMAZON_BRAKET_QDMI_device_job_set_parameter()`                 | Store job configuration         | Set shots and Braket options                |
| `AMAZON_BRAKET_QDMI_device_job_query_property()`                | Stored values or refreshed task | Query job properties                        |
| `AMAZON_BRAKET_QDMI_device_job_set_programs()`                  | Local configuration             | Set an ordered list of program payloads     |
| `AMAZON_BRAKET_QDMI_device_job_get_program()`                   | Stored values                   | Read one program payload by input index     |
| `AMAZON_BRAKET_QDMI_device_job_submit()`                        | `CreateQuantumTask()`           | Submit a QuantumTask                        |
| `AMAZON_BRAKET_QDMI_device_job_check()`                         | `GetQuantumTask()`              | Refresh task status                         |
| `AMAZON_BRAKET_QDMI_device_job_wait()`                          | Poll `GetQuantumTask()`         | Wait for completion                         |
| `AMAZON_BRAKET_QDMI_device_job_get_results()`                   | `S3Client::GetObject()`         | Retrieve results from S3                    |
| `AMAZON_BRAKET_QDMI_device_job_cancel()`                        | `CancelQuantumTask()`           | Cancel a task                               |
| `AMAZON_BRAKET_QDMI_device_job_free()`                          | Internal cleanup                | Release job resources                       |
