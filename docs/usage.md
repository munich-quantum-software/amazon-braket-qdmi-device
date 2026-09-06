# Direct QDMI execution

The following program initializes the Amazon Braket QDMI implementation, opens
an SV1 session, submits an OpenQASM 3 Bell-state circuit, and reads its
histogram. AWS credentials must be available through the default provider chain
described in {doc}`configuration`.

```cpp
#include <amazon-braket-qdmi-device/constants.hpp>
#include <amazon_braket_qdmi/device.h>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

int main() {
    AMAZON_BRAKET_QDMI_device_initialize();

    AMAZON_BRAKET_QDMI_Device_Session session = nullptr;
    AMAZON_BRAKET_QDMI_device_session_alloc(&session);

    const char* deviceArn =
        "arn:aws:braket:::device/quantum-simulator/amazon/sv1";
    AMAZON_BRAKET_QDMI_device_session_set_parameter(
        session, AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN,
        strlen(deviceArn) + 1, deviceArn);
    AMAZON_BRAKET_QDMI_device_session_init(session);

    size_t qubits = 0;
    AMAZON_BRAKET_QDMI_device_session_query_device_property(
        session, QDMI_DEVICE_PROPERTY_QUBITSNUM, sizeof(qubits), &qubits,
        nullptr);
    std::cout << "Device has " << qubits << " qubits\n";

    AMAZON_BRAKET_QDMI_Device_Job job = nullptr;
    AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &job);

    size_t shots = 1000;
    AMAZON_BRAKET_QDMI_device_job_set_parameter(
        job, QDMI_DEVICE_JOB_PARAMETER_SHOTSNUM, sizeof(shots), &shots);

    QDMI_Program_Format format = QDMI_PROGRAM_FORMAT_QASM3;
    AMAZON_BRAKET_QDMI_device_job_set_parameter(
        job, QDMI_DEVICE_JOB_PARAMETER_PROGRAMFORMAT, sizeof(format), &format);

    const char* circuit = R"(OPENQASM 3.0;
        qubit[2] q;
        bit[2] c;
        h q[0];
        cnot q[0], q[1];
        c[0] = measure q[0];
        c[1] = measure q[1];
    )";
    AMAZON_BRAKET_QDMI_device_job_set_parameter(
        job, QDMI_DEVICE_JOB_PARAMETER_PROGRAM, strlen(circuit) + 1, circuit);

    AMAZON_BRAKET_QDMI_device_job_submit(job);
    AMAZON_BRAKET_QDMI_device_job_wait(job, 60);

    QDMI_Job_Status status;
    AMAZON_BRAKET_QDMI_device_job_check(job, &status);
    if (status == QDMI_JOB_STATUS_DONE) {
        size_t keysSize = 0;
        size_t valuesSize = 0;
        AMAZON_BRAKET_QDMI_device_job_get_results(
            job, QDMI_JOB_RESULT_HIST_KEYS, 0, nullptr, &keysSize);
        AMAZON_BRAKET_QDMI_device_job_get_results(
            job, QDMI_JOB_RESULT_HIST_VALUES, 0, nullptr, &valuesSize);

        std::vector<char> keys(keysSize);
        std::vector<size_t> counts(valuesSize / sizeof(size_t));
        AMAZON_BRAKET_QDMI_device_job_get_results(
            job, QDMI_JOB_RESULT_HIST_KEYS, keysSize, keys.data(), nullptr);
        AMAZON_BRAKET_QDMI_device_job_get_results(
            job, QDMI_JOB_RESULT_HIST_VALUES, valuesSize, counts.data(),
            nullptr);

        std::cout << "Shot counts: {";
        std::stringstream keyStream(keys.data());
        std::string key;
        for (size_t i = 0; i < counts.size(); ++i) {
            std::getline(keyStream, key, ',');
            std::cout << (i == 0 ? "" : ", ") << '"' << key
                      << "\": " << counts[i];
        }
        std::cout << "}\n";
    }

    AMAZON_BRAKET_QDMI_device_job_free(job);
    AMAZON_BRAKET_QDMI_device_session_free(session);
    AMAZON_BRAKET_QDMI_device_finalize();
    return 0;
}
```

The QDMI program format may be OpenQASM 2 or OpenQASM 3. OpenQASM 3 is used by
default. Amazon Braket Hybrid Jobs are distinct from QuantumTasks and are not
created by this interface.

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
so ID queries and cancellation wait for pending acceptance. Avoid querying each
ID immediately after submission, which serializes the batch.

Freeing a job or session stops background polling and drains pending HTTP
requests. It does not wait for remote execution or cancel the QuantumTask.
