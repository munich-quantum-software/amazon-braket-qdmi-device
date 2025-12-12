/*
 * Copyright (c) 2023 - 2025 Chair for Design Automation, TUM
 * Copyright (c) 2025 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
 */

/** @file
 * @brief QDMI device implementation for AWS Braket.
 * 
 * This file implements the Quantum Device Management Interface (QDMI) specification
 * for AWS Braket, Amazon's quantum computing service.
 * 
 * QDMI Project: https://github.com/Munich-Quantum-Software-Stack/QDMI
 * 
 * ============================================================================
 * Purpose: QDMI Adapter for AWS Braket
 * ============================================================================
 * 
 * You can target AWS Braket devices by simply linking against this library
 * instead of another QDMI implementation. Your OpenQASM circuits will
 * execute on AWS Braket simulators or real quantum hardware.
 * 
 * ============================================================================
 * QDMI to AWS Braket Mapping
 * ============================================================================
 * 
 * This implementation translates QDMI standard calls into AWS Braket SDK calls:
 * 
 * QDMI Concept          | AWS Braket Equivalent
 * ----------------------|--------------------------------------------------
 * Device                | BraketClient + GetDeviceRequest/Result
 * Device Status         | DeviceStatus enum (ONLINE, OFFLINE, RETIRED)
 * Session               | BraketClient instance with credentials
 * 
 * Job                   | QuantumTask
 * Job Status            | QuantumTaskStatus (CREATED, QUEUED, RUNNING, etc.)
 * Job Submission        | BraketClient::CreateQuantumTask()
 * Job Cancellation      | BraketClient::CancelQuantumTask()
 * 
 * Program               | Action field (OpenQASM string wrapped in JSON)
 * Shots                 | CreateQuantumTaskRequest::SetShots()
 * 
 * Site (Qubit)          | Parsed from DeviceCapabilities JSON (qubitCount)
 * Operation (Gate)      | Parsed from nativeGateSet / supportedOperations
 * Coupling Map          | Parsed from connectivity graph
 * 
 */

#include "aws_qdmi_device_impl.hpp"

#include <aws/braket/BraketClient.h>
#include <aws/braket/model/CancelQuantumTaskRequest.h>
#include <aws/braket/model/CreateQuantumTaskRequest.h>
#include <aws/braket/model/GetDeviceRequest.h>
#include <aws/braket/model/GetQuantumTaskRequest.h>
#include <aws/braket/model/SearchDevicesRequest.h>
#include <aws/braket/model/SearchDevicesFilter.h>
#include <aws/core/Aws.h>
#include <aws/core/client/ClientConfiguration.h>
#include <aws/core/utils/json/JsonSerializer.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/GetObjectRequest.h>

#include <chrono>
#include <cstring>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>

#define ADD_SINGLE_VALUE_PROPERTY(prop_name, prop_type, prop_value, prop,     \
                                  size, value, size_ret)                       \
  {                                                                            \
    if ((prop) == (prop_name)) {                                               \
      if ((value) != nullptr) {                                                \
        if ((size) < sizeof(prop_type)) {                                      \
          return QDMI_ERROR_INVALIDARGUMENT;                                   \
        }                                                                      \
        *static_cast<prop_type*>(value) = prop_value;                          \
      }                                                                        \
      if ((size_ret) != nullptr) {                                             \
        *size_ret = sizeof(prop_type);                                         \
      }                                                                        \
      return QDMI_SUCCESS;                                                     \
    }                                                                          \
  }

#ifdef _WIN32
#define STRNCPY(dest, src, size)                                               \
  strncpy_s(static_cast<char*>(dest), size, src, size);
#else
#define STRNCPY(dest, src, size) strncpy(static_cast<char*>(dest), src, size);
#endif

#define ADD_STRING_PROPERTY(prop_name, prop_value, prop, size, value,         \
                            size_ret)                                          \
  {                                                                            \
    if ((prop) == (prop_name)) {                                               \
      if ((value) != nullptr) {                                                \
        if ((size) < strlen(prop_value) + 1) {                                 \
          return QDMI_ERROR_INVALIDARGUMENT;                                   \
        }                                                                      \
        STRNCPY(value, prop_value, size);                                      \
        static_cast<char*>(value)[size - 1] = '\0';                            \
      }                                                                        \
      if ((size_ret) != nullptr) {                                             \
        *size_ret = strlen(prop_value) + 1;                                    \
      }                                                                        \
      return QDMI_SUCCESS;                                                     \
    }                                                                          \
  }

#define ADD_LIST_PROPERTY(prop_name, prop_type, prop_values, prop, size,      \
                          value, size_ret)                                     \
  {                                                                            \
    if ((prop) == (prop_name)) {                                               \
      if ((value) != nullptr) {                                                \
        if ((size) < (prop_values).size() * sizeof(prop_type)) {              \
          return QDMI_ERROR_INVALIDARGUMENT;                                   \
        }                                                                      \
        memcpy(static_cast<void*>(value),                                      \
               static_cast<const void*>((prop_values).data()),                 \
               (prop_values).size() * sizeof(prop_type));                      \
      }                                                                        \
      if ((size_ret) != nullptr) {                                             \
        *size_ret = (prop_values).size() * sizeof(prop_type);                 \
      }                                                                        \
      return QDMI_SUCCESS;                                                     \
    }                                                                          \
  }

