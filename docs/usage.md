# Native job execution

Use
[MQT Core's QDMI client](https://mqt.readthedocs.io/projects/core/en/latest/qdmi/driver.html)
for device discovery, submission, and result retrieval. This page describes the
Amazon Braket execution model; {doc}`configuration` covers AWS credentials and
session settings, and {doc}`api` lists the supported native interfaces.

The device accepts OpenQASM 2 and OpenQASM 3. QPU submissions must already use
the device's native gates and physical sites. Amazon Braket Hybrid Jobs are
distinct from QuantumTasks and are not created by this interface.

## Execution architecture

```text
QDMI application
    |
    v
Amazon Braket QDMI Device
    device_session_init()       -> construct AWS clients
    first property query        -> BraketClient::GetDevice()
    device_job_submit()         -> queue CreateQuantumTask() on AWS worker pool
    device_job_check()          -> GetQuantumTask()
    device_job_get_results()    -> S3Client::GetObject()
    |
    v
Amazon Braket gate-model QPU or simulator
```

The device schema returned by `GetDevice` determines the QDMI sites,
connectivity, operations, and calibration data. Static properties are cached for
the session; open a new session to refresh them. Explicit device status and
queue-length queries refresh `GetDevice`. Results use the S3 location returned
by Amazon Braket.

### Concurrent submission

`device_job_submit()` queues the request and returns before AWS acceptance.
Submit the whole batch before waiting for results. Each session uses separate
eight-worker pools for submission and result prefetch; these limit local HTTP
requests, not remote running tasks. Foreground reads use prefetched results when
available and can fetch results without waiting for the prefetch queue.

Submitted jobs are immutable and report `SUBMITTED` while AWS acceptance is
pending. Submission failures appear through `device_job_check()` and
`device_job_wait()` with status `FAILED`. Job IDs remain AWS QuantumTask ARNs,
so ID queries wait for pending acceptance. Avoid querying each ID immediately
after submission, which serializes the batch.

To stop unwanted work, call `device_job_cancel()` before freeing the job.
Requests still waiting locally are canceled immediately without creating a
QuantumTask. If `CreateQuantumTask` has already started, cancellation waits for
its ARN and sends a cancellation request to Braket. Check the job status to
confirm the outcome: a remote task can finish before cancellation takes effect.

Freeing a job or session stops background polling and drains pending worker
callbacks and HTTP requests. It does not wait for remote execution or cancel the
QuantumTask.

## Multiple circuits

Qiskit and PennyLane group compatible circuits into one QuantumTask when the
device advertises OpenQASM 3 ProgramSets. Each circuit uses the configured shot
count; the request's total shots are the circuit count multiplied by that value.
Devices without ProgramSets, including SV1 and DM1, use separate QuantumTasks.
Each program must also satisfy the device's advertised shot range. Different
shot counts or formats also use separate jobs.

For direct QDMI use, pass the ordered program pointers and sizes to
`AMAZON_BRAKET_QDMI_device_job_set_programs`. A null program-pointer array
probes the exact format and count with the current job parameters. Unsupported
groups return `QDMI_ERROR_NOTSUPPORTED` before submission. The implementation
supports one fully bound OpenQASM 3 executable per program; parameter sweeps are
not supported.

Program and result indices follow input order. Use
`AMAZON_BRAKET_QDMI_device_job_get_program` to read a stored program; historical
task payloads may be unavailable. ProgramSets expose
`AMAZON_BRAKET_QDMI_device_job_get_program_status` after the task finishes,
allowing completed circuits to retain their results when another circuit fails
or is cancelled. MQT Core can retry failed circuits without repeating successful
ones. While the task is active, the status query returns `QDMI_ERROR_BADSTATE`;
ordinary single-circuit tasks return `QDMI_ERROR_NOTSUPPORTED`. Result-download
errors remain errors and do not trigger another execution.

A retrieved ProgramSet preserves its program count and indexed outcomes. Foreign
ProgramSets that expand programs into multiple executables cannot be reopened.
See
[AWS ProgramSets](https://docs.aws.amazon.com/braket/latest/developerguide/braket-batching-tasks.html)
for the service model and device capabilities.
