#include "aws_qdmi/device.h"
#include <cstring>
#include <iostream>
#include <aws/braket/model/SearchDevicesRequest.h>
#include <aws/braket/model/SearchDevicesFilter.h>
#include <aws/braket/BraketClient.h>
#include <aws/core/Aws.h>
#include <aws/braket/model/DeviceType.h>
#include <aws/braket/model/DeviceStatus.h>


int main() {
  // Initialize the device
  int ret = AWS_QDMI_device_initialize();
  if (ret != QDMI_SUCCESS) {
    std::cerr << "Failed to initialize device: " << ret << "\n";
    return 1;
  }

  // Allocate a session
  AWS_QDMI_Device_Session session;
  ret = AWS_QDMI_device_session_alloc(&session);
  if (ret != QDMI_SUCCESS) {
    std::cerr << "Failed to allocate session: " << ret << "\n";
    return 1;
  }

  // Set session parameters (optional - configure AWS region and device)
  const char* region = "us-east-1";
  ret = AWS_QDMI_device_session_set_parameter(
      session, static_cast<QDMI_Device_Session_Parameter>(QDMI_DEVICE_SESSION_PARAMETER_REGION), 
      strlen(region) + 1, region);
  if (ret != QDMI_SUCCESS) {
    std::cerr << "Failed to set region: " << ret << "\n";
  }

  // Not part of QDMI, but useful for debugging, search for available devices:
  {
      Aws::Client::ClientConfiguration config;
      config.region = region;
      Aws::Braket::BraketClient client(config);

      Aws::Braket::Model::SearchDevicesRequest searchRequest;
      Aws::Braket::Model::SearchDevicesFilter filter;
      filter.SetName("deviceArn");
      const char* targetDeviceArn = "arn:aws:braket:::device/quantum-simulator/amazon/sv1";
      filter.SetValues({targetDeviceArn}); 
      searchRequest.AddFilters(filter);

      std::cout << "DEBUG: Searching for devices..." << std::endl;
      auto searchOutcome = client.SearchDevices(searchRequest);
      if (searchOutcome.IsSuccess()) {
          std::cout << "DEBUG: Found devices:" << std::endl;
          for (const auto& device : searchOutcome.GetResult().GetDevices()) {
              std::cout << "  - " << device.GetDeviceName() 
                        << " (" << Aws::Braket::Model::DeviceTypeMapper::GetNameForDeviceType(device.GetDeviceType()) << ")"
                        << " [" << Aws::Braket::Model::DeviceStatusMapper::GetNameForDeviceStatus(device.GetDeviceStatus()) << "]"
                        << "\n    ARN: " << device.GetDeviceArn() << std::endl;
          }
      } else {
          std::cerr << "DEBUG: Failed to search devices: " 
                    << searchOutcome.GetError().GetMessage() << std::endl;
      }
  }

  // Check if simulator is available
  const char* deviceArn = "arn:aws:braket:::device/quantum-simulator/amazon/sv1";
  ret = AWS_QDMI_device_session_set_parameter(
      session, static_cast<QDMI_Device_Session_Parameter>(QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN), 
      strlen(deviceArn) + 1, deviceArn);
  if (ret != QDMI_SUCCESS) {
    std::cerr << "Failed to set device ARN: " << ret << "\n";
  }

  // Initialize the session
  ret = AWS_QDMI_device_session_init(session);
  if (ret != QDMI_SUCCESS) {
    std::cerr << "Failed to initialize session: " << ret << "\n";
    AWS_QDMI_device_session_free(session);
    return 1;
  }

  // Query device properties
  char name[256];
  size_t nameSize;
  ret = AWS_QDMI_device_session_query_device_property(
      session, QDMI_DEVICE_PROPERTY_NAME, sizeof(name), name, &nameSize);
  if (ret == QDMI_SUCCESS) {
    std::cout << "Device name: " << name << "\n";
  }

  size_t qubitsNum;
  ret = AWS_QDMI_device_session_query_device_property(
      session, QDMI_DEVICE_PROPERTY_QUBITSNUM, sizeof(qubitsNum), &qubitsNum,
      nullptr);
  if (ret == QDMI_SUCCESS) {
    std::cout << "Number of qubits: " << qubitsNum << "\n";
  }

  // Create a job (example - would need actual open QASM program)
  AWS_QDMI_Device_Job job;
  ret = AWS_QDMI_device_session_create_device_job(session, &job);
  if (ret != QDMI_SUCCESS) {
    std::cerr << "Failed to create job: " << ret << "\n";
  } else {
    std::cout << "Job created successfully\n";

    // Query job ID
    int jobId;
    ret = AWS_QDMI_device_job_query_property(job, QDMI_DEVICE_JOB_PROPERTY_ID,
                                              sizeof(jobId), &jobId, nullptr);
    if (ret == QDMI_SUCCESS) {
      std::cout << "Job ID: " << jobId << "\n";
    }

    // Set job parameters
    size_t shots = 1000;
    ret = AWS_QDMI_device_job_set_parameter(
        job, QDMI_DEVICE_JOB_PARAMETER_SHOTSNUM, sizeof(shots), &shots);
    if (ret == QDMI_SUCCESS) {
      std::cout << "Set shots to " << shots << "\n";
    }

    // Set program format
    QDMI_Program_Format format = QDMI_PROGRAM_FORMAT_QASM3;
    ret = AWS_QDMI_device_job_set_parameter(
        job, QDMI_DEVICE_JOB_PARAMETER_PROGRAMFORMAT, sizeof(format), &format);
    if (ret == QDMI_SUCCESS) {
      std::cout << "Set program format to QASM3\n";
    }

    // Note: In a real scenario, you would set circuit data here
    // const char* program = "OPENQASM 3.0; qubit[2] q; h q[0]; cx q[0], q[1]; "
    //                       "measure q;";
    // AWS_QDMI_device_job_set_parameter(job, QDMI_DEVICE_JOB_PARAMETER_PROGRAM,
    //                                   strlen(program) + 1, program);

    // Submit would happen here:
    // ret = AWS_QDMI_device_job_submit(job);
    // if (ret == QDMI_SUCCESS) {
    //   std::cout << "Job submitted\n";
    //   QDMI_Job_Status status;
    //   ret = AWS_QDMI_device_job_wait(job, 0);  // Wait indefinitely
    //   if (ret == QDMI_SUCCESS) {
    //     ret = AWS_QDMI_device_job_check(job, &status);
    //     std::cout << "Final status: " << status << "\n";
    //   }
    // }

    AWS_QDMI_device_job_free(job);
  }

  // Cleanup
  AWS_QDMI_device_session_free(session);
  AWS_QDMI_device_finalize();

  std::cout << "AWS QDMI device example completed\n";
  return 0;
}