namespace aws_qdmi {

/**
 * Device constructor - initializes the global device singleton.
 * 
 * This singleton manages all device sessions and provides device-level properties.
 * In QDMI, the device represents the physical or simulated quantum processor,
 * while sessions represent individual connections that can submit jobs.
 * 
 * The device maintains:
 * - Global state (idle/busy based on running jobs)
 * - Session registry for lifecycle management
 * - Random number generation for unique job IDs
 */
Device::Device() {
  name_ = "AWS Braket Device";
  provider_ = "AWS";  // AWS is the cloud provider
  deviceArn_ = "";    // ARN (Amazon Resource Name) identifies the specific quantum device
  deviceType_ = "QPU"; // Quantum Processing Unit
  qubitsNum_ = 0;      // Will be populated when device capabilities are queried
  status_.store(QDMI_DEVICE_STATUS_IDLE);
}

auto Device::sessionAlloc(AWS_QDMI_Device_Session* session) -> QDMI_STATUS {
  if (session == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  auto uniqueSession = std::make_unique<AWS_QDMI_Device_Session_impl_d>();
  const std::scoped_lock<std::mutex> lock(sessionsMutex_);
  const auto& it =
      sessions_.emplace(uniqueSession.get(), std::move(uniqueSession)).first;
  *session = it->first;
  return QDMI_SUCCESS;
}

auto Device::sessionFree(AWS_QDMI_Device_Session session) -> void {
  if (session != nullptr) {
    const std::scoped_lock<std::mutex> lock(sessionsMutex_);
    if (const auto& it = sessions_.find(session); it != sessions_.end()) {
      sessions_.erase(it);
    }
  }
}

auto Device::queryProperty(const QDMI_Device_Property prop, const size_t size,
                           void* value, size_t* sizeRet) const -> QDMI_STATUS {
  // Validate arguments and reject MAX sentinel value
  // Note: We allow AWS-specific extensions (100+) so don't check < MAX
  if ((value != nullptr && size == 0) || prop == QDMI_DEVICE_PROPERTY_MAX) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }

  ADD_STRING_PROPERTY(QDMI_DEVICE_PROPERTY_NAME, name_.c_str(), prop, size,
                      value, sizeRet)
  ADD_STRING_PROPERTY(QDMI_DEVICE_PROPERTY_VERSION, "1.0.0", prop, size, value,
                      sizeRet)
  ADD_STRING_PROPERTY(QDMI_DEVICE_PROPERTY_LIBRARYVERSION, AWS_QDMI_VERSION,
                      prop, size, value, sizeRet)
  ADD_SINGLE_VALUE_PROPERTY(QDMI_DEVICE_PROPERTY_STATUS, QDMI_Device_Status,
                            status_.load(), prop, size, value, sizeRet)
  ADD_SINGLE_VALUE_PROPERTY(QDMI_DEVICE_PROPERTY_QUBITSNUM, size_t, qubitsNum_,
                            prop, size, value, sizeRet)
  ADD_SINGLE_VALUE_PROPERTY(QDMI_DEVICE_PROPERTY_NEEDSCALIBRATION, size_t, 0,
                            prop, size, value, sizeRet)
  ADD_STRING_PROPERTY(QDMI_DEVICE_PROPERTY_PROVIDER, provider_.c_str(), prop,
                      size, value, sizeRet)
  ADD_STRING_PROPERTY(QDMI_DEVICE_PROPERTY_DEVICEARN, deviceArn_.c_str(), prop,
                      size, value, sizeRet)
  ADD_STRING_PROPERTY(QDMI_DEVICE_PROPERTY_DEVICETYPE, deviceType_.c_str(),
                      prop, size, value, sizeRet)

  return QDMI_ERROR_NOTSUPPORTED;
}

auto Device::generateUniqueID() -> int {
  const std::scoped_lock<std::mutex> lock(rngMutex_);
  return dis_(rng_);
}

auto Device::setStatus(const QDMI_Device_Status status) -> void {
  status_.store(status);
}

auto Device::increaseRunningJobs() -> void {
  if (const auto prev = runningJobs_.fetch_add(1); prev == 0) {
    setStatus(QDMI_DEVICE_STATUS_BUSY);
  }
}

auto Device::decreaseRunningJobs() -> void {
  if (const auto prev = runningJobs_.fetch_sub(1); prev == 1) {
    setStatus(QDMI_DEVICE_STATUS_IDLE);
  }
}

} // namespace aws_qdmi

// ============================================================================
// Session Implementation
// ============================================================================

/**
 * Fetches the quantum device architecture from AWS Braket.
 * 
 * This function queries the device capabilities to understand:
 * - Number of qubits (sites)
 * - Qubit connectivity (which qubits can interact)
 * - Available quantum gates/operations
 * - Coherence times (T1, T2) for each qubit
 * - Gate fidelities
 * 
 * AWS Braket returns device capabilities as JSON containing:
 * - paradigm.qubitCount: Number of qubits
 * - paradigm.connectivity: Graph of qubit connections
 * - paradigm.nativeGateSet: List of supported quantum gates
 * - provider: Hardware specifications
 * 
 * @return QDMI_SUCCESS on successful fetch, error code otherwise
 */
