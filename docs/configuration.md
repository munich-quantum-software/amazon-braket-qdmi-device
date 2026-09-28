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

The device uses AWS SDK retries, defaulting to `standard` mode and up to
**10 total attempts** per request. Configure retries through AWS environment
variables or your AWS profile before initialization. For example:

```console
export AWS_MAX_ATTEMPTS=15
```

See the [AWS retry reference] for `AWS_RETRY_MODE`, attempt limits, profile
settings, and retry behavior.

Device initialization enables `AWS_NEW_RETRIES_2026=true` unless explicitly set.
This process-wide setting remains after finalization and can affect other AWS
clients. Initialize the device before other threads use the SDK; if your
application initializes the SDK first, set the variable before that step. Set it
to `false` before startup to opt out.

Task submissions also retry `ServiceQuotaExceededException`. Retries are
bounded: persistent quota exhaustion can still fail a submission. Increase
`AWS_MAX_ATTEMPTS`, reduce concurrent work, or request an adjustable
[Amazon Braket quota] increase when needed.

[AWS retry reference]: https://docs.aws.amazon.com/sdkref/latest/guide/feature-retry-behavior.html
[Amazon Braket quota]: https://docs.aws.amazon.com/braket/latest/developerguide/braket-quotas.html

## Diagnostics

To see AWS service errors and SDK retry delays, enable logging before starting
the application:

```console
export AMAZON_BRAKET_QDMI_LOG_LEVEL=warn
```

The setting accepts `off` (the default), `fatal`, `error`, `warn`, `info`,
`debug`, and `trace`, ignoring case. An unset or empty value disables logging;
an invalid value makes device initialization fail. The SDK writes
`aws_sdk_*.log` files in the working directory. Higher verbosity can include
request and response details; use `warn` for routine retry diagnosis.

Logging is process-wide and configured when the SDK first initializes. If your
application initializes the SDK before the device, configure logging there. See
the
[AWS SDK logging guide](https://docs.aws.amazon.com/sdk-for-cpp/v1/developer-guide/logging.html)
for log levels and output behavior.

For structured request-attempt metrics, the SDK also supports Client Side
Monitoring (CSM), independently of logging. Set `AWS_CSM_ENABLED=true`,
`AWS_CSM_HOST=127.0.0.1`, and `AWS_CSM_PORT=31000` before initialization and
start a local UDP JSON receiver on that port. Count `ApiCallAttempt` events by
`Api` and `AwsException` to distinguish submission retries from polling errors.
These count HTTP attempts, not distinct failed QuantumTasks.

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

Set the ordered circuit payloads with
`AMAZON_BRAKET_QDMI_device_job_set_programs`. All programs in one job share
these parameters.

| Parameter                                                 | Type                  | Required | Description                                |
| --------------------------------------------------------- | --------------------- | -------- | ------------------------------------------ |
| `QDMI_DEVICE_JOB_PARAMETER_PROGRAMFORMAT`                 | `QDMI_Program_Format` | No       | QASM2 or QASM3; default QASM3              |
| `QDMI_DEVICE_JOB_PARAMETER_SHOTSNUM`                      | `size_t`              | No       | Shots per program; defaults to 100         |
| `AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3URI`     | `char*`               | No       | Complete S3 URI for QuantumTask results    |
| `AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_RESERVATION_ARN` | `char*`               | No       | Reservation ARN for a reserved time window |
