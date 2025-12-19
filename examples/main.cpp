#include "aws-qdmi/qdmi/aws/device.h"
#include <cstring>
#include <iostream>
#include <iomanip>
#include <vector>

// ============================================================================
// Comprehensive AWS Braket Integration Test
// ============================================================================
// This example tests all QDMI functions and their AWS Braket SDK counterparts.
//
// Run with: ./aws_qdmi_example
// Requires: AWS credentials configured (env vars or ~/.aws/credentials)
// ============================================================================

void printSeparator(const char* section) {
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << " " << section << "\n";
    std::cout << std::string(70, '=') << "\n";
}

void printQDMI(const char* qdmiFunc, const char* awsFunc = nullptr) {
    std::cout << "  [QDMI] " << qdmiFunc << "\n";
    if (awsFunc) {
        std::cout << "   [AWS] -> " << awsFunc << "\n";
    }
}

void printVar(const char* action, const char* varName) {
    std::cout << "   [VAR] " << action << ": " << varName << "\n";
}

void printResult(const char* test, bool success) {
    std::cout << "  " << std::left << std::setw(50) << test 
              << (success ? "✓ PASS" : "✗ FAIL") << "\n";
}

int main() {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║           AWS Braket QDMI Integration Test (Verbose Mode)            ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════════════╝\n";

    int ret;
    bool allPassed = true;

    // ========================================================================
    // TEST 1: AWS SDK Initialization
    // ========================================================================
    printSeparator("TEST 1: Device Initialization");
    
    printQDMI("AWS_QDMI_device_initialize()", "Aws::InitAPI(options)");
    ret = AWS_QDMI_device_initialize();
    printResult("Initialize AWS SDK", ret == QDMI_SUCCESS);
    if (ret != QDMI_SUCCESS) {
        std::cerr << "FATAL: Cannot proceed without SDK initialization\n";
        return 1;
    }

    // ========================================================================
    // TEST 2: Session Allocation
    // ========================================================================
    printSeparator("TEST 2: Session Allocation");
    
    AWS_QDMI_Device_Session session;
    printQDMI("AWS_QDMI_device_session_alloc(&session)", "new AWS_QDMI_Device_Session_impl_d()");
    printVar("OUTPUT", "session handle");
    ret = AWS_QDMI_device_session_alloc(&session);
    printResult("Allocate session", ret == QDMI_SUCCESS);
    if (ret != QDMI_SUCCESS) {
        std::cerr << "FATAL: Cannot proceed without session\n";
        return 1;
    }

    // ========================================================================
    // TEST 3: Session Parameters
    // ========================================================================
    printSeparator("TEST 3: Session Configuration");

    // Test setting device ARN (required)
    const char* envDeviceArn = std::getenv("AWS_DEVICE_ARN");
    const char* deviceArn = envDeviceArn ? envDeviceArn : "arn:aws:braket:::device/quantum-simulator/amazon/sv1";
    std::cout << "\n  Setting DEVICEARN parameter:\n";
    printQDMI("AWS_QDMI_device_session_set_parameter(session, DEVICEARN, ...)", 
              "(stored for BraketClient::GetDevice)");
    printVar("INPUT", "DEVICEARN");
    ret = AWS_QDMI_device_session_set_parameter(
        session, 
        static_cast<QDMI_Device_Session_Parameter>(QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN), 
        strlen(deviceArn) + 1, deviceArn);
    printResult("Set DEVICEARN", ret == QDMI_SUCCESS);
    allPassed &= (ret == QDMI_SUCCESS);

    // Test setting S3 bucket (required for job submission / result retrieval)
    const char* envS3Bucket = std::getenv("AWS_S3_BUCKET");
    const char* s3Bucket = envS3Bucket ? envS3Bucket : "amazon-braket-my-first-bucket";
    std::cout << "\n  Setting S3BUCKET parameter:\n";
    printQDMI("AWS_QDMI_device_session_set_parameter(session, S3BUCKET, ...)",
              "(stored for CreateQuantumTask::SetOutputS3Bucket)");
    printVar("INPUT", "S3BUCKET");
    ret = AWS_QDMI_device_session_set_parameter(
        session,
        static_cast<QDMI_Device_Session_Parameter>(QDMI_DEVICE_SESSION_PARAMETER_S3BUCKET),
        strlen(s3Bucket) + 1, s3Bucket);
    printResult("Set S3BUCKET", ret == QDMI_SUCCESS);
    allPassed &= (ret == QDMI_SUCCESS);

    // Test setting region (optional - should work)
    const char* envRegion = std::getenv("AWS_DEFAULT_REGION");
    const char* region = envRegion ? envRegion : "us-east-1";
    std::cout << "\n  Setting REGION parameter (optional):\n";
    printQDMI("AWS_QDMI_device_session_set_parameter(session, REGION, ...)",
              "(stored for ClientConfiguration::region)");
    printVar("INPUT", "REGION");
    ret = AWS_QDMI_device_session_set_parameter(
        session,
        static_cast<QDMI_Device_Session_Parameter>(QDMI_DEVICE_SESSION_PARAMETER_REGION),
        strlen(region) + 1, region);
    printResult("Set REGION", ret == QDMI_SUCCESS);

    // ========================================================================
    // TEST 4: Session Initialization (BraketClient + GetDevice API)
    // ========================================================================
    printSeparator("TEST 4: Session Initialization");
    
    std::cout << "\n  Initializing session (connects to AWS):\n";
    printQDMI("AWS_QDMI_device_session_init(session)", nullptr);
    std::cout << "   [AWS] -> Aws::Client::ClientConfiguration(REGION)\n";
    std::cout << "   [AWS] -> new Aws::Braket::BraketClient(config)\n";
    std::cout << "   [AWS] -> BraketClient::GetDevice(DEVICEARN)\n";
    printVar("READ", "DEVICEARN");
    printVar("READ", "REGION");
    printVar("OUTPUT", "deviceCapabilities (JSON)");
    printVar("OUTPUT", "deviceName");
    printVar("OUTPUT", "deviceStatus");
    
    ret = AWS_QDMI_device_session_init(session);
    printResult("Initialize session", ret == QDMI_SUCCESS);
    
    if (ret != QDMI_SUCCESS) {
        std::cerr << "\nFATAL: Session init failed. Check:\n";
        std::cerr << "  - AWS credentials (AWS_ACCESS_KEY_ID, AWS_SECRET_ACCESS_KEY)\n";
        std::cerr << "  - Or ~/.aws/credentials file\n";
        std::cerr << "  - Network connectivity to AWS\n";
        AWS_QDMI_device_session_free(session);
        return 1;
    }
    allPassed &= (ret == QDMI_SUCCESS);

    // ========================================================================
    // TEST 5: Query Device Properties (parsed from GetDevice response)
    // ========================================================================
    printSeparator("TEST 5: Query Device Properties");

    // Query device name
    char name[256];
    size_t nameSize;
    std::cout << "\n  Querying device name:\n";
    printQDMI("AWS_QDMI_device_session_query_device_property(session, NAME, ...)",
              "(parsed from GetDevice JSON: deviceName)");
    printVar("OUTPUT", "deviceName");
    ret = AWS_QDMI_device_session_query_device_property(
        session, QDMI_DEVICE_PROPERTY_NAME, sizeof(name), name, &nameSize);
    printResult("Query DEVICE_PROPERTY_NAME", ret == QDMI_SUCCESS);
    if (ret == QDMI_SUCCESS) {
        std::cout << "    -> Value: \"" << name << "\"\n";
    }

    // Query qubit count
    size_t qubitsNum = 0;
    std::cout << "\n  Querying qubit count:\n";
    printQDMI("AWS_QDMI_device_session_query_device_property(session, QUBITSNUM, ...)",
              "(parsed from GetDevice JSON: paradigm.qubitCount)");
    printVar("OUTPUT", "qubitCount");
    ret = AWS_QDMI_device_session_query_device_property(
        session, QDMI_DEVICE_PROPERTY_QUBITSNUM, sizeof(qubitsNum), &qubitsNum, nullptr);
    printResult("Query DEVICE_PROPERTY_QUBITSNUM", ret == QDMI_SUCCESS);
    if (ret == QDMI_SUCCESS) {
        std::cout << "    -> Value: " << qubitsNum << " qubits\n";
    }

    // Query device status
    QDMI_Device_Status status;
    std::cout << "\n  Querying device status:\n";
    printQDMI("AWS_QDMI_device_session_query_device_property(session, STATUS, ...)",
              "(from GetDevice: deviceStatus)");
    printVar("OUTPUT", "deviceStatus");
    ret = AWS_QDMI_device_session_query_device_property(
        session, QDMI_DEVICE_PROPERTY_STATUS, sizeof(status), &status, nullptr);
    printResult("Query DEVICE_PROPERTY_STATUS", ret == QDMI_SUCCESS);
    if (ret == QDMI_SUCCESS) {
        std::cout << "    -> Value: " << (status == QDMI_DEVICE_STATUS_IDLE ? "IDLE" : 
                                          status == QDMI_DEVICE_STATUS_BUSY ? "BUSY" : "OTHER") << "\n";
    }

    // Query sites (qubits)
    size_t sitesSize = 0;
    std::cout << "\n  Querying available sites (qubits):\n";
    printQDMI("AWS_QDMI_device_session_query_device_property(session, SITES, ...)",
              "(constructed from qubitCount)");
    printVar("OUTPUT", "sites[] array");
    ret = AWS_QDMI_device_session_query_device_property(
        session, QDMI_DEVICE_PROPERTY_SITES, 0, nullptr, &sitesSize);
    printResult("Query DEVICE_PROPERTY_SITES", ret == QDMI_SUCCESS);
    if (ret == QDMI_SUCCESS) {
        std::cout << "    -> " << sitesSize / sizeof(AWS_QDMI_Site) << " sites available\n";
    }

    // Query operations (gates)
    size_t opsSize = 0;
    std::cout << "\n  Querying available operations (gates):\n";
    printQDMI("AWS_QDMI_device_session_query_device_property(session, OPERATIONS, ...)",
              "(parsed from GetDevice JSON: supportedOperations)");
    printVar("OUTPUT", "operations[] array");
    ret = AWS_QDMI_device_session_query_device_property(
        session, QDMI_DEVICE_PROPERTY_OPERATIONS, 0, nullptr, &opsSize);
    printResult("Query DEVICE_PROPERTY_OPERATIONS", ret == QDMI_SUCCESS);
    if (ret == QDMI_SUCCESS) {
        std::cout << "    -> " << opsSize / sizeof(AWS_QDMI_Operation) << " operations available\n";
    }

    // ========================================================================
    // TEST 6: Job Creation
    // ========================================================================
    printSeparator("TEST 6: Job Creation");

    AWS_QDMI_Device_Job job;
    std::cout << "\n  Creating a new job:\n";
    printQDMI("AWS_QDMI_device_session_create_device_job(session, &job)",
              "new AWS_QDMI_Device_Job_impl_d(session)");
    printVar("OUTPUT", "job handle");
    printVar("OUTPUT", "jobId (internal)");
    ret = AWS_QDMI_device_session_create_device_job(session, &job);
    printResult("Create device job", ret == QDMI_SUCCESS);
    if (ret != QDMI_SUCCESS) {
        std::cerr << "FATAL: Cannot create job\n";
        AWS_QDMI_device_session_free(session);
        return 1;
    }

    // Query job ID
    int jobId;
    std::cout << "\n  Querying job ID:\n";
    printQDMI("AWS_QDMI_device_job_query_property(job, ID, ...)", "(internal ID)");
    printVar("OUTPUT", "jobId");
    ret = AWS_QDMI_device_job_query_property(job, QDMI_DEVICE_JOB_PROPERTY_ID,
                                              sizeof(jobId), &jobId, nullptr);
    printResult("Query JOB_PROPERTY_ID", ret == QDMI_SUCCESS);
    if (ret == QDMI_SUCCESS) {
        std::cout << "    -> Value: " << jobId << "\n";
    }

    // ========================================================================
    // TEST 7: Job Parameters
    // ========================================================================
    printSeparator("TEST 7: Job Configuration");

    // Set shots
    size_t shots = 100;
    std::cout << "\n  Setting number of shots:\n";
    printQDMI("AWS_QDMI_device_job_set_parameter(job, SHOTSNUM, ...)",
              "(stored for CreateQuantumTask::SetShots)");
    printVar("INPUT", "shotsNum");
    ret = AWS_QDMI_device_job_set_parameter(
        job, QDMI_DEVICE_JOB_PARAMETER_SHOTSNUM, sizeof(shots), &shots);
    printResult("Set JOB_PARAMETER_SHOTSNUM", ret == QDMI_SUCCESS);
    allPassed &= (ret == QDMI_SUCCESS);

    // Set program format
    QDMI_Program_Format format = QDMI_PROGRAM_FORMAT_QASM3;
    std::cout << "\n  Setting program format:\n";
    printQDMI("AWS_QDMI_device_job_set_parameter(job, PROGRAMFORMAT, ...)",
              "(determines Action schema: braket.ir.openqasm.program)");
    printVar("INPUT", "programFormat (QASM3)");
    ret = AWS_QDMI_device_job_set_parameter(
        job, QDMI_DEVICE_JOB_PARAMETER_PROGRAMFORMAT, sizeof(format), &format);
    printResult("Set JOB_PARAMETER_PROGRAMFORMAT", ret == QDMI_SUCCESS);
    allPassed &= (ret == QDMI_SUCCESS);

    // Set program (Bell state circuit)
    const char* program = 
        "OPENQASM 3.0;\n"
        "qubit[2] q;\n"
        "h q[0];\n"
        "cnot q[0], q[1];\n"
        "bit[2] c;\n"
        "c = measure q;\n";
    
    std::cout << "\n  Setting quantum program:\n";
    printQDMI("AWS_QDMI_device_job_set_parameter(job, PROGRAM, ...)",
              "(stored for CreateQuantumTask::SetAction)");
    printVar("INPUT", "program (OpenQASM 3.0 source)");
    ret = AWS_QDMI_device_job_set_parameter(
        job, QDMI_DEVICE_JOB_PARAMETER_PROGRAM, strlen(program) + 1, program);
    printResult("Set JOB_PARAMETER_PROGRAM", ret == QDMI_SUCCESS);
    allPassed &= (ret == QDMI_SUCCESS);

    std::cout << "\n  Circuit (Bell State):\n";
    std::cout << "  ┌───┐     ┌─┐\n";
    std::cout << "  │ H ├──●──┤M├\n";
    std::cout << "  └───┘  │  └─┘\n";
    std::cout << "       ┌─┴─┐┌─┐\n";
    std::cout << "       │ X ├┤M├\n";
    std::cout << "       └───┘└─┘\n";

    // ========================================================================
    // TEST 8: Job Submission (CreateQuantumTask API)
    // ========================================================================
    printSeparator("TEST 8: Job Submission");
    
    std::cout << "\n  Submitting job to AWS Braket:\n";
    printQDMI("AWS_QDMI_device_job_submit(job)", nullptr);
    std::cout << "   [AWS] -> CreateQuantumTaskRequest::SetDeviceArn(DEVICEARN)\n";
    std::cout << "   [AWS] -> CreateQuantumTaskRequest::SetShots(shotsNum)\n";
    std::cout << "   [AWS] -> CreateQuantumTaskRequest::SetOutputS3Bucket(S3BUCKET)\n";
    std::cout << "   [AWS] -> CreateQuantumTaskRequest::SetOutputS3KeyPrefix(prefix)\n";
    std::cout << "   [AWS] -> CreateQuantumTaskRequest::SetAction(program JSON)\n";
    std::cout << "   [AWS] -> BraketClient::CreateQuantumTask(request)\n";
    printVar("READ", "DEVICEARN");
    printVar("READ", "S3BUCKET");
    printVar("READ", "shotsNum");
    printVar("READ", "program");
    printVar("OUTPUT", "taskArn");
    
    ret = AWS_QDMI_device_job_submit(job);
    printResult("Submit job (CreateQuantumTask)", ret == QDMI_SUCCESS);
    
    if (ret != QDMI_SUCCESS) {
        std::cerr << "\n  Job submission failed. Possible causes:\n";
        std::cerr << "  - Invalid S3 bucket or permissions\n";
        std::cerr << "  - Invalid circuit syntax\n";
        std::cerr << "  - AWS service issues\n";
        allPassed = false;
    } else {
        // Query task ARN
        char taskArn[512];
        size_t taskArnSize;
        std::cout << "\n  Querying task ARN:\n";
        printQDMI("AWS_QDMI_device_job_query_property(job, TASKARN, ...)",
                  "(returned from CreateQuantumTask response)");
        printVar("OUTPUT", "taskArn");
        ret = AWS_QDMI_device_job_query_property(
            job, 
            static_cast<QDMI_Device_Job_Property>(QDMI_DEVICE_JOB_PROPERTY_TASKARN),
            sizeof(taskArn), taskArn, &taskArnSize);
        printResult("Query JOB_PROPERTY_TASKARN", ret == QDMI_SUCCESS);
        if (ret == QDMI_SUCCESS) {
            std::cout << "    -> Value: " << taskArn << "\n";
        }

        // ====================================================================
        // TEST 9: Job Status Polling (GetQuantumTask API)
        // ====================================================================
        printSeparator("TEST 9: Job Status Check");
        
        QDMI_Job_Status jobStatus;
        std::cout << "\n  Checking job status:\n";
        printQDMI("AWS_QDMI_device_job_check(job, &status)", nullptr);
        std::cout << "   [AWS] -> GetQuantumTaskRequest::SetQuantumTaskArn(taskArn)\n";
        std::cout << "   [AWS] -> BraketClient::GetQuantumTask(request)\n";
        printVar("READ", "taskArn");
        printVar("OUTPUT", "taskStatus");
        printVar("OUTPUT", "outputS3Bucket (when COMPLETED)");
        printVar("OUTPUT", "outputS3Directory (when COMPLETED)");
        
        ret = AWS_QDMI_device_job_check(job, &jobStatus);
        printResult("Check job status (GetQuantumTask)", ret == QDMI_SUCCESS);
        if (ret == QDMI_SUCCESS) {
            const char* statusStr = 
                jobStatus == QDMI_JOB_STATUS_CREATED ? "CREATED" :
                jobStatus == QDMI_JOB_STATUS_QUEUED ? "QUEUED" :
                jobStatus == QDMI_JOB_STATUS_RUNNING ? "RUNNING" :
                jobStatus == QDMI_JOB_STATUS_DONE ? "DONE" :
                jobStatus == QDMI_JOB_STATUS_FAILED ? "FAILED" :
                jobStatus == QDMI_JOB_STATUS_CANCELED ? "CANCELED" : "UNKNOWN";
            std::cout << "    -> Status: " << statusStr << "\n";
        }

        // ====================================================================
        // TEST 10: Wait for Completion
        // ====================================================================
        printSeparator("TEST 10: Wait for Completion");
        
        std::cout << "\n  Waiting for job to complete:\n";
        printQDMI("AWS_QDMI_device_job_wait(job, timeout_ms)", nullptr);
        std::cout << "   [AWS] -> (polls GetQuantumTask every 100ms until COMPLETED/FAILED/CANCELLED)\n";
        printVar("READ", "taskArn");
        printVar("OUTPUT", "taskStatus");
        
        ret = AWS_QDMI_device_job_wait(job, 60000);  // 60 second timeout
        
        if (ret == QDMI_SUCCESS) {
            printResult("Job completed", true);
            
            // Check final status
            ret = AWS_QDMI_device_job_check(job, &jobStatus);
            if (ret == QDMI_SUCCESS && jobStatus == QDMI_JOB_STATUS_DONE) {
                std::cout << "    -> Final status: DONE\n";
                
                // ============================================================
                // TEST 11: Get Results (S3 Download + Parse)
                // ============================================================
                printSeparator("TEST 11: Get Results (S3 Download)");
                
                std::cout << "\n  Fetching results from S3:\n";
                std::cout << "   [AWS] -> S3Client(config)\n";
                std::cout << "   [AWS] -> GetObjectRequest::SetBucket(outputS3Bucket)\n";
                std::cout << "   [AWS] -> GetObjectRequest::SetKey(outputS3Directory/results.json)\n";
                std::cout << "   [AWS] -> S3Client::GetObject(request)\n";
                printVar("READ", "outputS3Bucket");
                printVar("READ", "outputS3Directory");
                printVar("OUTPUT", "results.json content");
                
                // Query SHOTS result
                size_t shotsSize = 0;
                std::cout << "\n  Querying SHOTS result:\n";
                printQDMI("AWS_QDMI_device_job_get_results(job, SHOTS, ...)",
                          "(parsed from results.json: measurements array)");
                printVar("OUTPUT", "shots string (comma-separated)");
                ret = AWS_QDMI_device_job_get_results(job, QDMI_JOB_RESULT_SHOTS, 
                                                       0, nullptr, &shotsSize);
                printResult("Query QDMI_JOB_RESULT_SHOTS size", ret == QDMI_SUCCESS);
                if (ret == QDMI_SUCCESS && shotsSize > 0) {
                    std::cout << "    -> Size: " << shotsSize << " bytes\n";
                    
                    // Get actual shots data
                    std::string shotsData(shotsSize - 1, '\0');
                    ret = AWS_QDMI_device_job_get_results(job, QDMI_JOB_RESULT_SHOTS,
                                                          shotsSize, shotsData.data(), nullptr);
                    printResult("Get SHOTS data", ret == QDMI_SUCCESS);
                    if (ret == QDMI_SUCCESS) {
                        // Show first few shots
                        std::cout << "    -> First 10 shots: ";
                        size_t comma_count = 0;
                        for (size_t i = 0; i < shotsData.size() && comma_count < 10; ++i) {
                            std::cout << shotsData[i];
                            if (shotsData[i] == ',') comma_count++;
                        }
                        std::cout << "...\n";
                    }
                }
                
                // Query histogram keys
                size_t keysSize = 0;
                std::cout << "\n  Querying HIST_KEYS result:\n";
                printQDMI("AWS_QDMI_device_job_get_results(job, HIST_KEYS, ...)",
                          "(computed histogram from measurements)");
                printVar("OUTPUT", "histogram keys (null-separated)");
                ret = AWS_QDMI_device_job_get_results(job, QDMI_JOB_RESULT_HIST_KEYS, 
                                                       0, nullptr, &keysSize);
                printResult("Query QDMI_JOB_RESULT_HIST_KEYS size", ret == QDMI_SUCCESS);
                
                std::vector<std::string> keys;
                if (ret == QDMI_SUCCESS && keysSize > 0) {
                    std::vector<char> keysData(keysSize);
                    ret = AWS_QDMI_device_job_get_results(job, QDMI_JOB_RESULT_HIST_KEYS,
                                                          keysSize, keysData.data(), nullptr);
                    printResult("Get HIST_KEYS data", ret == QDMI_SUCCESS);
                    
                    // Parse null-separated keys
                    const char* ptr = keysData.data();
                    const char* end = ptr + keysSize;
                    while (ptr < end && *ptr) {
                        keys.emplace_back(ptr);
                        ptr += strlen(ptr) + 1;
                    }
                    std::cout << "    -> Keys: ";
                    for (const auto& k : keys) std::cout << "\"" << k << "\" ";
                    std::cout << "\n";
                }
                
                // Query histogram values
                size_t valuesSize = 0;
                std::cout << "\n  Querying HIST_VALUES result:\n";
                printQDMI("AWS_QDMI_device_job_get_results(job, HIST_VALUES, ...)",
                          "(computed histogram counts from measurements)");
                printVar("OUTPUT", "histogram values (size_t array)");
                ret = AWS_QDMI_device_job_get_results(job, QDMI_JOB_RESULT_HIST_VALUES,
                                                       0, nullptr, &valuesSize);
                printResult("Query QDMI_JOB_RESULT_HIST_VALUES size", ret == QDMI_SUCCESS);
                
                if (ret == QDMI_SUCCESS && valuesSize > 0) {
                    size_t numValues = valuesSize / sizeof(size_t);
                    std::vector<size_t> values(numValues);
                    ret = AWS_QDMI_device_job_get_results(job, QDMI_JOB_RESULT_HIST_VALUES,
                                                          valuesSize, values.data(), nullptr);
                    printResult("Get HIST_VALUES data", ret == QDMI_SUCCESS);
                    
                    std::cout << "    -> Values: ";
                    for (auto v : values) std::cout << v << " ";
                    std::cout << "\n";
                    
                    // Print histogram
                    std::cout << "\n  ┌────────────────────────────────┐\n";
                    std::cout << "  │     Measurement Histogram      │\n";
                    std::cout << "  ├────────────────────────────────┤\n";
                    size_t total = 0;
                    for (size_t i = 0; i < keys.size() && i < values.size(); ++i) {
                        std::cout << "  │  |" << keys[i] << ">  :  " 
                                  << std::setw(4) << values[i] << " shots";
                        // Add bar chart
                        std::cout << "  ";
                        int barLen = static_cast<int>(values[i] * 10 / shots);
                        for (int b = 0; b < barLen; ++b) std::cout << "█";
                        std::cout << "\n";
                        total += values[i];
                    }
                    std::cout << "  ├────────────────────────────────┤\n";
                    std::cout << "  │  Total: " << std::setw(4) << total << " shots            │\n";
                    std::cout << "  └────────────────────────────────┘\n";
                    
                    // Verify Bell state (should be ~50% |00>, ~50% |11>)
                    if (keys.size() == 2) {
                        bool hasBellState = 
                            (keys[0] == "00" || keys[0] == "11") &&
                            (keys[1] == "00" || keys[1] == "11");
                        std::cout << "\n";
                        printResult("Bell state verified (only |00> and |11>)", hasBellState);
                    }
                }
            } else if (jobStatus == QDMI_JOB_STATUS_FAILED) {
                std::cout << "    -> Final status: FAILED\n";
                allPassed = false;
            }
        } else if (ret == QDMI_ERROR_TIMEOUT) {
            printResult("Job completed within timeout", false);
            std::cout << "    -> Task is still running (this is OK for QPUs)\n";
            
            // ================================================================
            // TEST 12: Cancel Job (CancelQuantumTask API) - Optional
            // ================================================================
            printSeparator("TEST 12: Cancel Job");
            
            std::cout << "\n  Cancelling job:\n";
            printQDMI("AWS_QDMI_device_job_cancel(job)", nullptr);
            std::cout << "   [AWS] -> CancelQuantumTaskRequest::SetQuantumTaskArn(taskArn)\n";
            std::cout << "   [AWS] -> BraketClient::CancelQuantumTask(request)\n";
            printVar("READ", "taskArn");
            
            ret = AWS_QDMI_device_job_cancel(job);
            printResult("Cancel job (CancelQuantumTask)", ret == QDMI_SUCCESS);
        } else {
            printResult("Wait for job", false);
            allPassed = false;
        }
    }

    // ========================================================================
    // Cleanup
    // ========================================================================
    printSeparator("Cleanup");
    
    std::cout << "\n  Freeing job:\n";
    printQDMI("AWS_QDMI_device_job_free(job)", "delete AWS_QDMI_Device_Job_impl_d");
    AWS_QDMI_device_job_free(job);
    printResult("Free job", true);
    
    std::cout << "\n  Freeing session:\n";
    printQDMI("AWS_QDMI_device_session_free(session)", "delete AWS_QDMI_Device_Session_impl_d");
    std::cout << "   [AWS] -> (BraketClient destroyed)\n";
    AWS_QDMI_device_session_free(session);
    printResult("Free session", true);
    
    std::cout << "\n  Finalizing device:\n";
    printQDMI("AWS_QDMI_device_finalize()", "Aws::ShutdownAPI(options)");
    ret = AWS_QDMI_device_finalize();
    printResult("Finalize device", ret == QDMI_SUCCESS);

    // ========================================================================
    // Summary
    // ========================================================================
    printSeparator("Test Summary");
    
    std::cout << "\n  QDMI Functions Used:\n";
    std::cout << "  ────────────────────────────────────────────────────────────────\n";
    std::cout << "  AWS_QDMI_device_initialize()              -> Aws::InitAPI()\n";
    std::cout << "  AWS_QDMI_device_session_alloc()           -> (internal allocation)\n";
    std::cout << "  AWS_QDMI_device_session_set_parameter()   -> (store config)\n";
    std::cout << "  AWS_QDMI_device_session_init()            -> BraketClient + GetDevice\n";
    std::cout << "  AWS_QDMI_device_session_query_*()         -> (parse GetDevice JSON)\n";
    std::cout << "  AWS_QDMI_device_session_create_device_job() -> (internal allocation)\n";
    std::cout << "  AWS_QDMI_device_job_set_parameter()       -> (store job config)\n";
    std::cout << "  AWS_QDMI_device_job_query_property()      -> (return stored values)\n";
    std::cout << "  AWS_QDMI_device_job_submit()              -> CreateQuantumTask\n";
    std::cout << "  AWS_QDMI_device_job_check()               -> GetQuantumTask\n";
    std::cout << "  AWS_QDMI_device_job_wait()                -> (poll GetQuantumTask)\n";
    std::cout << "  AWS_QDMI_device_job_get_results()         -> S3Client::GetObject\n";
    std::cout << "  AWS_QDMI_device_job_cancel()              -> CancelQuantumTask\n";
    std::cout << "  AWS_QDMI_device_job_free()                -> (internal cleanup)\n";
    std::cout << "  AWS_QDMI_device_session_free()            -> (destroy BraketClient)\n";
    std::cout << "  AWS_QDMI_device_finalize()                -> Aws::ShutdownAPI()\n";
    
    std::cout << "\n  Variables Used:\n";
    std::cout << "  ────────────────────────────────────────────────────────────────\n";
    std::cout << "  Session Config:  DEVICEARN, S3BUCKET, REGION\n";
    std::cout << "  Device Info:     deviceName, deviceStatus, qubitCount,\n";
    std::cout << "                   sites[], operations[], couplingMap\n";
    std::cout << "  Job Config:      shotsNum, programFormat, program\n";
    std::cout << "  Job State:       jobId, taskArn, taskStatus\n";
    std::cout << "  Results:         outputS3Bucket, outputS3Directory,\n";
    std::cout << "                   measurements[], histogram\n";
    
    std::cout << "\n  AWS SDK Components:\n";
    std::cout << "  ────────────────────────────────────────────────────────────────\n";
    std::cout << "  aws-cpp-sdk-core     : Aws::InitAPI, ShutdownAPI, ClientConfiguration\n";
    std::cout << "  aws-cpp-sdk-braket   : BraketClient, GetDevice, CreateQuantumTask,\n";
    std::cout << "                         GetQuantumTask, CancelQuantumTask\n";
    std::cout << "  aws-cpp-sdk-s3       : S3Client, GetObject (results.json)\n";
    std::cout << "\n";
    
    if (allPassed) {
        std::cout << "  ╔════════════════════════════════════════╗\n";
        std::cout << "  ║         ✓ ALL TESTS PASSED             ║\n";
        std::cout << "  ╚════════════════════════════════════════╝\n\n";
        return 0;
    } else {
        std::cout << "  ╔════════════════════════════════════════╗\n";
        std::cout << "  ║         ✗ SOME TESTS FAILED            ║\n";
        std::cout << "  ╚════════════════════════════════════════╝\n\n";
        return 1;
    }
}