auto AWS_QDMI_Device_Session_impl_d::fetchDeviceArchitecture() -> QDMI_STATUS {
  if (deviceArn_.empty() || client_ == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }

  // ============================================================================
  // AWS Braket GetDevice API Call
  // ============================================================================
  Aws::Braket::Model::GetDeviceRequest request;
  request.SetDeviceArn(deviceArn_.c_str());
  
  auto outcome = client_->GetDevice(request);
  if (!outcome.IsSuccess()) {
    std::cerr << "Failed to get device: " << outcome.GetError().GetMessage() << "\n";
    std::cerr << "Please ensure you have configured your AWS credentials correctly.\n";
    std::cerr << "You can set them via environment variables (AWS_ACCESS_KEY_ID, AWS_SECRET_ACCESS_KEY) or in ~/.aws/credentials.\n";
    return QDMI_ERROR_NOTSUPPORTED;
  }

  const auto& device = outcome.GetResult();
  const auto& capabilitiesStr = device.GetDeviceCapabilities(); // Returns JSON string
  
  Aws::Utils::Json::JsonValue json(capabilitiesStr);
  if (!json.WasParseSuccessful()) {
      std::cerr << "Failed to parse device capabilities JSON\n";
      return QDMI_ERROR_FATAL;
  }

  auto view = json.View();
  if (!view.ValueExists("paradigm")) {
      return QDMI_ERROR_FATAL;
  }
  auto paradigm = view.GetObject("paradigm");
  
  if (!paradigm.ValueExists("qubitCount")) {
      return QDMI_ERROR_FATAL;
  }
  // 1. Parse Qubit Count
  qubitsNum_ = paradigm.GetInteger("qubitCount");
  
  sites_.clear();
  sites_ptr_.clear();
  sites_map_.clear();
  
  for (size_t i = 0; i < qubitsNum_; ++i) {
    auto site = std::make_unique<AWS_QDMI_Site_impl_d>();
    site->id_ = i;
    site->name_ = "Q" + std::to_string(i);
    // Default values, ideally parsed from "provider" section if available
    site->t1_ = 50000; 
    site->t2_ = 30000; 
    
    sites_ptr_.push_back(site.get());
    sites_map_[site->name_] = site.get();
    sites_.push_back(std::move(site));
  }
  
  // Optional: Parse Provider Properties (T1, T2)
  // Strategy: Check multiple locations based on vendor schemas (IonQ, IQM, AQT, Standardized)
  
  // 1. Try Standardized Properties (e.g. IBEX-Q1)
  if (view.ValueExists("standardized")) {
      auto standardized = view.GetObject("standardized");
      if (standardized.ValueExists("T1")) {
          auto t1Obj = standardized.GetObject("T1");
          if (t1Obj.ValueExists("value")) {
              uint64_t t1Val = static_cast<uint64_t>(t1Obj.GetDouble("value") * 1e6); // Convert s to us
              for (auto* site : sites_ptr_) site->t1_ = t1Val;
          }
      }
      if (standardized.ValueExists("T2")) {
          auto t2Obj = standardized.GetObject("T2");
          if (t2Obj.ValueExists("value")) {
              uint64_t t2Val = static_cast<uint64_t>(t2Obj.GetDouble("value") * 1e6); // Convert s to us
              for (auto* site : sites_ptr_) site->t2_ = t2Val;
          }
      }
  }

  // 2. Try Provider Properties (Vendor Specific)
  if (view.ValueExists("provider")) {
      auto provider = view.GetObject("provider");
      
      // IQM Schema: provider.properties.one_qubit.<id>.T1
      if (provider.ValueExists("properties")) {
          auto props = provider.GetObject("properties");
          
          // IQM Style
          if (props.ValueExists("one_qubit")) {
              auto oneQubit = props.GetObject("one_qubit");
              auto oneQubitMap = oneQubit.GetAllObjects();
              for (const auto& [qubitIdxStr, qProps] : oneQubitMap) {
                  try {
                      size_t idx = std::stoi(qubitIdxStr);
                      if (idx < sites_ptr_.size()) {
                          auto qPropsObj = qProps;
                          if (qPropsObj.ValueExists("T1")) {
                              sites_ptr_[idx]->t1_ = static_cast<uint64_t>(qPropsObj.GetDouble("T1") * 1e6);
                          }
                          if (qPropsObj.ValueExists("T2")) {
                              sites_ptr_[idx]->t2_ = static_cast<uint64_t>(qPropsObj.GetDouble("T2") * 1e6);
                          }
                      }
                  } catch (...) {}
              }
          }
          
          // AQT Style: provider.properties.t1_s.value (Global)
          if (props.ValueExists("t1_s")) {
              auto t1Obj = props.GetObject("t1_s");
              if (t1Obj.ValueExists("value")) {
                  uint64_t t1Val = static_cast<uint64_t>(t1Obj.GetDouble("value") * 1e6);
                  for (auto* site : sites_ptr_) site->t1_ = t1Val;
              }
          }
          if (props.ValueExists("t2_coherence_time_s")) {
              auto t2Obj = props.GetObject("t2_coherence_time_s");
              if (t2Obj.ValueExists("value")) {
                  uint64_t t2Val = static_cast<uint64_t>(t2Obj.GetDouble("value") * 1e6);
                  for (auto* site : sites_ptr_) site->t2_ = t2Val;
              }
          }
      }
      
      // IonQ Schema: provider.timing.T1 (Global)
      if (provider.ValueExists("timing")) {
          auto timing = provider.GetObject("timing");
          if (timing.ValueExists("T1")) {
              uint64_t t1Val = static_cast<uint64_t>(timing.GetDouble("T1") * 1e6);
              for (auto* site : sites_ptr_) site->t1_ = t1Val;
          }
          if (timing.ValueExists("T2")) {
              uint64_t t2Val = static_cast<uint64_t>(timing.GetDouble("T2") * 1e6);
              for (auto* site : sites_ptr_) site->t2_ = t2Val;
          }
      }
      
      // Generic Schema: provider.1Q.<id>.T1
      if (provider.ValueExists("1Q")) {
          auto oneQ = provider.GetObject("1Q");
          auto oneQMap = oneQ.GetAllObjects();
          for (const auto& [qubitIdxStr, props] : oneQMap) {
              try {
                  size_t idx = std::stoi(qubitIdxStr);
                  if (idx < sites_ptr_.size()) {
                      auto propsObj = props; 
                      if (propsObj.ValueExists("T1")) {
                          sites_ptr_[idx]->t1_ = static_cast<uint64_t>(propsObj.GetDouble("T1")); // Usually us
                      }
                      if (propsObj.ValueExists("T2")) {
                          sites_ptr_[idx]->t2_ = static_cast<uint64_t>(propsObj.GetDouble("T2")); // Usually us
                      }
                  }
              } catch (...) {}
          }
      }
  }
  
  // 2. Parse Connectivity
  connectivity_.clear();
  auto connectivityObj = paradigm.GetObject("connectivity");
  if (connectivityObj.GetBool("fullyConnected")) {
      for (size_t i = 0; i < qubitsNum_; ++i) {
          for (size_t j = i + 1; j < qubitsNum_; ++j) {
              connectivity_.emplace_back(sites_ptr_[i], sites_ptr_[j]);
              connectivity_.emplace_back(sites_ptr_[j], sites_ptr_[i]);
          }
      }
  } else {
      auto graph = connectivityObj.GetObject("connectivityGraph");
      auto map = graph.GetAllObjects();
      for (const auto& [sourceStr, targets] : map) {
          size_t sourceIdx = std::stoi(sourceStr);
          if (sourceIdx >= qubitsNum_) continue;
          
          auto targetArray = targets.AsArray();
          for (size_t k = 0; k < targetArray.GetLength(); ++k) {
              size_t targetIdx = std::stoi(targetArray[k].AsString().c_str());
              if (targetIdx >= qubitsNum_) continue;
              
              connectivity_.emplace_back(sites_ptr_[sourceIdx], sites_ptr_[targetIdx]);
          }
      }
  }
  
  // 3. Parse Native Gate Set
  operations_.clear();
  operations_ptr_.clear();
  operations_map_.clear();
  
  Aws::Utils::Array<Aws::Utils::Json::JsonView> gateSet;
  bool gateSetFound = false;

  if (paradigm.ValueExists("nativeGateSet")) {
      gateSet = paradigm.GetArray("nativeGateSet");
      gateSetFound = true;
  } else if (view.ValueExists("action")) {
      auto action = view.GetObject("action");
      if (action.ValueExists("braket.ir.openqasm.program")) {
          auto openqasm = action.GetObject("braket.ir.openqasm.program");
          // TODO: What's the difference between nativeGateSet and supportedOperations?
          if (openqasm.ValueExists("supportedOperations")) {
              gateSet = openqasm.GetArray("supportedOperations");
              gateSetFound = true;
          }
      }
  }

  if (gateSetFound) {
      for (size_t i = 0; i < gateSet.GetLength(); ++i) {
          std::string gateName = gateSet[i].AsString().c_str();
      
          auto op = std::make_unique<AWS_QDMI_Operation_impl_d>();
          op->name_ = gateName;
      
          // Heuristic for qubit count based on name
          // Covers standard gates and native gates from IonQ (GPI, MS), IQM (prx, measure), AQT (r, xx)
          if (gateName == "cnot" || gateName == "cz" || gateName == "swap" || 
              gateName == "xx" || gateName == "yy" || gateName == "zz" || 
              gateName == "xy" || gateName == "cp" || gateName == "iswap" || 
              gateName == "pswap" || gateName == "ecr" || gateName == "cy" ||
              gateName == "MS" || gateName == "cc_prx") { // MS is IonQ 2-qubit, cc_prx is IQM 2-qubit? No, cc_prx is likely 2 or 3.
              // Correction: cc_prx in IQM Garnet is listed. Usually cc implies controlled-controlled (3).
              // But let's check if it's in the 2-qubit list.
              // For safety, let's assume 2 for now unless we know it's 3 (ccnot, cswap).
              op->num_qubits_ = 2;
          } else if (gateName == "ccnot" || gateName == "cswap") {
              op->num_qubits_ = 3;
          } else {
              // Default to 1 qubit (x, y, z, h, rx, ry, rz, s, t, v, prx, GPI, GPI2, measure_ff)
              op->num_qubits_ = 1;
          }
          
          // Heuristic for params
          if (gateName == "rx" || gateName == "ry" || gateName == "rz" || 
              gateName == "phaseshift" || gateName == "cphaseshift" || 
              gateName == "xy" || gateName == "xx" || gateName == "yy" || gateName == "zz" ||
              gateName == "GPI" || gateName == "GPI2") {
              op->num_params_ = 1;
          } else if (gateName == "prx" || gateName == "cc_prx") {
              op->num_params_ = 2; // Usually angle and phase
          } else if (gateName == "MS") {
              op->num_params_ = 3; // IonQ MS gate has 3 phases
          } else {
              op->num_params_ = 0;
          }

          op->fidelity_ = 0.999; // Default, could parse from provider properties
          
          operations_ptr_.push_back(op.get());
          operations_map_[gateName] = op.get();
          operations_.push_back(std::move(op));
      }
  } else {
      std::cerr << "Warning: Could not find nativeGateSet or supportedOperations in device capabilities." << std::endl;
  }
  
  return QDMI_SUCCESS;
}

