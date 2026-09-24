# AWS and job configuration

## Authentication

The device uses the
[AWS SDK for C++ default credential provider chain](https://docs.aws.amazon.com/sdk-for-cpp/v1/developer-guide/credproviders.html)
for Amazon Braket, S3, and STS. The chain supports environment credentials,
shared AWS profiles, `credential_process`, web identity, container credentials,
and instance roles. It also refreshes temporary credentials.

Select the required AWS profile or another credential source before starting the
process. For example:

```console
export AWS_PROFILE=hpc-quantum
```

Do not store access keys or session tokens in a QDMI device definition. The
generic QDMI `AUTHFILE`, `USERNAME`, `PASSWORD`, and `TOKEN` session parameters
return `QDMI_ERROR_NOTSUPPORTED`.

Linux wheels contain their HTTP and TLS libraries but use the host's CA trust
store. The provider discovers the standard system CA bundle automatically. Set
`AWS_CA_BUNDLE` to an explicit PEM bundle when the host uses a private CA or a
nonstandard location. `SSL_CERT_FILE` is also supported when `AWS_CA_BUNDLE` is
not set. An invalid explicit path is reported by the AWS client; certificate
verification is never disabled.

## Retries and quotas

The device delegates request retries to the AWS SDK for C++ for Amazon Braket,
S3, and STS. It enables the SDK's 2026 retry behavior by setting
`AWS_NEW_RETRIES_2026=true` during device initialization, before initializing
the SDK, if the variable is absent. The default retry mode is `standard`, with
up to **10 total attempts** per request, including the initial attempt. The SDK
handles error classification, exponential backoff with jitter, and the retry
token budget. Exhausting that budget can end retries before the attempt limit.

The opt-in is process-wide and remains set after device finalization. It can
affect other AWS clients created in the same process. Initialize the device at
process startup, before other threads use the AWS SDK or modify the environment.
If the application initializes the SDK itself first, set the variable before
that initialization so that the SDK's shared error classification also uses the
new behavior. Set `AWS_NEW_RETRIES_2026=false` before starting the process to
opt out; the device preserves explicit values.

Tune retries without rebuilding or adding QDMI parameters:

```console
export AWS_PROFILE=hpc-quantum
export AWS_RETRY_MODE=standard
export AWS_MAX_ATTEMPTS=15
```

Alternatively, configure the selected AWS profile in `~/.aws/config`:

```ini
[profile hpc-quantum]
retry_mode = standard
max_attempts = 15
```

Environment settings take precedence over profile settings. The device supplies
10 attempts only when neither contains an attempt limit. Set
`AWS_MAX_ATTEMPTS=1` to disable request retries. Configure these settings before
device and session initialization; existing clients retain their retry
strategies. Higher limits can make submission, polling, cancellation, and result
retrieval take longer. An in-flight SDK call can also outlast the QDMI job-wait
timeout.

Use `standard` for general workloads. `adaptive` also limits outgoing requests
per client and can delay the first attempt. Each session shares its Braket
client across API operations, which have different rate limits: throttling
submissions can also delay polling and cancellation. Keep `standard` unless this
tradeoff suits your workload. See the [AWS retry reference] for details.

### Simulator batches

Submit the whole batch before waiting for results. Each simulator session keeps
at most **10 outstanding QuantumTasks**, including submissions in flight, and
queues further submissions locally until tasks finish. This follows the
[Braket Python SDK's batch approach]. QPU jobs use Braket's remote queue.

Set the per-session limit before session initialization:

```console
export AMAZON_BRAKET_QDMI_MAX_PARALLEL=20
```

Choose a positive integer that leaves room within your [Amazon Braket quotas]
for other sessions, processes, and applications using the same account and
Region. This setting applies to simulators only; it does not discover or raise
account quotas. Freeing an accepted job handle does not free its capacity slot:
the session tracks the remote task until it finishes. Canceling a locally queued
job prevents its submission. An in-flight request must finish before
cancellation can reach the remote task. Freeing a job drains pending submission
and can wait for capacity; cancel it first to discard queued work.

The device classifies `ServiceQuotaExceededException` from task submission as
retryable throttling, so the SDK also retries capacity races with other clients.
The SDK owns backoff and attempt limits; there is no additional request-retry
loop. Persistent quota exhaustion or failed capacity queries can still fail a
submission after SDK retries. Increase `AWS_MAX_ATTEMPTS`, reduce concurrency,
or request an adjustable quota increase for sustained capacity shortages.

Validation and permission failures return without retries. After the SDK stops
retrying, the device reports the failed outcome through QDMI and logs the AWS
diagnostic. QuantumTask status polling remains separate from request retries.

[AWS retry reference]: https://docs.aws.amazon.com/sdkref/latest/guide/feature-retry-behavior.html
[Braket Python SDK's batch approach]: https://amazon-braket-sdk-python.readthedocs.io/en/latest/_apidoc/braket.aws.aws_quantum_task_batch.html
[Amazon Braket quotas]: https://docs.aws.amazon.com/braket/latest/developerguide/braket-quotas.html

## Device session

Set the Amazon Braket device ARN before initializing a direct QDMI session:

```cpp
#include <amazon-braket-qdmi-device/constants.hpp>

const char* deviceArn = "arn:aws:braket:eu-north-1::device/qpu/iqm/Garnet";
AMAZON_BRAKET_QDMI_device_session_set_parameter(
    session, AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN,
    strlen(deviceArn) + 1, deviceArn);
```

| Parameter                                                     | Type    | Required | Description                                                     |
| ------------------------------------------------------------- | ------- | -------- | --------------------------------------------------------------- |
| `AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN`       | `char*` | Yes      | Amazon Braket device ARN                                        |
| `AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_REGION`          | `char*` | No       | AWS Region override; otherwise extracted from the ARN           |
| `AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_RESERVATION_ARN` | `char*` | No       | Reservation ARN used for status reporting and inherited by jobs |

Installed catalogue definitions provide the device ARN and Region, so consumers
opening a stable device ID through MQT Core do not set these parameters
manually.

## S3 result destination

Amazon Braket requires an S3 destination for every QuantumTask. By default, the
device uses `amazon-braket-<region>-<account-id>` with the prefix `tasks`. It
resolves the account with STS and creates and secures this standard bucket when
needed. This work starts only when the first job is submitted; opening a device
and querying properties do not require STS or S3 permissions.

An explicit destination overrides the automatic one in this order:

1. `AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3URI` on the job.
2. `AMZN_BRAKET_TASK_RESULTS_S3_URI` in the process environment.

Both forms contain a complete URI such as `s3://my-results/experiments/run-42`.
They do not call STS or an S3 bucket management API.

```cpp
#include <amazon-braket-qdmi-device/constants.hpp>

AMAZON_BRAKET_QDMI_Device_Job job = nullptr;
AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &job);

const char* s3Uri = "s3://my-braket-results/experiments/run-42";
AMAZON_BRAKET_QDMI_device_job_set_parameter(
    job, AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3URI,
    strlen(s3Uri) + 1, s3Uri);
```

For a restricted HPC role, provision the bucket in advance and set the job URI
or `AMZN_BRAKET_TASK_RESULTS_S3_URI`. This path needs object access but does not
need STS, `CreateBucket`, or `PutPublicAccessBlock`.

## Job parameters

| Parameter                                                 | Type                  | Required | Description                                |
| --------------------------------------------------------- | --------------------- | -------- | ------------------------------------------ |
| `QDMI_DEVICE_JOB_PARAMETER_PROGRAM`                       | `char*`               | Yes      | OpenQASM circuit source                    |
| `QDMI_DEVICE_JOB_PARAMETER_PROGRAMFORMAT`                 | `QDMI_Program_Format` | No       | QASM2 or QASM3; default QASM3              |
| `QDMI_DEVICE_JOB_PARAMETER_SHOTSNUM`                      | `size_t`              | No       | Number of shots; defaults to 100           |
| `AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3URI`     | `char*`               | No       | Complete S3 URI for QuantumTask results    |
| `AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_RESERVATION_ARN` | `char*`               | No       | Reservation ARN for a reserved time window |