/**
 * Initialize a device session.
 * 
 * Session initialization involves:
 * 1. Setting up the AWS SDK client with proper region configuration
 * 2. Fetching device capabilities (topology, gates, etc.)
 * 3. Transitioning the session to INITIALIZED state
 * 
 * A session must be initialized before it can create and submit jobs.
 * 
 * AWS Regions: AWS Braket is available in specific regions (us-east-1, us-west-2, etc.)
 */
auto AWS_QDMI_Device_Session_impl_d::init() -> QDMI_STATUS {
  if (status_ != Status::ALLOCATED) {
    return QDMI_ERROR_BADSTATE;
  }

  // Extract region from ARN if not explicitly set
  // ARN format: arn:aws:braket:<region>::device/... or arn:aws:braket:::<device> (global)
  if (region_.empty() && !deviceArn_.empty()) {
    // Parse ARN: arn:aws:braket:REGION::device/...
    size_t start = deviceArn_.find("braket:");
    if (start != std::string::npos) {
      start += 7; // Skip "braket:"
      size_t end = deviceArn_.find(':', start);
      if (end != std::string::npos && end > start) {
        region_ = deviceArn_.substr(start, end - start);
      }
    }
    // If region is still empty (global ARN like simulators), default to us-east-1
    if (region_.empty()) {
      region_ = "us-east-1";
    }
  }

  // Configure AWS client with region
  Aws::Client::ClientConfiguration config;
  if (!region_.empty()) {
    config.region = region_.c_str();
  }
  client_ = std::make_unique<Aws::Braket::BraketClient>(config);

  status_ = Status::INITIALIZED;
  
  // Always fetch device architecture
  const auto ret = fetchDeviceArchitecture();
  if (ret != QDMI_SUCCESS) {
    return ret;
  }
  
  return QDMI_SUCCESS;
}

auto AWS_QDMI_Device_Session_impl_d::setParameter(
    const QDMI_Device_Session_Parameter param, const size_t size,
    const void* value) -> QDMI_STATUS {
  // Check for invalid arguments
  if (value != nullptr && size == 0) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  
  // Validate parameter: must be standard QDMI param, CUSTOM param, or AWS-specific param
  const bool isStandardParam = param < QDMI_DEVICE_SESSION_PARAMETER_MAX;
  const bool isCustomParam = (param == QDMI_DEVICE_SESSION_PARAMETER_CUSTOM1 ||
                              param == QDMI_DEVICE_SESSION_PARAMETER_CUSTOM2 ||
                              param == QDMI_DEVICE_SESSION_PARAMETER_CUSTOM3 ||
                              param == QDMI_DEVICE_SESSION_PARAMETER_CUSTOM4 ||
                              param == QDMI_DEVICE_SESSION_PARAMETER_CUSTOM5);
  const bool isAWSParam = (param == QDMI_DEVICE_SESSION_PARAMETER_REGION ||
                           param == QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN ||
                           param == QDMI_DEVICE_SESSION_PARAMETER_S3BUCKET);
  
  if (!isStandardParam && !isCustomParam && !isAWSParam) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  
  if (status_ != Status::ALLOCATED) {
    return QDMI_ERROR_BADSTATE;
  }

  if (param == QDMI_DEVICE_SESSION_PARAMETER_REGION) {
    region_ = std::string(static_cast<const char*>(value), size - 1);
    return QDMI_SUCCESS;
  }
  if (param == QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN) {
    deviceArn_ = std::string(static_cast<const char*>(value), size - 1);
    return QDMI_SUCCESS;
  }
  if (param == QDMI_DEVICE_SESSION_PARAMETER_S3BUCKET) {
    s3Bucket_ = std::string(static_cast<const char*>(value), size - 1);
    return QDMI_SUCCESS;
  }

  return QDMI_ERROR_NOTSUPPORTED;
}

auto AWS_QDMI_Device_Session_impl_d::createDeviceJob(
    AWS_QDMI_Device_Job* job) -> QDMI_STATUS {
  if (job == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  if (status_ == Status::ALLOCATED) {
    return QDMI_ERROR_BADSTATE;
  }

  auto uniqueJob = std::make_unique<AWS_QDMI_Device_Job_impl_d>(this);
  const std::scoped_lock<std::mutex> lock(jobsMutex_);
  *job = jobs_.emplace(uniqueJob.get(), std::move(uniqueJob)).first->first;
  return QDMI_SUCCESS;
}

auto AWS_QDMI_Device_Session_impl_d::freeDeviceJob(
    AWS_QDMI_Device_Job job) -> void {
  if (job != nullptr) {
    const std::scoped_lock<std::mutex> lock(jobsMutex_);
    jobs_.erase(job);
  }
}

auto AWS_QDMI_Device_Session_impl_d::queryDeviceProperty(
    const QDMI_Device_Property prop, const size_t size, void* value,
    size_t* sizeRet) const -> QDMI_STATUS {
  if (status_ != Status::INITIALIZED) {
    return QDMI_ERROR_BADSTATE;
  }
  
  // Session-specific properties
  ADD_LIST_PROPERTY(QDMI_DEVICE_PROPERTY_SITES, AWS_QDMI_Site, sites_ptr_,
                    prop, size, value, sizeRet)
  ADD_LIST_PROPERTY(QDMI_DEVICE_PROPERTY_OPERATIONS, AWS_QDMI_Operation,
                    operations_ptr_, prop, size, value, sizeRet)
  ADD_LIST_PROPERTY(QDMI_DEVICE_PROPERTY_COUPLINGMAP, AWS_QDMI_Site,
                    connectivity_, prop, size, value, sizeRet)
  ADD_SINGLE_VALUE_PROPERTY(QDMI_DEVICE_PROPERTY_QUBITSNUM, size_t, qubitsNum_,
                            prop, size, value, sizeRet)
  ADD_STRING_PROPERTY(QDMI_DEVICE_PROPERTY_DURATIONUNIT, "us", prop, size,
                      value, sizeRet)
  ADD_SINGLE_VALUE_PROPERTY(QDMI_DEVICE_PROPERTY_DURATIONSCALEFACTOR, double,
                            1.0, prop, size, value, sizeRet)
  
  // Delegate to device singleton for other properties
  return aws_qdmi::Device::get().queryProperty(prop, size, value, sizeRet);
}

auto AWS_QDMI_Device_Session_impl_d::querySiteProperty(
    AWS_QDMI_Site_impl_d* site, const QDMI_Site_Property prop,
    const size_t size, void* value, size_t* sizeRet) const -> QDMI_STATUS {
  if (site == nullptr || (value != nullptr && size == 0) ||
      prop >= QDMI_SITE_PROPERTY_MAX) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  
  ADD_SINGLE_VALUE_PROPERTY(QDMI_SITE_PROPERTY_INDEX, uint64_t, site->id_,
                            prop, size, value, sizeRet)
  ADD_STRING_PROPERTY(QDMI_SITE_PROPERTY_NAME, site->name_.c_str(), prop, size,
                      value, sizeRet)
  if (site->t1_ > 0) {
    ADD_SINGLE_VALUE_PROPERTY(QDMI_SITE_PROPERTY_T1, uint64_t, site->t1_,
                              prop, size, value, sizeRet)
  }
  if (site->t2_ > 0) {
    ADD_SINGLE_VALUE_PROPERTY(QDMI_SITE_PROPERTY_T2, uint64_t, site->t2_,
                              prop, size, value, sizeRet)
  }
  
  return QDMI_ERROR_NOTSUPPORTED;
}

auto AWS_QDMI_Device_Session_impl_d::queryOperationProperty(
    AWS_QDMI_Operation_impl_d* operation, const size_t num_sites,
    const AWS_QDMI_Site_impl_d* const* sites, const size_t num_params,
    const double* params, const QDMI_Operation_Property prop,
    const size_t size, void* value, size_t* sizeRet) const -> QDMI_STATUS {
  if (operation == nullptr || (value != nullptr && size == 0) ||
      (sites != nullptr && num_sites == 0) ||
      (params != nullptr && num_params == 0) ||
      prop >= QDMI_OPERATION_PROPERTY_MAX) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  
  ADD_STRING_PROPERTY(QDMI_OPERATION_PROPERTY_NAME, operation->name_.c_str(),
                      prop, size, value, sizeRet)
  ADD_SINGLE_VALUE_PROPERTY(QDMI_OPERATION_PROPERTY_QUBITSNUM, size_t,
                            operation->num_qubits_, prop, size, value, sizeRet)
  ADD_SINGLE_VALUE_PROPERTY(QDMI_OPERATION_PROPERTY_PARAMETERSNUM, size_t,
                            operation->num_params_, prop, size, value, sizeRet)
  ADD_SINGLE_VALUE_PROPERTY(QDMI_OPERATION_PROPERTY_FIDELITY, double,
                            operation->fidelity_, prop, size, value, sizeRet)
  
  return QDMI_ERROR_NOTSUPPORTED;
}

// Job implementation
auto AWS_QDMI_Device_Job_impl_d::free() -> void { session_->freeDeviceJob(this); }

auto AWS_QDMI_Device_Job_impl_d::setParameter(
    const QDMI_Device_Job_Parameter param, const size_t size,
    const void* value) -> QDMI_STATUS {
  if ((value != nullptr && size == 0) ||
      (param >= QDMI_DEVICE_JOB_PARAMETER_MAX &&
       param != QDMI_DEVICE_JOB_PARAMETER_CUSTOM1 &&
       param != QDMI_DEVICE_JOB_PARAMETER_CUSTOM2 &&
       param != QDMI_DEVICE_JOB_PARAMETER_CUSTOM3 &&
       param != QDMI_DEVICE_JOB_PARAMETER_CUSTOM4 &&
       param != QDMI_DEVICE_JOB_PARAMETER_CUSTOM5)) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }

  if (param == QDMI_DEVICE_JOB_PARAMETER_SHOTSNUM) {
    if (size != sizeof(size_t)) {
      return QDMI_ERROR_INVALIDARGUMENT;
    }
    shots_ = *static_cast<const size_t*>(value);
    return QDMI_SUCCESS;
  }
  if (param == QDMI_DEVICE_JOB_PARAMETER_PROGRAM) {
    program_ = std::string(static_cast<const char*>(value), size - 1);
    return QDMI_SUCCESS;
  }
  if (param == QDMI_DEVICE_JOB_PARAMETER_PROGRAMFORMAT) {
    if (size != sizeof(QDMI_Program_Format)) {
      return QDMI_ERROR_INVALIDARGUMENT;
    }
    auto fmt = *static_cast<const QDMI_Program_Format*>(value);
    
    // Only OpenQASM 2.0 and 3.0 are currently supported
    if (fmt != QDMI_PROGRAM_FORMAT_QASM2 && fmt != QDMI_PROGRAM_FORMAT_QASM3) {
      return QDMI_ERROR_NOTSUPPORTED;
    }
    
    format_ = fmt;
    return QDMI_SUCCESS;
  }

  return QDMI_ERROR_NOTSUPPORTED;
}

auto AWS_QDMI_Device_Job_impl_d::queryProperty(
    const QDMI_Device_Job_Property prop, const size_t size, void* value,
    size_t* sizeRet) const -> QDMI_STATUS {
  if ((value != nullptr && size == 0) || prop >= QDMI_DEVICE_JOB_PROPERTY_MAX) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }

  ADD_SINGLE_VALUE_PROPERTY(QDMI_DEVICE_JOB_PROPERTY_ID, int, id_, prop, size,
                            value, sizeRet)
  ADD_SINGLE_VALUE_PROPERTY(QDMI_DEVICE_JOB_PROPERTY_PROGRAMFORMAT,
                            QDMI_Program_Format, format_, prop, size, value,
                            sizeRet)
  ADD_STRING_PROPERTY(QDMI_DEVICE_JOB_PROPERTY_PROGRAM, program_.c_str(), prop,
                      size, value, sizeRet)
  ADD_SINGLE_VALUE_PROPERTY(QDMI_DEVICE_JOB_PROPERTY_SHOTSNUM, size_t, shots_,
                            prop, size, value, sizeRet)
  ADD_STRING_PROPERTY(QDMI_DEVICE_JOB_PROPERTY_TASKARN, taskArn_.c_str(), prop,
                      size, value, sizeRet)

  return QDMI_ERROR_NOTSUPPORTED;
}

auto AWS_QDMI_Device_Job_impl_d::submit() -> QDMI_STATUS {
  const auto currentStatus = status_.load();
  if (currentStatus != QDMI_JOB_STATUS_CREATED) {
    return QDMI_ERROR_BADSTATE;
  }

  // ============================================================================
  // AWS Braket CreateQuantumTask API Call
  // ============================================================================
  // Purpose: Submit a quantum circuit for execution on the target device
  // 
  // Required Parameters:
  // - deviceArn: Target device (e.g., "arn:aws:braket:::device/quantum-simulator/amazon/sv1")
  // - action: OpenQASM 2.0/3.0 circuit string WRAPPED in Braket JSON schema
  // - shots: Number of circuit executions (measurements)
  // - outputS3Bucket: S3 location for storing results
  // 
  // AWS SDK Usage:
  // 1. Create CreateQuantumTaskRequest
  // 2. Set device ARN, shots
  // 3. Construct Action JSON:
  //    {
  //      "braketSchemaHeader": {
  //        "name": "braket.ir.openqasm.program",
  //        "version": "1"
  //      },
  //      "source": "OPENQASM 3.0; ..."
  //    }
  // 4. Set Output S3 Bucket and Prefix
  // 5. Call BraketClient::CreateQuantumTask()
  // ============================================================================

  if (program_.empty() || session_->getDeviceArn().empty()) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }

  status_.store(QDMI_JOB_STATUS_QUEUED);
  aws_qdmi::Device::get().increaseRunningJobs();

  Aws::Braket::Model::CreateQuantumTaskRequest request;
  request.SetDeviceArn(session_->getDeviceArn().c_str());
  request.SetShots(static_cast<long long>(shots_));

  // Construct the Action JSON
  Aws::Utils::Json::JsonValue actionJson;
  Aws::Utils::Json::JsonValue header;
  header.WithString("name", "braket.ir.openqasm.program");
  header.WithString("version", "1");
  actionJson.WithObject("braketSchemaHeader", header);
  actionJson.WithString("source", program_);
  
  request.SetAction(actionJson.View().WriteCompact().c_str());

  // Configure Output S3 Bucket  
  if (!session_->getS3Bucket().empty()) {
      request.SetOutputS3Bucket(session_->getS3Bucket().c_str());
      
      // Generate a prefix for this specific job to avoid collisions
      std::string prefix = "qdmi-tasks/" + std::to_string(id_);
      request.SetOutputS3KeyPrefix(prefix.c_str());
  } else {
      // If no bucket provided, AWS SDK might fail or use a default if configured in environment.
      // Ideally we should return error if bucket is missing, but let's try to proceed.
      std::cerr << "Warning: No S3 bucket provided for task results.\n";
  }

  auto outcome = session_->getClient()->CreateQuantumTask(request);
  if (!outcome.IsSuccess()) {
    std::cerr << "Failed to submit task: " << outcome.GetError().GetMessage()
              << "\n";
    status_.store(QDMI_JOB_STATUS_FAILED);
    aws_qdmi::Device::get().decreaseRunningJobs();
    return QDMI_ERROR_NOTSUPPORTED;
  }

  taskArn_ = outcome.GetResult().GetQuantumTaskArn();
  status_.store(QDMI_JOB_STATUS_RUNNING);
  return QDMI_SUCCESS;
}

auto AWS_QDMI_Device_Job_impl_d::cancel() -> QDMI_STATUS {
  const auto currentStatus = status_.load();
  
  if (currentStatus == QDMI_JOB_STATUS_CREATED) {
    status_.store(QDMI_JOB_STATUS_CANCELED);
    return QDMI_SUCCESS;
  }
  
  if (currentStatus != QDMI_JOB_STATUS_QUEUED &&
      currentStatus != QDMI_JOB_STATUS_RUNNING) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }

  // ============================================================================
  // AWS Braket CancelQuantumTask API Call
  // ============================================================================
  // 
  // AWS SDK Usage:
  // 1. Create CancelQuantumTaskRequest with taskArn
  // 2. Call BraketClient::CancelQuantumTask()
  // 3. Task status will transition to CANCELLED
  // 
  // Important Notes:
  // - Can only cancel tasks in CREATED, QUEUED, or RUNNING state
  // - Cannot cancel COMPLETED or FAILED tasks
  // - Cancellation is best-effort: if task is already executing, it may complete
  // - Some devices may not support mid-execution cancellation
  // - No refund for cancelled tasks on QPU hardware (charged for queue time)
  // 
  // After cancellation:
  // - Task status becomes CANCELLED
  // - No results will be available
  // - GetQuantumTask() will show status and cancellation reason
  // ============================================================================

  if (taskArn_.empty()) {
    return QDMI_ERROR_BADSTATE;
  }

  Aws::Braket::Model::CancelQuantumTaskRequest request;
  request.SetQuantumTaskArn(taskArn_.c_str());

  auto outcome = session_->getClient()->CancelQuantumTask(request);
  if (!outcome.IsSuccess()) {
    std::cerr << "Failed to cancel task: " << outcome.GetError().GetMessage()
              << "\n";
    return QDMI_ERROR_NOTSUPPORTED;
  }

  status_.store(QDMI_JOB_STATUS_CANCELED);
  aws_qdmi::Device::get().decreaseRunningJobs();
  return QDMI_SUCCESS;
}

auto AWS_QDMI_Device_Job_impl_d::check(QDMI_Job_Status* status) const
    -> QDMI_STATUS {
  if (status == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }

  // ============================================================================
  // AWS Braket GetQuantumTask API Call
  // ============================================================================
  // Purpose: Poll the current status of a submitted quantum task
  // 
  // AWS SDK Usage:
  // 1. Create GetQuantumTaskRequest with taskArn
  // 2. Call BraketClient::GetQuantumTask()
  // 3. Extract QuantumTaskStatus from GetQuantumTaskResult
  // 4. Map AWS status to QDMI status enum
  // 
  // AWS QuantumTaskStatus Enum Values:
  // - CREATED: Task created but not yet queued
  // - QUEUED: Waiting in device queue
  // - RUNNING: Actively executing on device
  // - COMPLETED: Finished successfully, results available
  // - FAILED: Execution failed (circuit error, device error, etc.)
  // - CANCELLED: User cancelled the task
  // 
  // Status Mapping to QDMI:
  // AWS CREATED    → QDMI_JOB_STATUS_CREATED
  // AWS QUEUED     → QDMI_JOB_STATUS_QUEUED  
  // AWS RUNNING    → QDMI_JOB_STATUS_RUNNING
  // AWS COMPLETED  → QDMI_JOB_STATUS_DONE
  // AWS FAILED     → QDMI_JOB_STATUS_FAILED
  // AWS CANCELLED  → QDMI_JOB_STATUS_CANCELED
  // 
  // Typical polling pattern:
  // - Call check() every 1-5 seconds until status is DONE/FAILED/CANCELLED
  // - Simulators: usually complete in < 1 minute
  // - Real QPUs: can take minutes to hours depending on queue
  // ============================================================================

  if (taskArn_.empty()) {
    *status = status_.load();
    return QDMI_SUCCESS;
  }

  Aws::Braket::Model::GetQuantumTaskRequest request;
  request.SetQuantumTaskArn(taskArn_.c_str());

  auto outcome = session_->getClient()->GetQuantumTask(request);
  if (!outcome.IsSuccess()) {
    std::cerr << "Failed to check task: " << outcome.GetError().GetMessage()
              << "\n";
    return QDMI_ERROR_NOTSUPPORTED;
  }

  const auto& taskStatus = outcome.GetResult().GetStatus();
  QDMI_Job_Status newStatus;
  if (taskStatus == Aws::Braket::Model::QuantumTaskStatus::CREATED) {
    newStatus = QDMI_JOB_STATUS_CREATED;
  } else if (taskStatus == Aws::Braket::Model::QuantumTaskStatus::QUEUED) {
    newStatus = QDMI_JOB_STATUS_QUEUED;
  } else if (taskStatus == Aws::Braket::Model::QuantumTaskStatus::RUNNING) {
    newStatus = QDMI_JOB_STATUS_RUNNING;
  } else if (taskStatus == Aws::Braket::Model::QuantumTaskStatus::COMPLETED) {
    newStatus = QDMI_JOB_STATUS_DONE;
    // Store S3 location for result retrieval
    outputS3Bucket_ = outcome.GetResult().GetOutputS3Bucket();
    outputS3Directory_ = outcome.GetResult().GetOutputS3Directory();
    aws_qdmi::Device::get().decreaseRunningJobs();
  } else if (taskStatus == Aws::Braket::Model::QuantumTaskStatus::FAILED) {
    newStatus = QDMI_JOB_STATUS_FAILED;
    aws_qdmi::Device::get().decreaseRunningJobs();
  } else if (taskStatus == Aws::Braket::Model::QuantumTaskStatus::CANCELLED) {
    newStatus = QDMI_JOB_STATUS_CANCELED;
    aws_qdmi::Device::get().decreaseRunningJobs();
  } else {
    newStatus = status_.load();
  }
  
  status_.store(newStatus);
  *status = newStatus;
  return QDMI_SUCCESS;
}

auto AWS_QDMI_Device_Job_impl_d::wait(const size_t timeout) const
    -> QDMI_STATUS {
  const auto currentStatus = status_.load();
  if (currentStatus == QDMI_JOB_STATUS_CREATED) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }

  if (currentStatus == QDMI_JOB_STATUS_DONE ||
      currentStatus == QDMI_JOB_STATUS_FAILED ||
      currentStatus == QDMI_JOB_STATUS_CANCELED) {
    return QDMI_SUCCESS;
  }

  const auto startTime = std::chrono::steady_clock::now();
  QDMI_Job_Status checkedStatus;
  
  while (true) {
    const QDMI_STATUS result = check(&checkedStatus);
    if (result != QDMI_SUCCESS) {
      return result;
    }
    
    if (checkedStatus == QDMI_JOB_STATUS_DONE ||
        checkedStatus == QDMI_JOB_STATUS_FAILED ||
        checkedStatus == QDMI_JOB_STATUS_CANCELED) {
      return QDMI_SUCCESS;
    }
    
    if (timeout > 0) {
      const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - startTime).count();
      if (static_cast<size_t>(elapsed) >= timeout) {
        return QDMI_ERROR_TIMEOUT;
      }
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

/**
 * Fetch results from S3 and parse into QDMI format.
 * 
 * Downloads results.json from S3 and parses the measurements array.
 * AWS Braket stores results in format:
 * {
 *   "measurements": [[0,0], [1,1], [0,0], ...],
 *   "measuredQubits": [0, 1],
 *   "taskMetadata": { "shots": 100, ... }
 * }
 * 
 * Converts to:
 * - shotsString_: "00,11,00,..." (comma-separated bitstrings)
 * - counts_: {"00": 52, "11": 48} (histogram)
 */
auto AWS_QDMI_Device_Job_impl_d::fetchResults() const -> QDMI_STATUS {
  if (resultsFetched_) {
    return QDMI_SUCCESS;
  }
  
  if (outputS3Bucket_.empty() || outputS3Directory_.empty()) {
    std::cerr << "S3 output location not available\n";
    return QDMI_ERROR_FATAL;
  }
  
  // Create S3 client with same region as Braket client
  Aws::Client::ClientConfiguration s3Config;
  s3Config.region = session_->getRegion();
  Aws::S3::S3Client s3Client(s3Config);
  
  // Download results.json from S3
  // Key format: {outputS3Directory}/results.json
  std::string objectKey = outputS3Directory_ + "/results.json";
  
  Aws::S3::Model::GetObjectRequest getRequest;
  getRequest.SetBucket(outputS3Bucket_.c_str());
  getRequest.SetKey(objectKey.c_str());
  
  auto outcome = s3Client.GetObject(getRequest);
  if (!outcome.IsSuccess()) {
    std::cerr << "Failed to download results from S3: " 
              << outcome.GetError().GetMessage() << "\n";
    return QDMI_ERROR_FATAL;
  }
  
  // Read response body into string
  std::stringstream ss;
  ss << outcome.GetResult().GetBody().rdbuf();
  std::string jsonStr = ss.str();
  
  // Parse JSON
  Aws::Utils::Json::JsonValue json(jsonStr);
  if (!json.WasParseSuccessful()) {
    std::cerr << "Failed to parse results JSON\n";
    return QDMI_ERROR_FATAL;
  }
  
  auto root = json.View();
  
  // Parse measurements array: [[0,0], [1,1], [0,0], ...]
  if (!root.KeyExists("measurements")) {
    std::cerr << "No measurements in results\n";
    return QDMI_ERROR_FATAL;
  }
  
  auto measurements = root.GetArray("measurements");
  std::vector<std::string> shotsList;
  shotsList.reserve(measurements.GetLength());
  
  for (size_t i = 0; i < measurements.GetLength(); ++i) {
    auto shot = measurements[i].AsArray();
    std::string bitstring;
    
    // Each shot is an array of qubit values: [0, 0] or [1, 1]
    for (size_t q = 0; q < shot.GetLength(); ++q) {
      bitstring += std::to_string(shot[q].AsInteger());
    }
    
    shotsList.push_back(bitstring);
    counts_[bitstring]++;
  }
  
  // Build comma-separated shots string for QDMI_JOB_RESULT_SHOTS
  for (size_t i = 0; i < shotsList.size(); ++i) {
    if (i > 0) {
      shotsString_ += ',';
    }
    shotsString_ += shotsList[i];
  }
  
  resultsFetched_ = true;
  return QDMI_SUCCESS;
}

/**
 * Retrieve results from a completed quantum job.
 * 
 * Result Types:
 * - SHOTS: Comma-separated bitstrings "00,11,00,11,..." 
 * - HIST_KEYS: Null-terminated unique outcomes "00\011\0"
 * - HIST_VALUES: Count for each outcome [52, 48]
 * - STATEVECTOR/PROBABILITIES: Only from simulators (not supported yet)
 */
auto AWS_QDMI_Device_Job_impl_d::getResults(const QDMI_Job_Result result,
                                             const size_t size, void* data,
                                             size_t* sizeRet) const -> QDMI_STATUS {
  if ((data != nullptr && size == 0) || result >= QDMI_JOB_RESULT_MAX) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  
  const auto currentStatus = status_.load();
  if (currentStatus != QDMI_JOB_STATUS_DONE) {
    return QDMI_ERROR_BADSTATE;
  }

  // Fetch results from S3 if not already done
  QDMI_STATUS fetchStatus = fetchResults();
  if (fetchStatus != QDMI_SUCCESS) {
    return fetchStatus;
  }

  if (result == QDMI_JOB_RESULT_SHOTS) {
    // Return comma-separated shot results: "00,11,00,11,..."
    // Size includes null terminator
    size_t totalSize = shotsString_.size() + 1;
    
    if (data != nullptr) {
      if (size < totalSize) {
        return QDMI_ERROR_INVALIDARGUMENT;
      }
      memcpy(data, shotsString_.c_str(), totalSize);
    }
    if (sizeRet != nullptr) {
      *sizeRet = totalSize;
    }
    return QDMI_SUCCESS;
  }

  if (result == QDMI_JOB_RESULT_HIST_KEYS) {
    // Return concatenated null-terminated bitstrings: "00\011\0"
    std::string keys_str;
    for (const auto& [key, count] : counts_) {
      keys_str += key;
      keys_str += '\0';
    }
    
    if (data != nullptr) {
      if (size < keys_str.size()) {
        return QDMI_ERROR_INVALIDARGUMENT;
      }
      memcpy(data, keys_str.data(), keys_str.size());
    }
    if (sizeRet != nullptr) {
      *sizeRet = keys_str.size();
    }
    return QDMI_SUCCESS;
  }
  
  if (result == QDMI_JOB_RESULT_HIST_VALUES) {
    // Return array of counts corresponding to HIST_KEYS
    std::vector<size_t> values;
    for (const auto& [key, count] : counts_) {
      values.push_back(count);
    }
    
    if (data != nullptr) {
      if (size < values.size() * sizeof(size_t)) {
        return QDMI_ERROR_INVALIDARGUMENT;
      }
      memcpy(data, values.data(), values.size() * sizeof(size_t));
    }
    if (sizeRet != nullptr) {
      *sizeRet = values.size() * sizeof(size_t);
    }
    return QDMI_SUCCESS;
  }
  
  if (result == QDMI_JOB_RESULT_STATEVECTOR_DENSE ||
      result == QDMI_JOB_RESULT_PROBABILITIES_DENSE ||
      result == QDMI_JOB_RESULT_STATEVECTOR_SPARSE_KEYS ||
      result == QDMI_JOB_RESULT_STATEVECTOR_SPARSE_VALUES ||
      result == QDMI_JOB_RESULT_PROBABILITIES_SPARSE_KEYS ||
      result == QDMI_JOB_RESULT_PROBABILITIES_SPARSE_VALUES) {
    // Statevector and probabilities only available from simulators
    // TODO: Parse from S3 results if device supports statevector
    if (sizeRet != nullptr) {
      *sizeRet = 0;
    }
    return QDMI_ERROR_NOTSUPPORTED;
  }

  return QDMI_ERROR_NOTSUPPORTED;
}

// ============================================================================
// C API Implementation
// ============================================================================
// These functions provide the C interface required by QDMI specification.
// They wrap the C++ implementation classes above.

/**
 * Initialize the QDMI device library.
 * 
 * Must be called once at program startup before any other QDMI functions.
 * Initializes the AWS SDK and sets up the device singleton.
 * 
 * Thread-safe: Can be called multiple times, only first call initializes.
 * 
 * @return QDMI_SUCCESS on successful initialization
 */

// Global SDK state - must be in same translation unit for safe init/shutdown
namespace {
  Aws::SDKOptions g_awsOptions;
  bool g_awsInitialized = false;
  std::mutex g_awsInitMutex;
}

int AWS_QDMI_device_initialize() {
  std::lock_guard<std::mutex> lock(g_awsInitMutex);
  if (!g_awsInitialized) {
    Aws::InitAPI(g_awsOptions);
    g_awsInitialized = true;
  }
  std::ignore = aws_qdmi::Device::get();
  return QDMI_SUCCESS;
}

/**
 * Finalize the QDMI device.
 * 
 * Should be called once at program exit after all QDMI resources are freed.
 * Shuts down the AWS SDK and releases global resources.
 * 
 * Thread-safe: Can be called multiple times, only first call shuts down.
 * 
 * WARNING: After calling finalize(), no other AWS SDK calls should be made.
 * Any sessions or jobs should be freed BEFORE calling this function.
 * 
 * @return QDMI_SUCCESS on successful finalization
 */
int AWS_QDMI_device_finalize() {
  std::lock_guard<std::mutex> lock(g_awsInitMutex);
  if (g_awsInitialized) {
    Aws::ShutdownAPI(g_awsOptions);
    g_awsInitialized = false;
  }
  return QDMI_SUCCESS;
}

int AWS_QDMI_device_session_alloc(AWS_QDMI_Device_Session* session) {
  return aws_qdmi::Device::get().sessionAlloc(session);
}

int AWS_QDMI_device_session_init(AWS_QDMI_Device_Session session) {
  if (session == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  return session->init();
}

void AWS_QDMI_device_session_free(AWS_QDMI_Device_Session session) {
  aws_qdmi::Device::get().sessionFree(session);
}

int AWS_QDMI_device_session_set_parameter(AWS_QDMI_Device_Session session,
                                           QDMI_Device_Session_Parameter param,
                                           const size_t size,
                                           const void* value) {
  if (session == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  return session->setParameter(param, size, value);
}

int AWS_QDMI_device_session_query_device_property(
    AWS_QDMI_Device_Session session, const QDMI_Device_Property prop,
    const size_t size, void* value, size_t* sizeRet) {
  if (session == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  return session->queryDeviceProperty(prop, size, value, sizeRet);
}

int AWS_QDMI_device_session_create_device_job(AWS_QDMI_Device_Session session,
                                               AWS_QDMI_Device_Job* job) {
  if (session == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  return session->createDeviceJob(job);
}

void AWS_QDMI_device_job_free(AWS_QDMI_Device_Job job) {
  if (job != nullptr) {
    job->free();
  }
}

int AWS_QDMI_device_job_set_parameter(AWS_QDMI_Device_Job job,
                                       const QDMI_Device_Job_Parameter param,
                                       const size_t size, const void* value) {
  if (job == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  return job->setParameter(param, size, value);
}

int AWS_QDMI_device_job_query_property(AWS_QDMI_Device_Job job,
                                        const QDMI_Device_Job_Property prop,
                                        const size_t size, void* value,
                                        size_t* sizeRet) {
  if (job == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  return job->queryProperty(prop, size, value, sizeRet);
}

int AWS_QDMI_device_job_submit(AWS_QDMI_Device_Job job) {
  if (job == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  return job->submit();
}

int AWS_QDMI_device_job_cancel(AWS_QDMI_Device_Job job) {
  if (job == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  return job->cancel();
}

int AWS_QDMI_device_job_check(AWS_QDMI_Device_Job job,
                               QDMI_Job_Status* status) {
  if (job == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  return job->check(status);
}

int AWS_QDMI_device_job_wait(AWS_QDMI_Device_Job job, const size_t timeout) {
  if (job == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  return job->wait(timeout);
}

int AWS_QDMI_device_job_get_results(AWS_QDMI_Device_Job job,
                                     QDMI_Job_Result result, const size_t size,
                                     void* data, size_t* sizeRet) {
  if (job == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  return job->getResults(result, size, data, sizeRet);
}

int AWS_QDMI_device_session_query_site_property(
    AWS_QDMI_Device_Session session, AWS_QDMI_Site site,
    QDMI_Site_Property prop, const size_t size, void* value,
    size_t* sizeRet) {
  if (session == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  return session->querySiteProperty(site, prop, size, value, sizeRet);
}

int AWS_QDMI_device_session_query_operation_property(
    AWS_QDMI_Device_Session session, AWS_QDMI_Operation operation,
    const size_t num_sites, const AWS_QDMI_Site* sites,
    const size_t num_params, const double* params,
    QDMI_Operation_Property prop, const size_t size, void* value,
    size_t* sizeRet) {
  if (session == nullptr) {
    return QDMI_ERROR_INVALIDARGUMENT;
  }
  return session->queryOperationProperty(
      operation, num_sites,
      reinterpret_cast<const AWS_QDMI_Site_impl_d* const*>(sites), num_params,
      params, prop, size, value, sizeRet);
}
