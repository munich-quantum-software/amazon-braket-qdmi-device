/*
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * Licensed under the Apache License v2.0 with LLVM Exceptions (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * https://llvm.org/LICENSE.txt
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations under
 * the License.
 *
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

/**
 * @file test_device_unit.cpp
 * @brief Offline unit tests for the Amazon Braket QDMI device.
 *
 * These tests exercise local code paths only, without making any real AWS API
 * calls.
 *
 * Fixtures:
 *  - AmazonBraketQDMIOfflineTest   : session allocated but NOT initialised
 *  - AmazonBraketQDMILocalJobTest  : session initialised with fake credentials
 */

#include "CapabilityFixtures.hpp"
#include "amazon-braket-qdmi-device/Device.hpp"
#include "amazon-braket-qdmi-device/DeviceParser.hpp"
#include "amazon-braket-qdmi-device/Queue.hpp"
#include "amazon-braket-qdmi-device/Wait.hpp"
#include "amazon-braket-qdmi-device/constants.hpp"
#include "amazon_braket_qdmi/device.h"

#include <algorithm>
#include <array>
#include <aws/braket/BraketClient.h>
#include <aws/braket/BraketErrors.h>
#include <aws/braket/BraketServiceClientModel.h>
#include <aws/braket/model/CancelQuantumTaskRequest.h>
#include <aws/braket/model/CancelQuantumTaskResult.h>
#include <aws/braket/model/CreateQuantumTaskRequest.h>
#include <aws/braket/model/CreateQuantumTaskResult.h>
#include <aws/braket/model/DeviceStatus.h>
#include <aws/braket/model/DeviceType.h>
#include <aws/braket/model/GetDeviceRequest.h>
#include <aws/braket/model/GetDeviceResult.h>
#include <aws/braket/model/GetQuantumTaskRequest.h>
#include <aws/braket/model/GetQuantumTaskResult.h>
#include <aws/braket/model/QuantumTaskStatus.h>
#include <aws/core/client/AWSError.h>
#include <aws/core/client/ClientConfiguration.h>
#include <aws/core/http/HttpResponse.h>
#include <aws/core/utils/memory/AWSMemory.h>
#include <aws/core/utils/memory/stl/AWSStringStream.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/S3Errors.h>
#include <aws/s3/S3ServiceClientModel.h>
#include <aws/s3/model/CreateBucketRequest.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <aws/s3/model/GetObjectResult.h>
#include <aws/sts/STSClient.h>
#include <aws/sts/STSErrors.h>
#include <aws/sts/STSServiceClientModel.h>
#include <aws/sts/model/GetCallerIdentityResult.h>
#ifdef _WIN32
#include <aws/core/Aws.h>
#endif
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <future>
#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>
#include <iterator>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <stdlib.h> // NOLINT(modernize-deprecated-headers): POSIX setenv/unsetenv
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

struct AMAZON_BRAKET_QDMI_Device_Job_TestAccess {
  static auto setStatus(AMAZON_BRAKET_QDMI_Device_Job job,
                        const QDMI_Job_Status status) -> void {
    job->status_.store(status);
  }

  static auto
  wait(AMAZON_BRAKET_QDMI_Device_Job job, const size_t timeout,
       const amazon::braket::qdmi::detail::JobWaitFunctions& functions)
      -> QDMI_STATUS {
    job->status_.store(QDMI_JOB_STATUS_RUNNING);
    return job->wait(timeout, functions);
  }

  [[nodiscard]] static auto outputLocation(AMAZON_BRAKET_QDMI_Device_Job job)
      -> std::pair<std::string, std::string> {
    return {job->outputS3Bucket_, job->outputS3Directory_};
  }
};

struct AMAZON_BRAKET_QDMI_Device_Session_TestAccess {
  static auto setArchitecture(
      AMAZON_BRAKET_QDMI_Device_Session session,
      std::shared_ptr<amazon::braket::qdmi::DeviceArchitecture> architecture)
      -> void {
    const std::scoped_lock lock(session->cachedArchitectureMutex_);
    session->cachedArchitecture_ = std::move(architecture);
  }

  static auto setClient(AMAZON_BRAKET_QDMI_Device_Session session,
                        std::unique_ptr<Aws::Braket::BraketClient> client)
      -> void {
    session->client_ = std::move(client);
  }

  static auto setS3Client(AMAZON_BRAKET_QDMI_Device_Session session,
                          std::unique_ptr<Aws::S3::S3Client> client) -> void {
    session->s3Client_ = std::move(client);
  }

  static auto setStsClient(AMAZON_BRAKET_QDMI_Device_Session session,
                           std::unique_ptr<Aws::STS::STSClient> client)
      -> void {
    session->stsClient_ = std::move(client);
  }

  static auto setRegion(AMAZON_BRAKET_QDMI_Device_Session session,
                        std::string region) -> void {
    session->region_ = std::move(region);
  }
};

namespace {
class StubBraketClient final : public Aws::Braket::BraketClient {
public:
  explicit StubBraketClient(
      Aws::Braket::Model::GetQuantumTaskResult result,
      const std::optional<Aws::Braket::BraketErrors> getError = std::nullopt,
      const std::optional<Aws::Braket::BraketErrors> cancelError = std::nullopt,
      const bool failGetAfterFirstCall = false,
      const std::optional<Aws::Braket::BraketErrors> createError = std::nullopt)
      : Aws::Braket::BraketClient(
            Aws::Auth::AWSCredentials{"access-key", "secret-key"},
            clientConfiguration()),
        result_(std::move(result)), getError_(getError),
        cancelError_(cancelError),
        failGetAfterFirstCall_(failGetAfterFirstCall),
        createError_(createError) {}

  auto GetDevice(const Aws::Braket::Model::GetDeviceRequest& request) const
      -> Aws::Braket::Model::GetDeviceOutcome override {
    static_cast<void>(request);
    Aws::Braket::Model::GetDeviceResult result;
    result.SetDeviceName("SV1");
    result.SetProviderName("Amazon Web Services");
    result.SetDeviceType(Aws::Braket::Model::DeviceType::SIMULATOR);
    result.SetDeviceStatus(Aws::Braket::Model::DeviceStatus::ONLINE);
    result.SetDeviceCapabilities(
        std::string{amazon::braket::qdmi::test::SV1}.c_str());
    return result;
  }

  auto
  GetQuantumTask(const Aws::Braket::Model::GetQuantumTaskRequest& request) const
      -> Aws::Braket::Model::GetQuantumTaskOutcome override {
    ++calls_;
    requestedArn_ = request.GetQuantumTaskArn();
    if (getError_.has_value() && (!failGetAfterFirstCall_ || calls_ > 1)) {
      Aws::Client::AWSError<Aws::Braket::BraketErrors> error{
          *getError_, "StubError", "stubbed GetQuantumTask failure", false};
      return Aws::Braket::BraketError{std::move(error)};
    }
    return result_;
  }

  auto CancelQuantumTask(
      const Aws::Braket::Model::CancelQuantumTaskRequest& request) const
      -> Aws::Braket::Model::CancelQuantumTaskOutcome override {
    ++cancelCalls_;
    canceledArn_ = request.GetQuantumTaskArn();
    if (cancelError_.has_value()) {
      Aws::Client::AWSError<Aws::Braket::BraketErrors> error{
          *cancelError_, "StubError", "stubbed CancelQuantumTask failure",
          false};
      return Aws::Braket::BraketError{std::move(error)};
    }
    return Aws::Braket::Model::CancelQuantumTaskResult{};
  }

  auto CreateQuantumTask(
      const Aws::Braket::Model::CreateQuantumTaskRequest& request) const
      -> Aws::Braket::Model::CreateQuantumTaskOutcome override {
    ++createCalls_;
    {
      std::unique_lock lock(createMutex_);
      outputBucket_ = request.GetOutputS3Bucket();
      outputPrefix_ = request.GetOutputS3KeyPrefix();
      if (!request.GetAssociations().empty()) {
        reservationArn_ = request.GetAssociations().front().GetArn();
      }
      createEntered_ = true;
      createChanged_.notify_all();
      createChanged_.wait(lock,
                          [this] { return !blockCreate_ || releaseCreate_; });
    }
    if (createError_.has_value()) {
      Aws::Client::AWSError<Aws::Braket::BraketErrors> error{
          *createError_, "StubError", "stubbed CreateQuantumTask failure",
          false};
      return Aws::Braket::BraketError{std::move(error)};
    }
    return Aws::Braket::Model::CreateQuantumTaskResult{}.WithQuantumTaskArn(
        "arn:aws:braket:us-east-1:123456789012:quantum-task/task-id");
  }

  [[nodiscard]] auto calls() const -> size_t { return calls_; }
  [[nodiscard]] auto cancelCalls() const -> size_t { return cancelCalls_; }
  [[nodiscard]] auto requestedArn() const -> const std::string& {
    return requestedArn_;
  }
  [[nodiscard]] auto canceledArn() const -> const std::string& {
    return canceledArn_;
  }
  [[nodiscard]] auto createCalls() const -> size_t {
    return createCalls_.load();
  }
  [[nodiscard]] auto outputBucket() const -> std::string {
    const std::scoped_lock lock(createMutex_);
    return outputBucket_;
  }
  [[nodiscard]] auto outputPrefix() const -> std::string {
    const std::scoped_lock lock(createMutex_);
    return outputPrefix_;
  }
  [[nodiscard]] auto reservationArn() const -> std::string {
    const std::scoped_lock lock(createMutex_);
    return reservationArn_;
  }
  auto blockCreate() -> void {
    const std::scoped_lock lock(createMutex_);
    blockCreate_ = true;
  }
  [[nodiscard]] auto waitForCreate() const -> bool {
    std::unique_lock lock(createMutex_);
    return createChanged_.wait_for(lock, std::chrono::seconds{5},
                                   [this] { return createEntered_; });
  }
  auto releaseCreate() -> void {
    const std::scoped_lock lock(createMutex_);
    releaseCreate_ = true;
    createChanged_.notify_all();
  }

private:
  static auto clientConfiguration() -> Aws::Client::ClientConfiguration {
    Aws::Client::ClientConfiguration config;
    config.region = "us-east-1";
    return config;
  }

  Aws::Braket::Model::GetQuantumTaskResult result_;
  std::optional<Aws::Braket::BraketErrors> getError_;
  std::optional<Aws::Braket::BraketErrors> cancelError_;
  bool failGetAfterFirstCall_;
  std::optional<Aws::Braket::BraketErrors> createError_;
  mutable size_t calls_ = 0;
  mutable size_t cancelCalls_ = 0;
  mutable std::string requestedArn_;
  mutable std::string canceledArn_;
  mutable std::atomic<size_t> createCalls_ = 0;
  mutable std::mutex createMutex_;
  mutable std::condition_variable createChanged_;
  mutable bool blockCreate_ = false;
  mutable bool createEntered_ = false;
  mutable bool releaseCreate_ = false;
  mutable std::string outputBucket_;
  mutable std::string outputPrefix_;
  mutable std::string reservationArn_;
};

class StubStsClient final : public Aws::STS::STSClient {
public:
  explicit StubStsClient(
      const std::optional<Aws::STS::STSErrors> error = std::nullopt,
      std::string account = "123456789012")
      : Aws::STS::STSClient(Aws::Auth::AWSCredentials{"access", "secret"}),
        error_(error), account_(std::move(account)) {}

  auto GetCallerIdentity(
      const Aws::STS::Model::GetCallerIdentityRequest& request) const
      -> Aws::STS::Model::GetCallerIdentityOutcome override {
    static_cast<void>(request);
    ++calls_;
    if (error_.has_value()) {
      Aws::Client::AWSError<Aws::STS::STSErrors> error{
          *error_, "StubError", "stubbed STS failure", false};
      return Aws::STS::STSError{std::move(error)};
    }
    return Aws::STS::Model::GetCallerIdentityResult{}.WithAccount(account_);
  }

  [[nodiscard]] auto calls() const -> size_t { return calls_; }

private:
  std::optional<Aws::STS::STSErrors> error_;
  std::string account_;
  mutable size_t calls_ = 0;
};

class StubS3Client final : public Aws::S3::S3Client {
public:
  struct Configuration {
    std::optional<Aws::S3::S3Errors> createError = std::nullopt;
    std::optional<Aws::S3::S3Errors> getObjectError = std::nullopt;
    Aws::Http::HttpResponseCode responseCode =
        Aws::Http::HttpResponseCode::REQUEST_NOT_MADE;
    std::string exceptionName = "StubError";
    std::string errorMessage = "stubbed S3 failure";
    std::string resultJson = R"({"measurements":[[0,0],[1,1]]})";
  };

  StubS3Client()
      : Aws::S3::S3Client(Aws::Auth::AWSCredentials{"access", "secret"}) {}

  explicit StubS3Client(Configuration configuration)
      : Aws::S3::S3Client(Aws::Auth::AWSCredentials{"access", "secret"}),
        configuration_(std::move(configuration)) {}

  auto CreateBucket(const Aws::S3::Model::CreateBucketRequest& request) const
      -> Aws::S3::Model::CreateBucketOutcome override {
    ++createCalls_;
    bucket_ = request.GetBucket();
    hasLocationConstraint_ = request.CreateBucketConfigurationHasBeenSet();
    if (configuration_.createError.has_value()) {
      Aws::Client::AWSError<Aws::S3::S3Errors> error{
          *configuration_.createError, configuration_.exceptionName,
          configuration_.errorMessage, false};
      error.SetResponseCode(configuration_.responseCode);
      return Aws::S3::S3Error{std::move(error)};
    }
    return Aws::S3::Model::CreateBucketResult{};
  }

  auto GetObject(const Aws::S3::Model::GetObjectRequest& request) const
      -> Aws::S3::Model::GetObjectOutcome override {
    ++getObjectCalls_;
    getObjectBucket_ = request.GetBucket();
    getObjectKey_ = request.GetKey();
    if (configuration_.getObjectError.has_value()) {
      Aws::Client::AWSError<Aws::S3::S3Errors> error{
          *configuration_.getObjectError, configuration_.exceptionName,
          configuration_.errorMessage, false};
      error.SetResponseCode(configuration_.responseCode);
      return Aws::S3::S3Error{std::move(error)};
    }
    Aws::S3::Model::GetObjectResult result;
    result.ReplaceBody(Aws::New<Aws::StringStream>("AmazonBraketQDMIResult",
                                                   configuration_.resultJson));
    return Aws::S3::Model::GetObjectOutcome{std::move(result)};
  }

  [[nodiscard]] auto createCalls() const -> size_t { return createCalls_; }
  [[nodiscard]] auto hasLocationConstraint() const -> bool {
    return hasLocationConstraint_;
  }
  [[nodiscard]] auto bucket() const -> const std::string& { return bucket_; }
  [[nodiscard]] auto getObjectCalls() const -> size_t {
    return getObjectCalls_;
  }
  [[nodiscard]] auto getObjectBucket() const -> const std::string& {
    return getObjectBucket_;
  }
  [[nodiscard]] auto getObjectKey() const -> const std::string& {
    return getObjectKey_;
  }

private:
  Configuration configuration_;
  mutable size_t createCalls_ = 0;
  mutable bool hasLocationConstraint_ = false;
  mutable std::string bucket_;
  mutable size_t getObjectCalls_ = 0;
  mutable std::string getObjectBucket_;
  mutable std::string getObjectKey_;
};

class StubGetDeviceClient final : public Aws::Braket::BraketClient {
public:
  explicit StubGetDeviceClient(
      Aws::Braket::Model::GetDeviceResult result,
      const std::optional<Aws::Braket::BraketErrors> error = std::nullopt,
      const bool failAfterFirstCall = false)
      : Aws::Braket::BraketClient(
            Aws::Auth::AWSCredentials{"access-key", "secret-key"},
            clientConfiguration()),
        result_(std::move(result)), error_(error),
        failAfterFirstCall_(failAfterFirstCall) {}

  auto GetDevice(const Aws::Braket::Model::GetDeviceRequest& request) const
      -> Aws::Braket::Model::GetDeviceOutcome override {
    requestedArn_ = request.GetDeviceArn();
    ++calls_;
    if (error_.has_value() && (!failAfterFirstCall_ || calls_ > 1U)) {
      Aws::Client::AWSError<Aws::Braket::BraketErrors> error{
          *error_, "StubError", "stubbed GetDevice failure", false};
      return Aws::Braket::BraketError{std::move(error)};
    }
    return result_;
  }

  [[nodiscard]] auto calls() const -> size_t { return calls_; }
  [[nodiscard]] auto requestedArn() const -> const std::string& {
    return requestedArn_;
  }

private:
  static auto clientConfiguration() -> Aws::Client::ClientConfiguration {
    Aws::Client::ClientConfiguration config;
    config.region = "us-east-1";
    return config;
  }

  Aws::Braket::Model::GetDeviceResult result_;
  std::optional<Aws::Braket::BraketErrors> error_;
  bool failAfterFirstCall_;
  mutable size_t calls_ = 0;
  mutable std::string requestedArn_;
};

class ThrowingGetDeviceClient final : public Aws::Braket::BraketClient {
public:
  ThrowingGetDeviceClient()
      : Aws::Braket::BraketClient(
            Aws::Auth::AWSCredentials{"access-key", "secret-key"},
            clientConfiguration()) {}

  auto GetDevice([[maybe_unused]] const Aws::Braket::Model::GetDeviceRequest&
                     request) const
      -> Aws::Braket::Model::GetDeviceOutcome override {
    switch (calls_++) {
    case 0:
      throw std::bad_alloc{};
    case 1:
      throw std::invalid_argument{"stubbed invalid argument"};
    default:
      throw std::runtime_error{"stubbed unexpected failure"};
    }
  }

private:
  static auto clientConfiguration() -> Aws::Client::ClientConfiguration {
    Aws::Client::ClientConfiguration config;
    config.region = "us-east-1";
    return config;
  }

  mutable size_t calls_ = 0;
};

class ConcurrentGetDeviceClient final : public Aws::Braket::BraketClient {
public:
  explicit ConcurrentGetDeviceClient(Aws::Braket::Model::GetDeviceResult result)
      : Aws::Braket::BraketClient(
            Aws::Auth::AWSCredentials{"access-key", "secret-key"},
            clientConfiguration()),
        result_(std::move(result)) {}

  auto GetDevice([[maybe_unused]] const Aws::Braket::Model::GetDeviceRequest&
                     request) const
      -> Aws::Braket::Model::GetDeviceOutcome override {
    std::unique_lock lock(mutex_);
    ++calls_;
    callsChanged_.notify_all();
    if (calls_ == 1U) {
      // On an unsynchronized implementation, let the second cache miss return
      // and publish its independent architecture first. With serialized
      // publication, this wait expires before the second status refresh.
      callsChanged_.wait_for(lock, std::chrono::milliseconds{250},
                             [this] { return calls_ >= 2U; });
      if (calls_ >= 2U) {
        lock.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds{25});
      }
    }
    return result_;
  }

  [[nodiscard]] auto waitForFirstCall() const -> bool {
    std::unique_lock lock(mutex_);
    return callsChanged_.wait_for(lock, std::chrono::seconds{5},
                                  [this] { return calls_ >= 1U; });
  }

private:
  static auto clientConfiguration() -> Aws::Client::ClientConfiguration {
    Aws::Client::ClientConfiguration config;
    config.region = "us-east-1";
    return config;
  }

  Aws::Braket::Model::GetDeviceResult result_;
  mutable std::mutex mutex_;
  mutable std::condition_variable callsChanged_;
  mutable size_t calls_ = 0;
};

auto installParsedArchitecture(AMAZON_BRAKET_QDMI_Device_Session session,
                               ParsedDeviceProperties properties)
    -> std::shared_ptr<amazon::braket::qdmi::DeviceArchitecture> {
  auto architecture =
      std::make_shared<amazon::braket::qdmi::DeviceArchitecture>();
  architecture->qubitsNum = properties.qubitCount;
  architecture->sites = std::move(properties.sites);
  architecture->sitesPtr = std::move(properties.sitesPtr);
  architecture->sitesMap = std::move(properties.sitesMap);
  architecture->operations = std::move(properties.operations);
  architecture->allOperationsPtr = std::move(properties.allOperationsPtr);
  architecture->operationsPtr = std::move(properties.operationsPtr);
  architecture->supportedOperationsPtr =
      std::move(properties.supportedOperationsPtr);
  architecture->operationsMap = std::move(properties.operationsMap);
  architecture->connectivity = std::move(properties.connectivity);
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setArchitecture(session,
                                                                architecture);
  return architecture;
}

struct WaitState {
  QDMI_STATUS checkResult = QDMI_SUCCESS;
  std::array<QDMI_Job_Status, 2> checkedStatuses{QDMI_JOB_STATUS_RUNNING,
                                                 QDMI_JOB_STATUS_RUNNING};
  amazon::braket::qdmi::detail::WaitClock::duration elapsed{};
  size_t checkCalls = 0;
  size_t nowCalls = 0;
  size_t sleepCalls = 0;
};

auto makeWaitFunctions(WaitState& state)
    -> amazon::braket::qdmi::detail::JobWaitFunctions {
  return {
      .context = &state,
      .checkStatus =
          [](void* context, QDMI_Job_Status* status) {
            auto* waitState = static_cast<WaitState*>(context);
            const auto statusIndex = waitState->checkCalls == 0U ? 0U : 1U;
            ++waitState->checkCalls;
            *status = waitState->checkedStatuses[statusIndex];
            return waitState->checkResult;
          },
      .now =
          [](void* context) {
            auto* waitState = static_cast<WaitState*>(context);
            ++waitState->nowCalls;
            return amazon::braket::qdmi::detail::WaitClock::time_point{} +
                   (waitState->nowCalls == 1U
                        ? amazon::braket::qdmi::detail::WaitClock::duration{}
                        : waitState->elapsed);
          },
      .sleepFor =
          [](void* context, std::chrono::steady_clock::duration) {
            auto* waitState = static_cast<WaitState*>(context);
            ++waitState->sleepCalls;
          }};
}

TEST(AmazonBraketQDMIWaitTimeoutTest, TimeoutUsesSecondsWithoutNarrowing) {
  using amazon::braket::qdmi::detail::WaitClock;
  using amazon::braket::qdmi::detail::waitTimedOut;

  constexpr WaitClock::time_point start{};
  EXPECT_FALSE(waitTimedOut(start, start + std::chrono::milliseconds{999}, 1U));
  EXPECT_TRUE(waitTimedOut(start, start + std::chrono::seconds{1}, 1U));
  EXPECT_FALSE(waitTimedOut(start, start + std::chrono::seconds{1}, 0U));
  EXPECT_FALSE(waitTimedOut(start, start + std::chrono::seconds{1},
                            std::numeric_limits<size_t>::max()));
}

constexpr const char* BELL_STATE_PROGRAM = "OPENQASM 3.0;\n"
                                           "qubit[2] q;\n"
                                           "bit[2] c;\n"
                                           "h q[0];\n"
                                           "cnot q[0], q[1];\n"
                                           "c[0] = measure q[0];\n"
                                           "c[1] = measure q[1];\n";

auto createConfiguredJob(AMAZON_BRAKET_QDMI_Device_Session session)
    -> AMAZON_BRAKET_QDMI_Device_Job {
  AMAZON_BRAKET_QDMI_Device_Job job = nullptr;
  if (AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &job) !=
          QDMI_SUCCESS ||
      AMAZON_BRAKET_QDMI_device_job_set_parameter(
          job, QDMI_DEVICE_JOB_PARAMETER_PROGRAM,
          strlen(BELL_STATE_PROGRAM) + 1, BELL_STATE_PROGRAM) != QDMI_SUCCESS) {
    AMAZON_BRAKET_QDMI_device_job_free(job);
    return nullptr;
  }
  return job;
}

static_assert(AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN ==
              QDMI_DEVICE_SESSION_PARAMETER_BASEURL);
static_assert(AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_REGION ==
              QDMI_DEVICE_SESSION_PARAMETER_CUSTOM2);
static_assert(AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_RESERVATION_ARN ==
              QDMI_DEVICE_SESSION_PARAMETER_CUSTOM3);
static_assert(AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3URI ==
              QDMI_DEVICE_JOB_PARAMETER_CUSTOM1);
static_assert(AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_RESERVATION_ARN ==
              QDMI_DEVICE_JOB_PARAMETER_CUSTOM3);
static_assert(AMAZON_BRAKET_QDMI_DEVICE_JOB_PROPERTY_OUTPUTS3URI ==
              QDMI_DEVICE_JOB_PROPERTY_CUSTOM1);

// NOLINTBEGIN(misc-include-cleaner)
class ScopedEnvironment {
public:
  ScopedEnvironment(const char* name, const char* value) : name_(name) {
    if (const char* previous = std::getenv(name); previous != nullptr) {
      previous_ = previous;
    }
#ifdef _WIN32
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
  }

  ~ScopedEnvironment() {
#ifdef _WIN32
    // The Microsoft CRT defines an empty value as removal, so its observable
    // environment has no distinct "present but empty" state.
    _putenv_s(name_.c_str(), previous_.has_value() ? previous_->c_str() : "");
#else
    if (previous_.has_value()) {
      setenv(name_.c_str(), previous_->c_str(), 1);
    } else {
      unsetenv(name_.c_str());
    }
#endif
  }

private:
  std::string name_;
  std::optional<std::string> previous_;
};
// NOLINTEND(misc-include-cleaner)

#ifdef _WIN32
TEST(ScopedEnvironmentTest, TreatsEmptyValueAsAbsent) {
  constexpr auto* variable = "AMAZON_BRAKET_QDMI_TEST_EMPTY_ENVIRONMENT";
  const ScopedEnvironment emptyEnvironment(variable, "");
  ASSERT_EQ(std::getenv(variable), nullptr);
  {
    const ScopedEnvironment temporaryEnvironment(variable, "temporary");
    ASSERT_STREQ(std::getenv(variable), "temporary");
  }
  EXPECT_EQ(std::getenv(variable), nullptr);
}
#else
TEST(ScopedEnvironmentTest, RestoresExistingEmptyValue) {
  constexpr auto* variable = "AMAZON_BRAKET_QDMI_TEST_EMPTY_ENVIRONMENT";
  const ScopedEnvironment emptyEnvironment(variable, "");
  {
    const ScopedEnvironment temporaryEnvironment(variable, "temporary");
    ASSERT_STREQ(std::getenv(variable), "temporary");
  }
  const auto* restored = std::getenv(variable);
  ASSERT_NE(restored, nullptr);
  EXPECT_STREQ(restored, "");
}
#endif
} // namespace

TEST(QueueValueParsingTest, ParsesExactAndLowerBoundValues) {
  EXPECT_EQ(amazon::braket::qdmi::detail::parseQueueValue("0"), 0U);
  EXPECT_EQ(amazon::braket::qdmi::detail::parseQueueValue("12"), 12U);
  EXPECT_EQ(amazon::braket::qdmi::detail::parseQueueValue(">50"), 50U);
}

TEST(QueueValueParsingTest, RejectsUntrustworthyValues) {
  EXPECT_EQ(amazon::braket::qdmi::detail::parseQueueValue(""), std::nullopt);
  EXPECT_EQ(amazon::braket::qdmi::detail::parseQueueValue(">"), std::nullopt);
  EXPECT_EQ(amazon::braket::qdmi::detail::parseQueueValue("-1"), std::nullopt);
  EXPECT_EQ(amazon::braket::qdmi::detail::parseQueueValue("12 jobs"),
            std::nullopt);
}

// =============================================================================
// Fixture: allocate-only (never initialised)
// =============================================================================

/**
 * Session that has been allocated but never initialised.
 * No AWS credentials are configured and no network call is ever made.
 */
class AmazonBraketQDMIOfflineTest : public ::testing::Test {
protected:
  AMAZON_BRAKET_QDMI_Device_Session session = nullptr;

  void SetUp() override {
    ASSERT_EQ(AMAZON_BRAKET_QDMI_device_initialize(), QDMI_SUCCESS);
    ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_alloc(&session), QDMI_SUCCESS);
  }

  void TearDown() override {
    if (session != nullptr) {
      AMAZON_BRAKET_QDMI_device_session_free(session);
      session = nullptr;
    }
    AMAZON_BRAKET_QDMI_device_finalize();
  }
};

TEST_F(AmazonBraketQDMIOfflineTest, SessionInitUsesEnvironmentFallbacks) {
  const ScopedEnvironment baseUrl(
      AMAZON_BRAKET_QDMI_DEVICE_ENV_DEVICE_ARN,
      "arn:aws:braket:::device/quantum-simulator/amazon/sv1");
  const ScopedEnvironment region(AMAZON_BRAKET_QDMI_DEVICE_ENV_REGION,
                                 "us-east-1");

  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_init(session), QDMI_SUCCESS);
}

// =============================================================================
// Fixture: initialised with fake credentials (no network calls in tests)
// =============================================================================

/**
 * Session initialised with placeholder credentials.
 * init() creates the BraketClient object but makes no outbound API call.
 * Any test that calls queryDeviceProperty() would then hit AWS, so these
 * tests must only exercise local state machine and argument-validation paths.
 */
class AmazonBraketQDMILocalJobTest : public ::testing::Test {
protected:
  AMAZON_BRAKET_QDMI_Device_Session session = nullptr;
#ifdef _WIN32
  Aws::SDKOptions testAWSOptions_;
#endif

  void SetUp() override {
    ASSERT_EQ(AMAZON_BRAKET_QDMI_device_initialize(), QDMI_SUCCESS);
#ifdef _WIN32
    // The provider DLL and this test executable each contain their own
    // statically linked AWS SDK state. StubBraketClient is instantiated in the
    // executable, so initialise that copy as well.
    Aws::InitAPI(testAWSOptions_);
#endif
    ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_alloc(&session), QDMI_SUCCESS);

    const char* deviceArn =
        "arn:aws:braket:::device/quantum-simulator/amazon/sv1";
    ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                  session,
                  AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN,
                  strlen(deviceArn) + 1, deviceArn),
              QDMI_SUCCESS);

    ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_init(session), QDMI_SUCCESS);

    AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setClient(
        session, std::make_unique<StubBraketClient>(
                     Aws::Braket::Model::GetQuantumTaskResult{}));
  }

  void TearDown() override {
    if (session != nullptr) {
      AMAZON_BRAKET_QDMI_device_session_free(session);
      session = nullptr;
    }
    AMAZON_BRAKET_QDMI_device_finalize();
#ifdef _WIN32
    Aws::ShutdownAPI(testAWSOptions_);
#endif
  }
};

// =============================================================================
// AmazonBraketQDMIOfflineTest — session init() error paths
// =============================================================================

// init() without setting a device ARN must fail.
TEST_F(AmazonBraketQDMIOfflineTest, SessionInitNoDeviceArn) {
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_init(session),
            QDMI_ERROR_INVALIDARGUMENT);
}

// init() without explicit credentials constructs the AWS SDK default credential
// provider chain. Client construction is offline and must not require the chain
// to resolve credentials eagerly.
TEST_F(AmazonBraketQDMIOfflineTest,
       SessionInitUsesDefaultCredentialProviderChain) {
  const char* deviceArn =
      "arn:aws:braket:us-east-1::device/qpu/test/FakeDevice";
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                session, AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN,
                strlen(deviceArn) + 1, deviceArn),
            QDMI_SUCCESS);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_init(session), QDMI_SUCCESS);
}

TEST_F(AmazonBraketQDMIOfflineTest,
       SessionCredentialParametersAreNotSupported) {
  constexpr std::array credentialParameters{
      QDMI_DEVICE_SESSION_PARAMETER_AUTHFILE,
      QDMI_DEVICE_SESSION_PARAMETER_USERNAME,
      QDMI_DEVICE_SESSION_PARAMETER_PASSWORD,
      QDMI_DEVICE_SESSION_PARAMETER_TOKEN};
  constexpr std::string_view value = "not-used";
  for (const auto parameter : credentialParameters) {
    EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                  session, parameter, value.size() + 1, value.data()),
              QDMI_ERROR_NOTSUPPORTED);
  }
}

TEST_F(AmazonBraketQDMIOfflineTest,
       OtherStandardSessionParametersAreNotSupported) {
  constexpr std::string_view value = "not-used";
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                session, QDMI_DEVICE_SESSION_PARAMETER_AUTHURL,
                value.size() + 1, value.data()),
            QDMI_ERROR_NOTSUPPORTED);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                session, QDMI_DEVICE_SESSION_PARAMETER_CHILDDEVICE,
                value.size() + 1, value.data()),
            QDMI_ERROR_NOTSUPPORTED);
}

TEST_F(AmazonBraketQDMIOfflineTest, UnknownCustomSessionParameterIsRejected) {
  constexpr std::string_view value = "not-used";
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                session, QDMI_DEVICE_SESSION_PARAMETER_CUSTOM4,
                value.size() + 1, value.data()),
            QDMI_ERROR_INVALIDARGUMENT);
}

// =============================================================================
// AmazonBraketQDMIOfflineTest — session C-API null-pointer guards
// =============================================================================

// alloc(nullptr) must return INVALIDARGUMENT.
TEST_F(AmazonBraketQDMIOfflineTest, SessionAllocNullptr) {
  AMAZON_BRAKET_QDMI_Device_Session* nullSessionOut = nullptr;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_alloc(nullSessionOut),
            QDMI_ERROR_INVALIDARGUMENT);
}

// init(nullptr) must return INVALIDARGUMENT.
TEST_F(AmazonBraketQDMIOfflineTest, SessionInitNullptr) {
  AMAZON_BRAKET_QDMI_Device_Session nullSession = nullptr;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_init(nullSession),
            QDMI_ERROR_INVALIDARGUMENT);
}

// job API functions with a null job handle must all return INVALIDARGUMENT.
TEST_F(AmazonBraketQDMIOfflineTest, JobSetParameterNullptr) {
  AMAZON_BRAKET_QDMI_Device_Job nullJob = nullptr;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                nullJob, QDMI_DEVICE_JOB_PARAMETER_MAX, 0, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AmazonBraketQDMIOfflineTest, JobQueryPropertyNullptr) {
  AMAZON_BRAKET_QDMI_Device_Job nullJob = nullptr;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_query_property(
                nullJob, QDMI_DEVICE_JOB_PROPERTY_MAX, 0, nullptr, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AmazonBraketQDMIOfflineTest, JobSubmitNullptr) {
  AMAZON_BRAKET_QDMI_Device_Job nullJob = nullptr;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_submit(nullJob),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AmazonBraketQDMIOfflineTest, JobCancelNullptr) {
  AMAZON_BRAKET_QDMI_Device_Job nullJob = nullptr;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_cancel(nullJob),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AmazonBraketQDMIOfflineTest, JobCheckNullptr) {
  AMAZON_BRAKET_QDMI_Device_Job nullJob = nullptr;
  QDMI_Job_Status* nullStatusOut = nullptr;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_check(nullJob, nullStatusOut),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AmazonBraketQDMIOfflineTest, JobWaitNullptr) {
  AMAZON_BRAKET_QDMI_Device_Job nullJob = nullptr;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_wait(nullJob, 0),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AmazonBraketQDMIOfflineTest, JobGetResultsNullptr) {
  AMAZON_BRAKET_QDMI_Device_Job nullJob = nullptr;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_get_results(
                nullJob, QDMI_JOB_RESULT_MAX, 0, nullptr, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
}

// =============================================================================
// AmazonBraketQDMIOfflineTest — setParameter() null-termination guards
// =============================================================================

// Each string parameter requires a null-terminated value; the library checks
// via memchr(). A byte array without a null terminator triggers
// INVALIDARGUMENT.

TEST_F(AmazonBraketQDMIOfflineTest,
       SessionSetParameterDeviceArnNotNullTerminated) {
  const std::array<char, 7> notTerminated = {'a', 'r', 'n', ':', 'a', 'w', 's'};
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                session, AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN,
                notTerminated.size(), notTerminated.data()),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AmazonBraketQDMIOfflineTest,
       SessionSetParameterRegionNotNullTerminated) {
  const std::array<char, 7> notTerminated = {'u', 's', '-', 'e', 'a', 's', 't'};
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                session, AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_REGION,
                notTerminated.size(), notTerminated.data()),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AmazonBraketQDMIOfflineTest,
       SessionSetParameterReservationArnNotNullTerminated) {
  const std::array<char, 7> notTerminated = {'a', 'r', 'n', ':', 'a', 'w', 's'};
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                session,
                AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_RESERVATION_ARN,
                notTerminated.size(), notTerminated.data()),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AmazonBraketQDMIOfflineTest,
       SessionSetParameterRejectsInconsistentPointerAndSize) {
  constexpr std::array<QDMI_Device_Session_Parameter, 3> parameters{
      AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN,
      AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_REGION,
      AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_RESERVATION_ARN};
  constexpr char value = 'x';
  for (const auto parameter : parameters) {
    SCOPED_TRACE(static_cast<int>(parameter));
    EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                  session, parameter, 0, &value),
              QDMI_ERROR_INVALIDARGUMENT);
    EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                  session, parameter, 1, nullptr),
              QDMI_ERROR_INVALIDARGUMENT);
  }
}

TEST_F(AmazonBraketQDMIOfflineTest,
       SessionSetParameterRejectsEmbeddedNullBytes) {
  constexpr std::array<QDMI_Device_Session_Parameter, 3> parameters{
      AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN,
      AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_REGION,
      AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_RESERVATION_ARN};
  constexpr std::array value{'x', '\0', 'y', '\0'};
  for (const auto parameter : parameters) {
    SCOPED_TRACE(static_cast<int>(parameter));
    EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                  session, parameter, value.size(), value.data()),
              QDMI_ERROR_INVALIDARGUMENT);
  }
}

// Region is optional and must be accepted without error when valid.
TEST_F(AmazonBraketQDMIOfflineTest, SessionSetParameterRegionValid) {
  const char* region = "eu-north-1";
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                session, AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_REGION,
                strlen(region) + 1, region),
            QDMI_SUCCESS);
}

TEST_F(AmazonBraketQDMIOfflineTest, SessionSetParameterReservationArnValid) {
  const char* reservationArn =
      "arn:aws:braket:us-east-1:123456789012:reservation/"
      "a1b2c3d4-5678-90ab-cdef-1234567890ab";
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                session,
                AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_RESERVATION_ARN,
                strlen(reservationArn) + 1, reservationArn),
            QDMI_SUCCESS);
}

// =============================================================================
// AmazonBraketQDMILocalJobTest — session-level state and parameter validation
// =============================================================================

// Re-calling init() on an already-initialised session must return BADSTATE.
TEST_F(AmazonBraketQDMILocalJobTest, SessionInitBadState) {
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_init(session),
            QDMI_ERROR_BADSTATE);
}

TEST_F(AmazonBraketQDMIOfflineTest,
       SiteAndOperationQueriesRequireInitializedSession) {
  AMAZON_BRAKET_QDMI_Site_impl_d site;
  AMAZON_BRAKET_QDMI_Operation_impl_d operation;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_query_site_property(
                session, &site, QDMI_SITE_PROPERTY_INDEX, 0, nullptr, nullptr),
            QDMI_ERROR_BADSTATE);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_query_operation_property(
                session, &operation, 0, nullptr, 0, nullptr,
                QDMI_OPERATION_PROPERTY_NAME, 0, nullptr, nullptr),
            QDMI_ERROR_BADSTATE);
}

TEST_F(AmazonBraketQDMILocalJobTest,
       SiteAndOperationQueriesRejectForeignHandles) {
  auto architecture =
      std::make_shared<amazon::braket::qdmi::DeviceArchitecture>();
  auto ownedSite = std::make_unique<AMAZON_BRAKET_QDMI_Site_impl_d>();
  ownedSite->name_ = "0";
  architecture->sitesPtr.push_back(ownedSite.get());
  architecture->sites.push_back(std::move(ownedSite));
  auto ownedOperation = std::make_unique<AMAZON_BRAKET_QDMI_Operation_impl_d>();
  ownedOperation->name_ = "x";
  architecture->allOperationsPtr.push_back(ownedOperation.get());
  architecture->operationsPtr.push_back(ownedOperation.get());
  architecture->operations.push_back(std::move(ownedOperation));
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setArchitecture(session,
                                                                architecture);

  AMAZON_BRAKET_QDMI_Site_impl_d foreignSite;
  AMAZON_BRAKET_QDMI_Operation_impl_d foreignOperation;
  EXPECT_EQ(
      AMAZON_BRAKET_QDMI_device_session_query_site_property(
          session, &foreignSite, QDMI_SITE_PROPERTY_INDEX, 0, nullptr, nullptr),
      QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_query_operation_property(
                session, &foreignOperation, 0, nullptr, 0, nullptr,
                QDMI_OPERATION_PROPERTY_NAME, 0, nullptr, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);

  AMAZON_BRAKET_QDMI_Site foreignSiteHandle = &foreignSite;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_query_operation_property(
                session, architecture->operationsPtr.front(), 1,
                &foreignSiteHandle, 0, nullptr, QDMI_OPERATION_PROPERTY_NAME, 0,
                nullptr, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_query_site_property(
                session, architecture->sitesPtr.front(),
                QDMI_SITE_PROPERTY_INDEX, 0, nullptr, nullptr),
            QDMI_SUCCESS);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_query_operation_property(
                session, architecture->operationsPtr.front(), 1,
                architecture->sitesPtr.data(), 0, nullptr,
                QDMI_OPERATION_PROPERTY_NAME, 0, nullptr, nullptr),
            QDMI_SUCCESS);
}

// Calling set_parameter() after init() returns BADSTATE; unsupported parameters
// return NOTSUPPORTED or INVALIDARGUMENT on an uninitialized session.
TEST_F(AmazonBraketQDMILocalJobTest, SessionSetParameter) {
  AMAZON_BRAKET_QDMI_Device_Session uninitializedSession = nullptr;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_alloc(&uninitializedSession),
            QDMI_SUCCESS);
  EXPECT_THAT(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                  uninitializedSession,
                  AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN, 20,
                  "https://example.com"),
              testing::AnyOf(QDMI_SUCCESS, QDMI_ERROR_NOTSUPPORTED,
                             QDMI_ERROR_INVALIDARGUMENT));
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                session, AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN,
                20, "https://example.com"),
            QDMI_ERROR_BADSTATE);
  const char* reservationArn =
      "arn:aws:braket:us-east-1:123456789012:reservation/"
      "a1b2c3d4-5678-90ab-cdef-1234567890ab";
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                session,
                AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_RESERVATION_ARN,
                strlen(reservationArn) + 1, reservationArn),
            QDMI_ERROR_BADSTATE);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                session, QDMI_DEVICE_SESSION_PARAMETER_MAX, 0, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
  AMAZON_BRAKET_QDMI_device_session_free(uninitializedSession);
}

TEST_F(AmazonBraketQDMILocalJobTest,
       SessionParameterCapabilityProbesDoNotMutateState) {
  AMAZON_BRAKET_QDMI_Device_Session uninitializedSession = nullptr;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_alloc(&uninitializedSession),
            QDMI_SUCCESS);
  for (const auto parameter :
       {AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN,
        AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_REGION,
        AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_RESERVATION_ARN}) {
    EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                  uninitializedSession, parameter, 0, nullptr),
              QDMI_SUCCESS);
  }
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_set_parameter(
                uninitializedSession, QDMI_DEVICE_SESSION_PARAMETER_AUTHFILE, 0,
                nullptr),
            QDMI_ERROR_NOTSUPPORTED);
  AMAZON_BRAKET_QDMI_device_session_free(uninitializedSession);
}

// =============================================================================
// AmazonBraketQDMILocalJobTest — job creation argument validation
// =============================================================================

TEST_F(AmazonBraketQDMILocalJobTest, JobCreate) {
  AMAZON_BRAKET_QDMI_Device_Session uninitializedSession = nullptr;
  AMAZON_BRAKET_QDMI_Device_Job job = nullptr;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_alloc(&uninitializedSession),
            QDMI_SUCCESS);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_create_device_job(
                uninitializedSession, &job),
            QDMI_ERROR_BADSTATE);
  EXPECT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, nullptr),
      QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_create_device_job(nullptr, &job),
            QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &job),
            QDMI_SUCCESS);
  AMAZON_BRAKET_QDMI_device_job_free(job);
  AMAZON_BRAKET_QDMI_device_session_free(uninitializedSession);
}

TEST_F(AmazonBraketQDMILocalJobTest, JobRetrieveValidation) {
  AMAZON_BRAKET_QDMI_Device_Job job = nullptr;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_retrieve_device_job_by_id(
                nullptr, "task-arn", &job),
            QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_retrieve_device_job_by_id(
                session, nullptr, &job),
            QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_retrieve_device_job_by_id(
                session, "", &job),
            QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_retrieve_device_job_by_id(
                session, "task-arn", nullptr),
            QDMI_ERROR_INVALIDARGUMENT);

  AMAZON_BRAKET_QDMI_Device_Session uninitializedSession = nullptr;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_alloc(&uninitializedSession),
            QDMI_SUCCESS);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_retrieve_device_job_by_id(
                uninitializedSession, "task-arn", &job),
            QDMI_ERROR_BADSTATE);
  AMAZON_BRAKET_QDMI_device_session_free(uninitializedSession);
}

TEST_F(AmazonBraketQDMILocalJobTest, JobRetrieveExistingQuantumTask) {
  constexpr auto* taskArn =
      "arn:aws:braket:us-east-1:123456789012:quantum-task/task-id";
  Aws::Braket::Model::GetQuantumTaskResult task;
  task.WithQuantumTaskArn(taskArn)
      .WithDeviceArn("arn:aws:braket:::device/quantum-simulator/amazon/sv1")
      .WithStatus(Aws::Braket::Model::QuantumTaskStatus::COMPLETED)
      .WithShots(42)
      .WithOutputS3Bucket("results")
      .WithOutputS3Directory("tasks/task-id");
  auto client = std::make_unique<StubBraketClient>(std::move(task));
  const auto* clientPtr = client.get();
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setClient(session,
                                                          std::move(client));

  AMAZON_BRAKET_QDMI_Device_Job job = nullptr;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_retrieve_device_job_by_id(
                session, taskArn, &job),
            QDMI_SUCCESS);
  ASSERT_NE(job, nullptr);
  EXPECT_EQ(clientPtr->calls(), 1U);
  EXPECT_EQ(clientPtr->requestedArn(), taskArn);

  size_t idSize = 0;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_job_query_property(
                job, QDMI_DEVICE_JOB_PROPERTY_ID, 0, nullptr, &idSize),
            QDMI_SUCCESS);
  std::string id(idSize - 1, '\0');
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_query_property(
                job, QDMI_DEVICE_JOB_PROPERTY_ID, idSize, id.data(), nullptr),
            QDMI_SUCCESS);
  EXPECT_EQ(id, taskArn);

  size_t shots = 0;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_query_property(
                job, QDMI_DEVICE_JOB_PROPERTY_SHOTSNUM, sizeof(shots), &shots,
                nullptr),
            QDMI_SUCCESS);
  EXPECT_EQ(shots, 42U);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_query_property(
                job, QDMI_DEVICE_JOB_PROPERTY_PROGRAM, 0, nullptr, nullptr),
            QDMI_ERROR_NOTSUPPORTED);

  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                job, QDMI_DEVICE_JOB_PARAMETER_SHOTSNUM, sizeof(shots), &shots),
            QDMI_ERROR_BADSTATE);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_submit(job), QDMI_ERROR_BADSTATE);
  QDMI_Job_Status status = QDMI_JOB_STATUS_CREATED;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_check(job, &status), QDMI_SUCCESS);
  EXPECT_EQ(status, QDMI_JOB_STATUS_DONE);
  EXPECT_EQ(clientPtr->calls(), 1U);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_Device_Job_TestAccess::outputLocation(job),
            (std::pair<std::string, std::string>{"results", "tasks/task-id"}));

  AMAZON_BRAKET_QDMI_device_job_free(job);
}

TEST_F(AmazonBraketQDMILocalJobTest,
       JobRetrieveMapsRemotePreExecutionStatesToSubmitted) {
  constexpr auto* taskArn =
      "arn:aws:braket:us-east-1:123456789012:quantum-task/task-id";
  constexpr std::array remoteStatuses{
      Aws::Braket::Model::QuantumTaskStatus::CREATED,
      Aws::Braket::Model::QuantumTaskStatus::CANCELLING,
  };

  for (const auto remoteStatus : remoteStatuses) {
    Aws::Braket::Model::GetQuantumTaskResult task;
    task.WithQuantumTaskArn(taskArn)
        .WithDeviceArn("arn:aws:braket:::device/quantum-simulator/amazon/sv1")
        .WithStatus(remoteStatus)
        .WithShots(42);
    auto client = std::make_unique<StubBraketClient>(std::move(task));
    const auto* clientPtr = client.get();
    AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setClient(session,
                                                            std::move(client));

    AMAZON_BRAKET_QDMI_Device_Job job = nullptr;
    ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_retrieve_device_job_by_id(
                  session, taskArn, &job),
              QDMI_SUCCESS);
    QDMI_Job_Status status = QDMI_JOB_STATUS_CREATED;
    EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_check(job, &status), QDMI_SUCCESS);
    EXPECT_EQ(status, QDMI_JOB_STATUS_SUBMITTED);
    EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_cancel(job), QDMI_SUCCESS);
    EXPECT_EQ(clientPtr->cancelCalls(), 1U);
    EXPECT_EQ(clientPtr->canceledArn(), taskArn);
    AMAZON_BRAKET_QDMI_device_job_free(job);
  }
}

TEST_F(AmazonBraketQDMILocalJobTest, JobRetrieveMapsRemoteTaskStatuses) {
  constexpr auto* taskArn =
      "arn:aws:braket:us-east-1:123456789012:quantum-task/task-id";
  constexpr std::array statusMappings{
      std::pair{Aws::Braket::Model::QuantumTaskStatus::QUEUED,
                QDMI_JOB_STATUS_QUEUED},
      std::pair{Aws::Braket::Model::QuantumTaskStatus::RUNNING,
                QDMI_JOB_STATUS_RUNNING},
      std::pair{Aws::Braket::Model::QuantumTaskStatus::FAILED,
                QDMI_JOB_STATUS_FAILED},
      std::pair{Aws::Braket::Model::QuantumTaskStatus::CANCELLED,
                QDMI_JOB_STATUS_CANCELED},
  };

  for (const auto& [remoteStatus, expectedStatus] : statusMappings) {
    Aws::Braket::Model::GetQuantumTaskResult task;
    task.WithQuantumTaskArn(taskArn)
        .WithDeviceArn("arn:aws:braket:::device/quantum-simulator/amazon/sv1")
        .WithStatus(remoteStatus)
        .WithShots(42);
    AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setClient(
        session, std::make_unique<StubBraketClient>(std::move(task)));

    AMAZON_BRAKET_QDMI_Device_Job job = nullptr;
    ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_retrieve_device_job_by_id(
                  session, taskArn, &job),
              QDMI_SUCCESS);
    QDMI_Job_Status status = QDMI_JOB_STATUS_CREATED;
    EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_check(job, &status), QDMI_SUCCESS);
    EXPECT_EQ(status, expectedStatus);
    AMAZON_BRAKET_QDMI_device_job_free(job);
  }
}

TEST_F(AmazonBraketQDMILocalJobTest, JobRetrieveMapsAwsErrors) {
  const std::array errorMappings{
      std::pair{Aws::Braket::BraketErrors::RESOURCE_NOT_FOUND,
                QDMI_ERROR_NOTFOUND},
      std::pair{Aws::Braket::BraketErrors::VALIDATION,
                QDMI_ERROR_INVALIDARGUMENT},
      std::pair{Aws::Braket::BraketErrors::ACCESS_DENIED,
                QDMI_ERROR_PERMISSIONDENIED},
      std::pair{Aws::Braket::BraketErrors::UNRECOGNIZED_CLIENT,
                QDMI_ERROR_PERMISSIONDENIED},
      std::pair{Aws::Braket::BraketErrors::INTERNAL_FAILURE, QDMI_ERROR_FATAL},
  };

  for (const auto& [error, expected] : errorMappings) {
    auto client = std::make_unique<StubBraketClient>(
        Aws::Braket::Model::GetQuantumTaskResult{}, error);
    AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setClient(session,
                                                            std::move(client));
    AMAZON_BRAKET_QDMI_Device_Job job = nullptr;
    EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_retrieve_device_job_by_id(
                  session, "task-arn", &job),
              expected);
    EXPECT_EQ(job, nullptr);
  }
}

TEST_F(AmazonBraketQDMILocalJobTest, JobRetrieveRejectsInvalidRemoteMetadata) {
  constexpr auto* taskArn =
      "arn:aws:braket:us-east-1:123456789012:quantum-task/task-id";
  constexpr std::array invalidTasks{
      std::pair{Aws::Braket::Model::QuantumTaskStatus::RUNNING, -1LL},
      std::pair{Aws::Braket::Model::QuantumTaskStatus::NOT_SET, 42LL},
  };

  for (const auto& [remoteStatus, shots] : invalidTasks) {
    Aws::Braket::Model::GetQuantumTaskResult task;
    task.WithQuantumTaskArn(taskArn)
        .WithDeviceArn("arn:aws:braket:::device/quantum-simulator/amazon/sv1")
        .WithStatus(remoteStatus)
        .WithShots(shots);
    AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setClient(
        session, std::make_unique<StubBraketClient>(std::move(task)));

    AMAZON_BRAKET_QDMI_Device_Job job = nullptr;
    EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_retrieve_device_job_by_id(
                  session, taskArn, &job),
              QDMI_ERROR_FATAL);
    EXPECT_EQ(job, nullptr);
  }
}

TEST_F(AmazonBraketQDMILocalJobTest, JobRetrieveMapsSubsequentAwsErrors) {
  constexpr auto* taskArn =
      "arn:aws:braket:us-east-1:123456789012:quantum-task/task-id";
  const std::array errorMappings{
      std::pair{Aws::Braket::BraketErrors::ACCESS_DENIED,
                QDMI_ERROR_PERMISSIONDENIED},
      std::pair{Aws::Braket::BraketErrors::INTERNAL_FAILURE, QDMI_ERROR_FATAL},
  };

  for (const auto& [error, expected] : errorMappings) {
    Aws::Braket::Model::GetQuantumTaskResult task;
    task.WithQuantumTaskArn(taskArn)
        .WithDeviceArn("arn:aws:braket:::device/quantum-simulator/amazon/sv1")
        .WithStatus(Aws::Braket::Model::QuantumTaskStatus::CREATED)
        .WithShots(42);
    AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setClient(
        session,
        std::make_unique<StubBraketClient>(task, error, std::nullopt, true));

    AMAZON_BRAKET_QDMI_Device_Job job = nullptr;
    ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_retrieve_device_job_by_id(
                  session, taskArn, &job),
              QDMI_SUCCESS);
    QDMI_Job_Status status = QDMI_JOB_STATUS_CREATED;
    EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_check(job, &status), expected);
    AMAZON_BRAKET_QDMI_device_job_free(job);

    AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setClient(
        session, std::make_unique<StubBraketClient>(std::move(task),
                                                    std::nullopt, error));
    ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_retrieve_device_job_by_id(
                  session, taskArn, &job),
              QDMI_SUCCESS);
    EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_cancel(job), expected);
    AMAZON_BRAKET_QDMI_device_job_free(job);
  }
}

TEST_F(AmazonBraketQDMILocalJobTest,
       JobRetrieveRejectsQuantumTaskForDifferentDevice) {
  Aws::Braket::Model::GetQuantumTaskResult task;
  task.WithQuantumTaskArn("task-arn")
      .WithDeviceArn("different-device")
      .WithStatus(Aws::Braket::Model::QuantumTaskStatus::RUNNING)
      .WithShots(100);
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setClient(
      session, std::make_unique<StubBraketClient>(std::move(task)));

  AMAZON_BRAKET_QDMI_Device_Job job = nullptr;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_retrieve_device_job_by_id(
                session, "task-arn", &job),
            QDMI_ERROR_NOTFOUND);
  EXPECT_EQ(job, nullptr);
}

// =============================================================================
// AmazonBraketQDMILocalJobTest — job setParameter() validation
// =============================================================================

TEST_F(AmazonBraketQDMILocalJobTest, JobSetParameterMaxReturnsInvalidArgument) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  QDMI_Program_Format value = QDMI_PROGRAM_FORMAT_QASM3;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                freshJob, QDMI_DEVICE_JOB_PARAMETER_PROGRAMFORMAT,
                sizeof(value), &value),
            QDMI_SUCCESS);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                freshJob, QDMI_DEVICE_JOB_PARAMETER_MAX, 0, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

TEST_F(AmazonBraketQDMILocalJobTest, JobSetParameterProgram) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                freshJob, QDMI_DEVICE_JOB_PARAMETER_PROGRAM,
                strlen(BELL_STATE_PROGRAM) + 1, BELL_STATE_PROGRAM),
            QDMI_SUCCESS);
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

TEST_F(AmazonBraketQDMILocalJobTest, JobSetParameterS3Uri) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  const char* s3Uri = "s3://test-job-specific-results-bucket/tasks/run-42";
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                freshJob, AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3URI,
                strlen(s3Uri) + 1, s3Uri),
            QDMI_SUCCESS);
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

TEST_F(AmazonBraketQDMILocalJobTest, JobSetParameterRejectsInvalidS3Uris) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  constexpr std::array invalidUris{"bucket/prefix", "s3://bucket",
                                   "s3:///prefix", "s3://bucket/"};
  for (const auto* uri : invalidUris) {
    EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                  freshJob, AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3URI,
                  strlen(uri) + 1, uri),
              QDMI_ERROR_INVALIDARGUMENT);
  }
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

TEST_F(AmazonBraketQDMILocalJobTest, JobSetParameterReservationArn) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  const char* reservationArn =
      "arn:aws:braket:us-east-1:123456789012:reservation/"
      "a1b2c3d4-5678-90ab-cdef-1234567890ab";
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                freshJob,
                AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_RESERVATION_ARN,
                strlen(reservationArn) + 1, reservationArn),
            QDMI_SUCCESS);
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

TEST_F(AmazonBraketQDMILocalJobTest,
       JobParameterCapabilityProbesDoNotMutateState) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  for (const auto parameter :
       {QDMI_DEVICE_JOB_PARAMETER_PROGRAM,
        QDMI_DEVICE_JOB_PARAMETER_PROGRAMFORMAT,
        QDMI_DEVICE_JOB_PARAMETER_SHOTSNUM,
        AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3URI,
        AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_RESERVATION_ARN}) {
    EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(freshJob, parameter,
                                                          0, nullptr),
              QDMI_SUCCESS);
  }
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

// SHOTSNUM with wrong size → INVALIDARGUMENT (must equal sizeof(size_t)).
TEST_F(AmazonBraketQDMILocalJobTest, JobSetParameterShotsWrongSize) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  size_t shots = 100;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                freshJob, QDMI_DEVICE_JOB_PARAMETER_SHOTSNUM, 1, &shots),
            QDMI_ERROR_INVALIDARGUMENT);
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

// An unsupported program format value must be rejected with NOTSUPPORTED.
TEST_F(AmazonBraketQDMILocalJobTest, JobSetParameterUnsupportedProgramFormat) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  const auto unsupported = static_cast<QDMI_Program_Format>(0xFF);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                freshJob, QDMI_DEVICE_JOB_PARAMETER_PROGRAMFORMAT,
                sizeof(unsupported), &unsupported),
            QDMI_ERROR_NOTSUPPORTED);
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

// QASM2 is a valid program format and must be accepted.
TEST_F(AmazonBraketQDMILocalJobTest, JobSetParameterQASM2Format) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  const QDMI_Program_Format fmt = QDMI_PROGRAM_FORMAT_QASM2;
  EXPECT_EQ(
      AMAZON_BRAKET_QDMI_device_job_set_parameter(
          freshJob, QDMI_DEVICE_JOB_PARAMETER_PROGRAMFORMAT, sizeof(fmt), &fmt),
      QDMI_SUCCESS);
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

// value != nullptr with size == 0 must return INVALIDARGUMENT.
TEST_F(AmazonBraketQDMILocalJobTest, JobSetParameterValueNonNullZeroSize) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  const QDMI_Program_Format fmt = QDMI_PROGRAM_FORMAT_QASM3;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                freshJob, QDMI_DEVICE_JOB_PARAMETER_PROGRAMFORMAT,
                /*size=*/0, &fmt),
            QDMI_ERROR_INVALIDARGUMENT);
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

TEST_F(AmazonBraketQDMILocalJobTest,
       JobSetParameterRejectsInconsistentPointerAndSize) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  constexpr std::array<QDMI_Device_Job_Parameter, 5> parameters{
      QDMI_DEVICE_JOB_PARAMETER_PROGRAM,
      QDMI_DEVICE_JOB_PARAMETER_PROGRAMFORMAT,
      QDMI_DEVICE_JOB_PARAMETER_SHOTSNUM,
      AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3URI,
      AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_RESERVATION_ARN};
  constexpr char value = 'x';
  for (const auto parameter : parameters) {
    SCOPED_TRACE(static_cast<int>(parameter));
    EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(freshJob, parameter,
                                                          0, &value),
              QDMI_ERROR_INVALIDARGUMENT);
    EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(freshJob, parameter,
                                                          1, nullptr),
              QDMI_ERROR_INVALIDARGUMENT);
  }
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

TEST_F(AmazonBraketQDMILocalJobTest, JobSetParameterRejectsEmbeddedNullBytes) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  constexpr std::array value{'x', '\0', 'y', '\0'};
  for (const auto parameter :
       {QDMI_DEVICE_JOB_PARAMETER_PROGRAM,
        AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3URI,
        AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_RESERVATION_ARN}) {
    SCOPED_TRACE(static_cast<int>(parameter));
    EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                  freshJob, parameter, value.size(), value.data()),
              QDMI_ERROR_INVALIDARGUMENT);
  }
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

// Custom string parameters must have a null-terminator at buf[size-1].

TEST_F(AmazonBraketQDMILocalJobTest, JobSetParameterS3UriNotNullTerminated) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  const std::array<char, 11> notTerminated = {'s', '3', ':', '/', '/', 'b',
                                              '/', 'p', 'r', 'e', 'f'};
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                freshJob, AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3URI,
                notTerminated.size(), notTerminated.data()),
            QDMI_ERROR_INVALIDARGUMENT);
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

TEST_F(AmazonBraketQDMILocalJobTest,
       JobSetParameterReservationArnNotNullTerminated) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  const std::array<char, 7> notTerminated = {'a', 'r', 'n', ':', 'a', 'w', 's'};
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                freshJob,
                AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_RESERVATION_ARN,
                notTerminated.size(), notTerminated.data()),
            QDMI_ERROR_INVALIDARGUMENT);
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

// =============================================================================
// AmazonBraketQDMILocalJobTest — job queryProperty()
// =============================================================================

// The durable job ID is the AWS QuantumTask ARN and therefore only becomes
// available after successful submission.
TEST_F(AmazonBraketQDMILocalJobTest, JobIdUnavailableBeforeSubmission) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  size_t idSize = 0;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_query_property(
                freshJob, QDMI_DEVICE_JOB_PROPERTY_ID, 0, nullptr, &idSize),
            QDMI_ERROR_NOTSUPPORTED);
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

// Two-step (size then value) query of the PROGRAM property.
TEST_F(AmazonBraketQDMILocalJobTest, JobQueryPropertyProgram) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                freshJob, QDMI_DEVICE_JOB_PARAMETER_PROGRAM,
                strlen(BELL_STATE_PROGRAM) + 1, BELL_STATE_PROGRAM),
            QDMI_SUCCESS);

  size_t programSize = 0;
  EXPECT_EQ(
      AMAZON_BRAKET_QDMI_device_job_query_property(
          freshJob, QDMI_DEVICE_JOB_PROPERTY_PROGRAM, 0, nullptr, &programSize),
      QDMI_SUCCESS);
  ASSERT_GT(programSize, 0U);

  std::string programBuf(programSize - 1, '\0');
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_query_property(
                freshJob, QDMI_DEVICE_JOB_PROPERTY_PROGRAM, programSize,
                programBuf.data(), nullptr),
            QDMI_SUCCESS);
  EXPECT_EQ(programBuf, BELL_STATE_PROGRAM);

  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

// PROGRAMFORMAT defaults to QASM3.
TEST_F(AmazonBraketQDMILocalJobTest, JobQueryPropertyProgramFormat) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  QDMI_Program_Format fmt = QDMI_PROGRAM_FORMAT_QASM2; // sentinel, overwritten
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_query_property(
                freshJob, QDMI_DEVICE_JOB_PROPERTY_PROGRAMFORMAT,
                sizeof(QDMI_Program_Format), &fmt, nullptr),
            QDMI_SUCCESS);
  EXPECT_EQ(fmt, QDMI_PROGRAM_FORMAT_QASM3);
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

// SHOTSNUM defaults to 100.
TEST_F(AmazonBraketQDMILocalJobTest, JobQueryPropertyShotsNum) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  size_t shots = 0;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_query_property(
                freshJob, QDMI_DEVICE_JOB_PROPERTY_SHOTSNUM, sizeof(size_t),
                &shots, nullptr),
            QDMI_SUCCESS);
  EXPECT_EQ(shots, 100U);
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

TEST_F(AmazonBraketQDMILocalJobTest, JobQueryPropertyOutputS3Uri) {
  constexpr auto* taskArn =
      "arn:aws:braket:us-east-1:123456789012:quantum-task/task-id";
  Aws::Braket::Model::GetQuantumTaskResult task;
  task.WithQuantumTaskArn(taskArn)
      .WithDeviceArn("arn:aws:braket:::device/quantum-simulator/amazon/sv1")
      .WithStatus(Aws::Braket::Model::QuantumTaskStatus::RUNNING)
      .WithShots(42)
      .WithOutputS3Bucket("results")
      .WithOutputS3Directory("tasks/task-id");
  auto client = std::make_unique<StubBraketClient>(std::move(task));
  const auto* clientPtr = client.get();
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setClient(session,
                                                          std::move(client));

  AMAZON_BRAKET_QDMI_Device_Job job = nullptr;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_retrieve_device_job_by_id(
                session, taskArn, &job),
            QDMI_SUCCESS);
  ASSERT_NE(job, nullptr);

  size_t uriSize = 0;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_job_query_property(
                job, AMAZON_BRAKET_QDMI_DEVICE_JOB_PROPERTY_OUTPUTS3URI, 0,
                nullptr, &uriSize),
            QDMI_SUCCESS);
  ASSERT_GT(uriSize, 0U);

  std::string uri(uriSize - 1, '\0');
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_query_property(
                job, AMAZON_BRAKET_QDMI_DEVICE_JOB_PROPERTY_OUTPUTS3URI,
                uriSize, uri.data(), nullptr),
            QDMI_SUCCESS);
  EXPECT_EQ(uri, "s3://results/tasks/task-id");
  EXPECT_EQ(clientPtr->calls(), 3U);
  AMAZON_BRAKET_QDMI_device_job_free(job);
}

TEST_F(AmazonBraketQDMILocalJobTest,
       JobQueryPropertyOutputS3UriRequiresResolvedLocation) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  size_t uriSize = 0;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_query_property(
                freshJob, AMAZON_BRAKET_QDMI_DEVICE_JOB_PROPERTY_OUTPUTS3URI, 0,
                nullptr, &uriSize),
            QDMI_ERROR_BADSTATE);
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

TEST_F(AmazonBraketQDMILocalJobTest,
       JobQueuePositionRequiresSubmittedQueuedTask) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  size_t queuePosition = 0;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_query_property(
                freshJob, QDMI_DEVICE_JOB_PROPERTY_QUEUEPOSITION,
                sizeof(queuePosition), &queuePosition, nullptr),
            QDMI_ERROR_BADSTATE);
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

// MAX property sentinel must return INVALIDARGUMENT.
TEST_F(AmazonBraketQDMILocalJobTest, JobQueryPropertyMax) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_query_property(
                freshJob, QDMI_DEVICE_JOB_PROPERTY_MAX, 0, nullptr, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

// =============================================================================
// AmazonBraketQDMILocalJobTest — job cancel() state machine
// =============================================================================

// cancel() before submission: transitions to CANCELED locally (no AWS call).
TEST_F(AmazonBraketQDMILocalJobTest, JobCancelFromCreatedState) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_cancel(freshJob), QDMI_SUCCESS);

  QDMI_Job_Status status = QDMI_JOB_STATUS_CREATED;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_check(freshJob, &status),
            QDMI_SUCCESS);
  EXPECT_EQ(status, QDMI_JOB_STATUS_CANCELED);

  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

// cancel() on an already-CANCELED job → INVALIDARGUMENT (not QUEUED/RUNNING).
TEST_F(AmazonBraketQDMILocalJobTest, JobCancelFromCanceledState) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_job_cancel(freshJob), QDMI_SUCCESS);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_cancel(freshJob),
            QDMI_ERROR_INVALIDARGUMENT);
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

// =============================================================================
// AmazonBraketQDMILocalJobTest — job check() state machine
// =============================================================================

// check() on a fresh job (no taskArn) returns the current local status.
TEST_F(AmazonBraketQDMILocalJobTest, JobCheckNoTaskArn) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  QDMI_Job_Status status = QDMI_JOB_STATUS_DONE; // sentinel
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_check(freshJob, &status),
            QDMI_SUCCESS);
  EXPECT_EQ(status, QDMI_JOB_STATUS_CREATED);
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

// =============================================================================
// AmazonBraketQDMILocalJobTest — job wait() state machine
// =============================================================================

// wait() on a CREATED (never submitted) job → INVALIDARGUMENT.
TEST_F(AmazonBraketQDMILocalJobTest, JobWaitOnCreatedJob) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_wait(freshJob, 1),
            QDMI_ERROR_INVALIDARGUMENT);
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

// wait() on a CANCELED (terminal) job → QDMI_SUCCESS immediately.
TEST_F(AmazonBraketQDMILocalJobTest, JobWaitOnCanceledJob) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_job_cancel(freshJob), QDMI_SUCCESS);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_wait(freshJob, 1), QDMI_SUCCESS);
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

// wait() on a FAILED (terminal) job → QDMI_SUCCESS immediately.
TEST_F(AmazonBraketQDMILocalJobTest, JobWaitOnFailedJob) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  AMAZON_BRAKET_QDMI_Device_Job_TestAccess::setStatus(freshJob,
                                                      QDMI_JOB_STATUS_FAILED);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_wait(freshJob, 1), QDMI_SUCCESS);
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

TEST_F(AmazonBraketQDMILocalJobTest, JobWaitTimesOutDeterministically) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);

  WaitState state;
  state.elapsed = std::chrono::seconds{1};
  const auto functions = makeWaitFunctions(state);

  EXPECT_EQ(
      AMAZON_BRAKET_QDMI_Device_Job_TestAccess::wait(freshJob, 1U, functions),
      QDMI_ERROR_TIMEOUT);
  EXPECT_EQ(state.checkCalls, 1U);
  EXPECT_EQ(state.nowCalls, 2U);
  EXPECT_EQ(state.sleepCalls, 0U);
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

TEST_F(AmazonBraketQDMILocalJobTest, JobWaitCompletesAfterPolling) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);

  WaitState state;
  state.checkedStatuses[1] = QDMI_JOB_STATUS_DONE;
  const auto functions = makeWaitFunctions(state);

  EXPECT_EQ(
      AMAZON_BRAKET_QDMI_Device_Job_TestAccess::wait(freshJob, 1U, functions),
      QDMI_SUCCESS);
  EXPECT_EQ(state.checkCalls, 2U);
  EXPECT_EQ(state.nowCalls, 2U);
  EXPECT_EQ(state.sleepCalls, 1U);
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

TEST_F(AmazonBraketQDMILocalJobTest, JobWaitPropagatesCheckFailure) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);

  WaitState state;
  state.checkResult = QDMI_ERROR_NOTSUPPORTED;
  const auto functions = makeWaitFunctions(state);

  EXPECT_EQ(
      AMAZON_BRAKET_QDMI_Device_Job_TestAccess::wait(freshJob, 1U, functions),
      QDMI_ERROR_NOTSUPPORTED);
  EXPECT_EQ(state.checkCalls, 1U);
  EXPECT_EQ(state.nowCalls, 1U);
  EXPECT_EQ(state.sleepCalls, 0U);
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

// =============================================================================
// AmazonBraketQDMILocalJobTest — job submit() local error paths
// =============================================================================

// submit() without a program set → INVALIDARGUMENT.
TEST_F(AmazonBraketQDMILocalJobTest, JobSubmitNoProgram) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_submit(freshJob),
            QDMI_ERROR_INVALIDARGUMENT);
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

TEST_F(AmazonBraketQDMILocalJobTest,
       ExplicitS3UriAvoidsStsAndBucketManagement) {
  const ScopedEnvironment environment(
      AMAZON_BRAKET_QDMI_DEVICE_ENV_TASK_RESULTS_S3_URI,
      "s3://ignored-environment/tasks");
  auto braketClient = std::make_unique<StubBraketClient>(
      Aws::Braket::Model::GetQuantumTaskResult{});
  const auto* braket = braketClient.get();
  auto s3Client = std::make_unique<StubS3Client>();
  const auto* s3 = s3Client.get();
  auto stsClient =
      std::make_unique<StubStsClient>(Aws::STS::STSErrors::ACCESS_DENIED);
  const auto* sts = stsClient.get();
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setClient(
      session, std::move(braketClient));
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setS3Client(
      session, std::move(s3Client));
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setStsClient(
      session, std::move(stsClient));

  AMAZON_BRAKET_QDMI_Device_Job job = nullptr;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &job),
            QDMI_SUCCESS);
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                job, QDMI_DEVICE_JOB_PARAMETER_PROGRAM,
                strlen(BELL_STATE_PROGRAM) + 1, BELL_STATE_PROGRAM),
            QDMI_SUCCESS);
  constexpr auto s3Uri = "s3://explicit-results/experiments/run-42";
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                job, AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3URI,
                strlen(s3Uri) + 1, s3Uri),
            QDMI_SUCCESS);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_submit(job), QDMI_SUCCESS);
  EXPECT_EQ(sts->calls(), 0U);
  EXPECT_EQ(s3->createCalls(), 0U);
  EXPECT_EQ(braket->outputBucket(), "explicit-results");
  EXPECT_EQ(braket->outputPrefix(), "experiments/run-42");
  AMAZON_BRAKET_QDMI_device_job_free(job);
}

TEST_F(AmazonBraketQDMILocalJobTest, JobReservationIsAttachedToQuantumTask) {
  auto braketClient = std::make_unique<StubBraketClient>(
      Aws::Braket::Model::GetQuantumTaskResult{});
  const auto* braket = braketClient.get();
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setClient(
      session, std::move(braketClient));
  auto* const job = createConfiguredJob(session);
  ASSERT_NE(job, nullptr);
  constexpr std::string_view s3Uri = "s3://explicit-results/tasks";
  constexpr std::string_view reservationArn =
      "arn:aws:braket:us-east-1:123456789012:reservation/"
      "a1b2c3d4-5678-90ab-cdef-1234567890ab";
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                job, AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3URI,
                s3Uri.size() + 1, s3Uri.data()),
            QDMI_SUCCESS);
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                job, AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_RESERVATION_ARN,
                reservationArn.size() + 1, reservationArn.data()),
            QDMI_SUCCESS);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_submit(job), QDMI_SUCCESS);
  EXPECT_EQ(braket->reservationArn(), reservationArn);
  AMAZON_BRAKET_QDMI_device_job_free(job);
}

TEST_F(AmazonBraketQDMILocalJobTest,
       JobRemainsCreatedAndImmutableUntilAwsAcceptsTask) {
  Aws::Braket::Model::GetQuantumTaskResult task;
  task.WithStatus(Aws::Braket::Model::QuantumTaskStatus::CREATED);
  auto braketClient = std::make_unique<StubBraketClient>(std::move(task));
  auto* const braket = braketClient.get();
  braket->blockCreate();
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setClient(
      session, std::move(braketClient));

  auto* const job = createConfiguredJob(session);
  ASSERT_NE(job, nullptr);
  constexpr std::string_view s3Uri = "s3://explicit-results/tasks";
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                job, AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3URI,
                s3Uri.size() + 1, s3Uri.data()),
            QDMI_SUCCESS);

  auto submission = std::async(std::launch::async, [job] {
    return AMAZON_BRAKET_QDMI_device_job_submit(job);
  });
  if (!braket->waitForCreate()) {
    braket->releaseCreate();
    FAIL() << "CreateQuantumTask was not reached";
  }

  QDMI_Job_Status status = QDMI_JOB_STATUS_FAILED;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_check(job, &status), QDMI_SUCCESS);
  EXPECT_EQ(status, QDMI_JOB_STATUS_CREATED);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_cancel(job), QDMI_ERROR_BADSTATE);
  constexpr size_t shots = 100;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                job, QDMI_DEVICE_JOB_PARAMETER_SHOTSNUM, sizeof(shots), &shots),
            QDMI_ERROR_BADSTATE);

  braket->releaseCreate();
  EXPECT_EQ(submission.get(), QDMI_SUCCESS);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_check(job, &status), QDMI_SUCCESS);
  EXPECT_EQ(status, QDMI_JOB_STATUS_SUBMITTED);
  AMAZON_BRAKET_QDMI_device_job_free(job);
}

TEST_F(AmazonBraketQDMILocalJobTest, CreateQuantumTaskFailuresMapAwsErrors) {
  constexpr std::array failures{
      std::pair{Aws::Braket::BraketErrors::ACCESS_DENIED,
                QDMI_ERROR_PERMISSIONDENIED},
      std::pair{Aws::Braket::BraketErrors::VALIDATION,
                QDMI_ERROR_INVALIDARGUMENT},
      std::pair{Aws::Braket::BraketErrors::DEVICE_OFFLINE, QDMI_ERROR_BADSTATE},
      std::pair{Aws::Braket::BraketErrors::DEVICE_RETIRED, QDMI_ERROR_BADSTATE},
      std::pair{Aws::Braket::BraketErrors::CONFLICT, QDMI_ERROR_BADSTATE},
      std::pair{Aws::Braket::BraketErrors::SERVICE_UNAVAILABLE,
                QDMI_ERROR_FATAL}};
  for (const auto& [error, expected] : failures) {
    SCOPED_TRACE(static_cast<int>(error));
    auto braketClient = std::make_unique<StubBraketClient>(
        Aws::Braket::Model::GetQuantumTaskResult{}, std::nullopt, std::nullopt,
        false, error);
    AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setClient(
        session, std::move(braketClient));
    auto* const job = createConfiguredJob(session);
    ASSERT_NE(job, nullptr);
    constexpr std::string_view s3Uri = "s3://explicit-results/tasks";
    ASSERT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                  job, AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3URI,
                  s3Uri.size() + 1, s3Uri.data()),
              QDMI_SUCCESS);
    EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_submit(job), expected);
    QDMI_Job_Status status = QDMI_JOB_STATUS_CREATED;
    EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_check(job, &status), QDMI_SUCCESS);
    EXPECT_EQ(status, QDMI_JOB_STATUS_FAILED);
    AMAZON_BRAKET_QDMI_device_job_free(job);
  }
}

TEST_F(AmazonBraketQDMILocalJobTest,
       EnvironmentS3UriAvoidsStsAndBucketManagement) {
  const ScopedEnvironment environment(
      AMAZON_BRAKET_QDMI_DEVICE_ENV_TASK_RESULTS_S3_URI,
      "s3://environment-results/tasks");
  auto braketClient = std::make_unique<StubBraketClient>(
      Aws::Braket::Model::GetQuantumTaskResult{});
  const auto* braket = braketClient.get();
  auto s3Client = std::make_unique<StubS3Client>();
  const auto* s3 = s3Client.get();
  auto stsClient =
      std::make_unique<StubStsClient>(Aws::STS::STSErrors::ACCESS_DENIED);
  const auto* sts = stsClient.get();
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setClient(
      session, std::move(braketClient));
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setS3Client(
      session, std::move(s3Client));
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setStsClient(
      session, std::move(stsClient));

  AMAZON_BRAKET_QDMI_Device_Job job = nullptr;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &job),
            QDMI_SUCCESS);
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                job, QDMI_DEVICE_JOB_PARAMETER_PROGRAM,
                strlen(BELL_STATE_PROGRAM) + 1, BELL_STATE_PROGRAM),
            QDMI_SUCCESS);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_submit(job), QDMI_SUCCESS);
  EXPECT_EQ(sts->calls(), 0U);
  EXPECT_EQ(s3->createCalls(), 0U);
  EXPECT_EQ(braket->outputBucket(), "environment-results");
  EXPECT_EQ(braket->outputPrefix(), "tasks");
  AMAZON_BRAKET_QDMI_device_job_free(job);
}

TEST_F(AmazonBraketQDMILocalJobTest, DefaultS3DestinationIsCreatedAndCached) {
  const ScopedEnvironment environment(
      AMAZON_BRAKET_QDMI_DEVICE_ENV_TASK_RESULTS_S3_URI, "");
  auto braketClient = std::make_unique<StubBraketClient>(
      Aws::Braket::Model::GetQuantumTaskResult{});
  const auto* braket = braketClient.get();
  auto s3Client = std::make_unique<StubS3Client>();
  const auto* s3 = s3Client.get();
  auto stsClient = std::make_unique<StubStsClient>();
  const auto* sts = stsClient.get();
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setClient(
      session, std::move(braketClient));
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setS3Client(
      session, std::move(s3Client));
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setStsClient(
      session, std::move(stsClient));

  auto* const firstJob = createConfiguredJob(session);
  auto* const secondJob = createConfiguredJob(session);
  ASSERT_NE(firstJob, nullptr);
  ASSERT_NE(secondJob, nullptr);
  auto firstSubmission = std::async(std::launch::async, [firstJob] {
    return AMAZON_BRAKET_QDMI_device_job_submit(firstJob);
  });
  auto secondSubmission = std::async(std::launch::async, [secondJob] {
    return AMAZON_BRAKET_QDMI_device_job_submit(secondJob);
  });
  EXPECT_EQ(firstSubmission.get(), QDMI_SUCCESS);
  EXPECT_EQ(secondSubmission.get(), QDMI_SUCCESS);
  AMAZON_BRAKET_QDMI_device_job_free(firstJob);
  AMAZON_BRAKET_QDMI_device_job_free(secondJob);
  EXPECT_EQ(sts->calls(), 1U);
  EXPECT_EQ(s3->createCalls(), 1U);
  EXPECT_FALSE(s3->hasLocationConstraint());
  EXPECT_EQ(s3->bucket(), "amazon-braket-us-east-1-123456789012");
  EXPECT_EQ(braket->createCalls(), 2U);
  EXPECT_EQ(braket->outputBucket(), "amazon-braket-us-east-1-123456789012");
  EXPECT_EQ(braket->outputPrefix(), "tasks");
}

TEST_F(AmazonBraketQDMILocalJobTest,
       DefaultS3DestinationMapsStsPermissionFailure) {
  const ScopedEnvironment environment(
      AMAZON_BRAKET_QDMI_DEVICE_ENV_TASK_RESULTS_S3_URI, "");
  auto stsClient =
      std::make_unique<StubStsClient>(Aws::STS::STSErrors::ACCESS_DENIED);
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setStsClient(
      session, std::move(stsClient));
  AMAZON_BRAKET_QDMI_Device_Job job = nullptr;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &job),
            QDMI_SUCCESS);
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_job_set_parameter(
                job, QDMI_DEVICE_JOB_PARAMETER_PROGRAM,
                strlen(BELL_STATE_PROGRAM) + 1, BELL_STATE_PROGRAM),
            QDMI_SUCCESS);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_submit(job),
            QDMI_ERROR_PERMISSIONDENIED);
  AMAZON_BRAKET_QDMI_device_job_free(job);
}

TEST_F(AmazonBraketQDMILocalJobTest,
       DefaultS3DestinationRequiresStsAndS3Clients) {
  const ScopedEnvironment environment(
      AMAZON_BRAKET_QDMI_DEVICE_ENV_TASK_RESULTS_S3_URI, "");
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setStsClient(session, nullptr);
  auto* const job = createConfiguredJob(session);
  ASSERT_NE(job, nullptr);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_submit(job), QDMI_ERROR_BADSTATE);
  AMAZON_BRAKET_QDMI_device_job_free(job);

  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setStsClient(
      session, std::make_unique<StubStsClient>());
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setS3Client(session, nullptr);
  auto* const secondJob = createConfiguredJob(session);
  ASSERT_NE(secondJob, nullptr);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_submit(secondJob),
            QDMI_ERROR_BADSTATE);
  AMAZON_BRAKET_QDMI_device_job_free(secondJob);
}

TEST_F(AmazonBraketQDMILocalJobTest,
       DefaultS3DestinationRejectsEmptyAccountId) {
  const ScopedEnvironment environment(
      AMAZON_BRAKET_QDMI_DEVICE_ENV_TASK_RESULTS_S3_URI, "");
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setStsClient(
      session, std::make_unique<StubStsClient>(std::nullopt, ""));
  auto* const job = createConfiguredJob(session);
  ASSERT_NE(job, nullptr);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_submit(job), QDMI_ERROR_FATAL);
  AMAZON_BRAKET_QDMI_device_job_free(job);
}

TEST_F(AmazonBraketQDMILocalJobTest, RegionalDefaultBucketSetsLocation) {
  const ScopedEnvironment environment(
      AMAZON_BRAKET_QDMI_DEVICE_ENV_TASK_RESULTS_S3_URI, "");
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setRegion(session,
                                                          "eu-north-1");
  auto braketClient = std::make_unique<StubBraketClient>(
      Aws::Braket::Model::GetQuantumTaskResult{});
  const auto* braket = braketClient.get();
  auto s3Client = std::make_unique<StubS3Client>();
  const auto* s3 = s3Client.get();
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setClient(
      session, std::move(braketClient));
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setS3Client(
      session, std::move(s3Client));
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setStsClient(
      session, std::make_unique<StubStsClient>());

  auto* const job = createConfiguredJob(session);
  ASSERT_NE(job, nullptr);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_submit(job), QDMI_SUCCESS);
  EXPECT_TRUE(s3->hasLocationConstraint());
  EXPECT_EQ(braket->outputBucket(), "amazon-braket-eu-north-1-123456789012");
  AMAZON_BRAKET_QDMI_device_job_free(job);
}

TEST_F(AmazonBraketQDMILocalJobTest, InvalidEnvironmentS3UriIsRejected) {
  const ScopedEnvironment environment(
      AMAZON_BRAKET_QDMI_DEVICE_ENV_TASK_RESULTS_S3_URI, "s3://missing-prefix");
  auto braketClient = std::make_unique<StubBraketClient>(
      Aws::Braket::Model::GetQuantumTaskResult{});
  const auto* braket = braketClient.get();
  auto s3Client = std::make_unique<StubS3Client>();
  const auto* s3 = s3Client.get();
  auto stsClient = std::make_unique<StubStsClient>();
  const auto* sts = stsClient.get();
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setClient(
      session, std::move(braketClient));
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setS3Client(
      session, std::move(s3Client));
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setStsClient(
      session, std::move(stsClient));

  auto* const job = createConfiguredJob(session);
  ASSERT_NE(job, nullptr);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_submit(job),
            QDMI_ERROR_INVALIDARGUMENT);
  EXPECT_EQ(sts->calls(), 0U);
  EXPECT_EQ(s3->createCalls(), 0U);
  EXPECT_EQ(braket->createCalls(), 0U);
  AMAZON_BRAKET_QDMI_device_job_free(job);
}

TEST_F(AmazonBraketQDMILocalJobTest, DefaultBucketNameConflictIsDenied) {
  const ScopedEnvironment environment(
      AMAZON_BRAKET_QDMI_DEVICE_ENV_TASK_RESULTS_S3_URI, "");
  auto s3Client = std::make_unique<StubS3Client>(StubS3Client::Configuration{
      .createError = Aws::S3::S3Errors::BUCKET_ALREADY_EXISTS});
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setS3Client(
      session, std::move(s3Client));
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setStsClient(
      session, std::make_unique<StubStsClient>());
  auto* const job = createConfiguredJob(session);
  ASSERT_NE(job, nullptr);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_submit(job),
            QDMI_ERROR_PERMISSIONDENIED);
  AMAZON_BRAKET_QDMI_device_job_free(job);
}

TEST_F(AmazonBraketQDMILocalJobTest, AlreadyOwnedDefaultBucketIsReused) {
  const ScopedEnvironment environment(
      AMAZON_BRAKET_QDMI_DEVICE_ENV_TASK_RESULTS_S3_URI, "");
  auto braketClient = std::make_unique<StubBraketClient>(
      Aws::Braket::Model::GetQuantumTaskResult{});
  auto s3Client = std::make_unique<StubS3Client>(StubS3Client::Configuration{
      .createError = Aws::S3::S3Errors::BUCKET_ALREADY_OWNED_BY_YOU});
  const auto* s3 = s3Client.get();
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setClient(
      session, std::move(braketClient));
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setS3Client(
      session, std::move(s3Client));
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setStsClient(
      session, std::make_unique<StubStsClient>());
  auto* const job = createConfiguredJob(session);
  ASSERT_NE(job, nullptr);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_submit(job), QDMI_SUCCESS);
  EXPECT_EQ(s3->createCalls(), 1U);
  AMAZON_BRAKET_QDMI_device_job_free(job);
}

class CreateBucketFailureTest
    : public AmazonBraketQDMILocalJobTest,
      public testing::WithParamInterface<
          std::tuple<Aws::S3::S3Errors, QDMI_STATUS>> {};

TEST_P(CreateBucketFailureTest, MapsServiceFailure) {
  const ScopedEnvironment environment(
      AMAZON_BRAKET_QDMI_DEVICE_ENV_TASK_RESULTS_S3_URI, "");
  auto s3Client = std::make_unique<StubS3Client>(
      StubS3Client::Configuration{.createError = std::get<0>(GetParam())});
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setS3Client(
      session, std::move(s3Client));
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setStsClient(
      session, std::make_unique<StubStsClient>());
  auto* const job = createConfiguredJob(session);
  ASSERT_NE(job, nullptr);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_submit(job), std::get<1>(GetParam()));
  AMAZON_BRAKET_QDMI_device_job_free(job);
}

INSTANTIATE_TEST_SUITE_P(
    AmazonBraketQDMILocalJobTest, CreateBucketFailureTest,
    testing::Values(std::tuple{Aws::S3::S3Errors::ACCESS_DENIED,
                               QDMI_ERROR_PERMISSIONDENIED},
                    std::tuple{Aws::S3::S3Errors::SERVICE_UNAVAILABLE,
                               QDMI_ERROR_FATAL}));

TEST_F(AmazonBraketQDMILocalJobTest,
       ConcurrentDefaultBucketCreationIsAccepted) {
  const ScopedEnvironment environment(
      AMAZON_BRAKET_QDMI_DEVICE_ENV_TASK_RESULTS_S3_URI, "");
  auto braketClient = std::make_unique<StubBraketClient>(
      Aws::Braket::Model::GetQuantumTaskResult{});
  const auto* braket = braketClient.get();
  auto s3Client = std::make_unique<StubS3Client>(StubS3Client::Configuration{
      .createError = Aws::S3::S3Errors::UNKNOWN,
      .exceptionName = "OperationAborted",
      .errorMessage = "A conflicting conditional operation is in progress"});
  const auto* s3 = s3Client.get();
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setClient(
      session, std::move(braketClient));
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setS3Client(
      session, std::move(s3Client));
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setStsClient(
      session, std::make_unique<StubStsClient>());
  auto* const job = createConfiguredJob(session);
  ASSERT_NE(job, nullptr);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_submit(job), QDMI_SUCCESS);
  EXPECT_EQ(s3->createCalls(), 1U);
  EXPECT_EQ(braket->createCalls(), 1U);
  AMAZON_BRAKET_QDMI_device_job_free(job);
}

TEST_F(AmazonBraketQDMILocalJobTest,
       UnrelatedOperationAbortedBucketFailureIsRejected) {
  const ScopedEnvironment environment(
      AMAZON_BRAKET_QDMI_DEVICE_ENV_TASK_RESULTS_S3_URI, "");
  auto s3Client = std::make_unique<StubS3Client>(StubS3Client::Configuration{
      .createError = Aws::S3::S3Errors::UNKNOWN,
      .exceptionName = "OperationAborted",
      .errorMessage = "A different operation was aborted"});
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setS3Client(
      session, std::move(s3Client));
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setStsClient(
      session, std::make_unique<StubStsClient>());

  auto* const job = createConfiguredJob(session);
  ASSERT_NE(job, nullptr);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_submit(job), QDMI_ERROR_FATAL);
  AMAZON_BRAKET_QDMI_device_job_free(job);
}

TEST_F(AmazonBraketQDMILocalJobTest,
       ResultRetrievalUsesTaskReturnedS3Location) {
#ifdef _WIN32
  GTEST_SKIP() << "The test executable and provider DLL contain separate "
                  "static AWS SDK ResponseStream state on Windows.";
#endif
  constexpr auto* taskArn =
      "arn:aws:braket:us-east-1:123456789012:quantum-task/task-id";
  Aws::Braket::Model::GetQuantumTaskResult
      task; // NOLINT(misc-const-correctness)
  task.WithQuantumTaskArn(taskArn)
      .WithDeviceArn("arn:aws:braket:::device/quantum-simulator/amazon/sv1")
      .WithStatus(Aws::Braket::Model::QuantumTaskStatus::COMPLETED)
      .WithShots(2)
      .WithOutputS3Bucket("task-returned-results")
      .WithOutputS3Directory("returned/task-id");
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setClient(
      session, std::make_unique<StubBraketClient>(std::move(task)));
  auto s3Client = std::make_unique<StubS3Client>();
  const auto* s3 = s3Client.get();
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setS3Client(
      session, std::move(s3Client));

  AMAZON_BRAKET_QDMI_Device_Job job = nullptr;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_retrieve_device_job_by_id(
                session, taskArn, &job),
            QDMI_SUCCESS);
  size_t resultSize = 0; // NOLINT(misc-const-correctness)
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_job_get_results(
                job, QDMI_JOB_RESULT_SHOTS, 0, nullptr, &resultSize),
            QDMI_SUCCESS);
  std::string result(resultSize - 1, '\0');
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_get_results(
                job, QDMI_JOB_RESULT_SHOTS, resultSize, result.data(), nullptr),
            QDMI_SUCCESS);
  EXPECT_EQ(result, "00,11");
  EXPECT_EQ(s3->getObjectCalls(), 1U);
  EXPECT_EQ(s3->getObjectBucket(), "task-returned-results");
  EXPECT_EQ(s3->getObjectKey(), "returned/task-id/results.json");
  AMAZON_BRAKET_QDMI_device_job_free(job);
}

TEST_F(AmazonBraketQDMILocalJobTest, ResultRetrievalMapsS3PermissionFailure) {
  constexpr auto* taskArn =
      "arn:aws:braket:us-east-1:123456789012:quantum-task/task-id";
  Aws::Braket::Model::GetQuantumTaskResult task;
  task.WithQuantumTaskArn(taskArn)
      .WithDeviceArn("arn:aws:braket:::device/quantum-simulator/amazon/sv1")
      .WithStatus(Aws::Braket::Model::QuantumTaskStatus::COMPLETED)
      .WithShots(2)
      .WithOutputS3Bucket("task-returned-results")
      .WithOutputS3Directory("returned/task-id");
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setClient(
      session, std::make_unique<StubBraketClient>(std::move(task)));
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setS3Client(
      session, std::make_unique<StubS3Client>(StubS3Client::Configuration{
                   .getObjectError = Aws::S3::S3Errors::ACCESS_DENIED}));
  AMAZON_BRAKET_QDMI_Device_Job job = nullptr;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_retrieve_device_job_by_id(
                session, taskArn, &job),
            QDMI_SUCCESS);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_get_results(
                job, QDMI_JOB_RESULT_SHOTS, 0, nullptr, nullptr),
            QDMI_ERROR_PERMISSIONDENIED);
  AMAZON_BRAKET_QDMI_device_job_free(job);
}

TEST_F(AmazonBraketQDMILocalJobTest, ResultRetrievalMapsMissingS3Object) {
  constexpr auto* taskArn =
      "arn:aws:braket:us-east-1:123456789012:quantum-task/task-id";
  Aws::Braket::Model::GetQuantumTaskResult task;
  task.WithQuantumTaskArn(taskArn)
      .WithDeviceArn("arn:aws:braket:::device/quantum-simulator/amazon/sv1")
      .WithStatus(Aws::Braket::Model::QuantumTaskStatus::COMPLETED)
      .WithShots(2)
      .WithOutputS3Bucket("task-returned-results")
      .WithOutputS3Directory("returned/task-id");
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setClient(
      session, std::make_unique<StubBraketClient>(std::move(task)));
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setS3Client(
      session, std::make_unique<StubS3Client>(StubS3Client::Configuration{
                   .getObjectError = Aws::S3::S3Errors::NO_SUCH_KEY}));
  AMAZON_BRAKET_QDMI_Device_Job job = nullptr;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_retrieve_device_job_by_id(
                session, taskArn, &job),
            QDMI_SUCCESS);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_get_results(
                job, QDMI_JOB_RESULT_SHOTS, 0, nullptr, nullptr),
            QDMI_ERROR_NOTFOUND);
  AMAZON_BRAKET_QDMI_device_job_free(job);
}

class InvalidResultDocumentTest
    : public AmazonBraketQDMILocalJobTest,
      public testing::WithParamInterface<std::string> {};

TEST_P(InvalidResultDocumentTest, IsRejected) {
#ifdef _WIN32
  GTEST_SKIP() << "The test executable and provider DLL contain separate "
                  "static AWS SDK ResponseStream state on Windows.";
#endif
  constexpr auto* taskArn =
      "arn:aws:braket:us-east-1:123456789012:quantum-task/task-id";
  Aws::Braket::Model::GetQuantumTaskResult task;
  task.WithQuantumTaskArn(taskArn)
      .WithDeviceArn("arn:aws:braket:::device/quantum-simulator/amazon/sv1")
      .WithStatus(Aws::Braket::Model::QuantumTaskStatus::COMPLETED)
      .WithShots(2)
      .WithOutputS3Bucket("task-returned-results")
      .WithOutputS3Directory("returned/task-id");
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setClient(
      session, std::make_unique<StubBraketClient>(std::move(task)));
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setS3Client(
      session, std::make_unique<StubS3Client>(
                   StubS3Client::Configuration{.resultJson = GetParam()}));
  AMAZON_BRAKET_QDMI_Device_Job job = nullptr;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_retrieve_device_job_by_id(
                session, taskArn, &job),
            QDMI_SUCCESS);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_get_results(
                job, QDMI_JOB_RESULT_SHOTS, 0, nullptr, nullptr),
            QDMI_ERROR_FATAL);
  AMAZON_BRAKET_QDMI_device_job_free(job);
}

INSTANTIATE_TEST_SUITE_P(AmazonBraketQDMILocalJobTest,
                         InvalidResultDocumentTest,
                         testing::Values("not-json", R"({"metadata":{}})"));

// =============================================================================
// DeviceParser offline error-path tests
// =============================================================================

namespace {
auto operationNames(
    const std::vector<AMAZON_BRAKET_QDMI_Operation_impl_d*>& operations)
    -> std::vector<std::string> {
  std::vector<std::string> names;
  names.reserve(operations.size());
  std::ranges::transform(
      operations, std::back_inserter(names),
      [](const auto* operation) { return operation->name_; });
  return names;
}

auto siteNames(const ParsedDeviceProperties& properties)
    -> std::vector<std::string> {
  std::vector<std::string> names;
  names.reserve(properties.sitesPtr.size());
  std::ranges::transform(properties.sitesPtr, std::back_inserter(names),
                         [](const auto* site) { return site->name_; });
  return names;
}

auto siteIndices(const ParsedDeviceProperties& properties)
    -> std::vector<size_t> {
  std::vector<size_t> indices;
  indices.reserve(properties.sitesPtr.size());
  std::ranges::transform(properties.sitesPtr, std::back_inserter(indices),
                         [](const auto* site) { return site->id_; });
  return indices;
}

struct CapabilityFixtureCase {
  std::string_view name;
  std::string_view json;
  Aws::Braket::Model::DeviceType type;
  std::vector<std::string> sites;
  std::vector<size_t> indices;
  std::vector<std::string> nativeOperations;
  std::vector<std::string> supportedOperations;
  size_t directedEdges;
};
} // namespace

TEST(DeviceParserOfflineTest, RejectsMalformedCapabilityDocuments) {
  const GateModelCapabilityParser parser;
  ParsedDeviceProperties properties;
  EXPECT_EQ(parser.parseProperties(Aws::Braket::Model::DeviceType::NOT_SET,
                                   R"({})", properties),
            QDMI_ERROR_NOTSUPPORTED);
  EXPECT_EQ(parser.parseProperties(Aws::Braket::Model::DeviceType::SIMULATOR,
                                   R"({)", properties),
            QDMI_ERROR_FATAL);
  EXPECT_EQ(parser.parseProperties(Aws::Braket::Model::DeviceType::SIMULATOR,
                                   R"({})", properties),
            QDMI_ERROR_FATAL);
  EXPECT_EQ(parser.parseProperties(Aws::Braket::Model::DeviceType::SIMULATOR,
                                   R"({"paradigm":{}})", properties),
            QDMI_ERROR_FATAL);
  EXPECT_EQ(parser.parseProperties(Aws::Braket::Model::DeviceType::SIMULATOR,
                                   R"({"paradigm":{"qubitCount":0}})",
                                   properties),
            QDMI_ERROR_FATAL);
  EXPECT_EQ(
      parser.parseProperties(
          Aws::Braket::Model::DeviceType::QPU,
          R"({"paradigm":{"qubitCount":2,"nativeGateSet":["x"],"connectivity":{"fullyConnected":false,"connectivityGraph":{"a":["b"]}}},"action":{"braket.ir.openqasm.program":{"supportedOperations":["x"]}}})",
          properties),
      QDMI_SUCCESS);
  EXPECT_EQ(
      parser.parseProperties(
          Aws::Braket::Model::DeviceType::QPU,
          R"({"paradigm":{"qubitCount":3,"nativeGateSet":["x"],"connectivity":{"fullyConnected":false,"connectivityGraph":{"a":["b"]}}},"action":{"braket.ir.openqasm.program":{"supportedOperations":["x"]}}})",
          properties),
      QDMI_ERROR_FATAL);
  EXPECT_EQ(
      parser.parseProperties(
          Aws::Braket::Model::DeviceType::QPU,
          R"({"paradigm":{"qubitCount":2,"nativeGateSet":["x"]},"action":{"braket.ir.openqasm.program":{"supportedOperations":["x"]}}})",
          properties),
      QDMI_ERROR_FATAL);
  EXPECT_EQ(
      parser.parseProperties(
          Aws::Braket::Model::DeviceType::QPU,
          R"({"paradigm":{"qubitCount":2,"nativeGateSet":["x"],"connectivity":{"fullyConnected":false}},"action":{"braket.ir.openqasm.program":{"supportedOperations":["x"]}}})",
          properties),
      QDMI_ERROR_FATAL);
  EXPECT_EQ(
      parser.parseProperties(
          Aws::Braket::Model::DeviceType::QPU,
          R"({"paradigm":{"qubitCount":2,"nativeGateSet":["x"],"connectivity":{"fullyConnected":false,"connectivityGraph":{"184467440737095516160000":["a"],"a":[]}}},"action":{"braket.ir.openqasm.program":{"supportedOperations":["x"]}}})",
          properties),
      QDMI_ERROR_FATAL);
  EXPECT_EQ(
      parser.parseProperties(
          Aws::Braket::Model::DeviceType::QPU,
          R"({"paradigm":{"qubitCount":2,"connectivity":{"fullyConnected":true}},"action":{"braket.ir.openqasm.program":{"supportedOperations":["x"]}}})",
          properties),
      QDMI_ERROR_FATAL);
}

TEST(DeviceParserOfflineTest, HandlesSingleSiteAndCalibrationEdgeCases) {
  ParsedDeviceProperties properties;
  const GateModelCapabilityParser parser;
  ASSERT_EQ(
      parser.parseProperties(
          Aws::Braket::Model::DeviceType::SIMULATOR,
          R"({"paradigm":{"qubitCount":1},"action":{"braket.ir.openqasm.program":{"supportedOperations":["x"]}}})",
          properties),
      QDMI_SUCCESS);
  EXPECT_TRUE(properties.connectivity.empty());

  const GateModelCapabilityParser parserWithCalibration{
      {GateModelCapabilityParser::enrichIqmCalibration}};
  ASSERT_EQ(
      parserWithCalibration.parseProperties(
          Aws::Braket::Model::DeviceType::QPU,
          R"({"paradigm":{"qubitCount":1,"nativeGateSet":["x"],"connectivity":{"fullyConnected":true}},"action":{"braket.ir.openqasm.program":{"supportedOperations":["x"]}},"provider":{"properties":{"one_qubit":{"0":{"T1":-1.0,"T2":0.0},"unknown":{"T1":0.001}}}}})",
          properties),
      QDMI_SUCCESS);
  ASSERT_EQ(properties.sitesPtr.size(), 1U);
  EXPECT_EQ(properties.sitesPtr.front()->t1_, std::nullopt);
  EXPECT_EQ(properties.sitesPtr.front()->t2_, std::nullopt);
}

TEST(DeviceParserOfflineTest, ParsesRepresentativeCapabilityFixtures) {
  using Aws::Braket::Model::DeviceType;
  using namespace amazon::braket::qdmi::test;
  const std::vector<CapabilityFixtureCase> fixtures{
      {"AQT IBEX-Q1",
       AQT_IBEX_Q1,
       DeviceType::QPU,
       {"0", "1", "2", "3"},
       {0, 1, 2, 3},
       {"prx", "xx", "rz"},
       {"prx", "xx", "rz", "h", "cnot", "swap"},
       12},
      {"IonQ Forte",
       IONQ_FORTE,
       DeviceType::QPU,
       {"0", "1", "2"},
       {0, 1, 2},
       {"gpi", "gpi2", "zz"},
       {"x", "y", "z", "rx", "ry", "rz", "h", "cnot", "gpi", "gpi2", "zz"},
       6},
      {"IQM Garnet",
       IQM_GARNET,
       DeviceType::QPU,
       {"1", "2", "5"},
       {1, 2, 5},
       {"cz", "prx"},
       {"h", "cnot", "cz", "prx", "rx", "rz"},
       3},
      {"Rigetti Ankaa-3",
       RIGETTI_ANKAA_3,
       DeviceType::QPU,
       {"0", "1", "2"},
       {0, 1, 2},
       {"rx", "rz", "iswap"},
       {"rx", "rz", "iswap", "cz", "xy", "h", "cnot"},
       2},
      {"Rigetti 12-site ordering",
       RIGETTI_ANKAA_12,
       DeviceType::QPU,
       {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11"},
       {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11},
       {"rx", "rz", "cz"},
       {"rx", "rz", "cz"},
       11},
      {"SV1",
       SV1,
       DeviceType::SIMULATOR,
       {"0", "1", "2", "3"},
       {0, 1, 2, 3},
       {"h", "cnot", "rx", "ry", "rz", "unitary", "gphase"},
       {"h", "cnot", "rx", "ry", "rz", "unitary", "gphase"},
       12},
      {"DM1",
       DM1,
       DeviceType::SIMULATOR,
       {"0", "1", "2"},
       {0, 1, 2},
       {"x", "cnot", "kraus", "bit_flip", "gphase"},
       {"x", "cnot", "kraus", "bit_flip", "gphase"},
       6},
  };

  for (const auto& fixture : fixtures) {
    SCOPED_TRACE(fixture.name);
    std::vector<GateModelCapabilityParser::CalibrationEnricher> enrichers;
    if (fixture.name == "IQM Garnet") {
      enrichers.emplace_back(GateModelCapabilityParser::enrichIqmCalibration);
    }
    const GateModelCapabilityParser parser{std::move(enrichers)};
    ParsedDeviceProperties properties;
    ASSERT_EQ(parser.parseProperties(fixture.type, std::string{fixture.json},
                                     properties),
              QDMI_SUCCESS);
    EXPECT_EQ(properties.qubitCount, fixture.sites.size());
    EXPECT_EQ(siteNames(properties), fixture.sites);
    EXPECT_EQ(siteIndices(properties), fixture.indices);
    EXPECT_EQ(properties.connectivity.size(), fixture.directedEdges * 2);
    EXPECT_EQ(operationNames(properties.operationsPtr),
              fixture.nativeOperations);
    EXPECT_EQ(operationNames(properties.supportedOperationsPtr),
              fixture.supportedOperations);
    if (fixture.type == DeviceType::QPU) {
      for (auto* operation : properties.operationsPtr) {
        EXPECT_TRUE(operation->numQubits_.has_value());
        EXPECT_TRUE(operation->numParams_.has_value());
      }
    }
  }
}

TEST(DeviceParserOfflineTest, RejectsAliasedDecimalSiteIndices) {
  const GateModelCapabilityParser parser;
  ParsedDeviceProperties properties;
  EXPECT_EQ(
      parser.parseProperties(
          Aws::Braket::Model::DeviceType::QPU,
          R"({"paradigm":{"qubitCount":2,"nativeGateSet":["x"],"connectivity":{"fullyConnected":false,"connectivityGraph":{"1":["01"],"01":[]}}},"action":{"braket.ir.openqasm.program":{"supportedOperations":["x"]}}})",
          properties),
      QDMI_ERROR_FATAL);
}

TEST(DeviceParserOfflineTest, AssignsDeterministicNonnumericSiteIndices) {
  const GateModelCapabilityParser parser;
  ParsedDeviceProperties properties;
  ASSERT_EQ(
      parser.parseProperties(
          Aws::Braket::Model::DeviceType::QPU,
          R"({"paradigm":{"qubitCount":4,"nativeGateSet":["x"],"connectivity":{"fullyConnected":false,"connectivityGraph":{"2":["10"],"10":["alpha"],"alpha":["beta"],"beta":[]}}},"action":{"braket.ir.openqasm.program":{"supportedOperations":["x"]}}})",
          properties),
      QDMI_SUCCESS);
  EXPECT_EQ(siteNames(properties),
            (std::vector<std::string>{"2", "10", "alpha", "beta"}));
  EXPECT_EQ(siteIndices(properties), (std::vector<size_t>{2, 10, 0, 1}));
}

TEST_F(AmazonBraketQDMILocalJobTest,
       SiteIndexQueriesPreserveProviderProgramIndices) {
  const auto checkFixture = [this](const std::string_view json,
                                   const std::vector<size_t>& expected) {
    const GateModelCapabilityParser parser;
    ParsedDeviceProperties properties;
    ASSERT_EQ(parser.parseProperties(Aws::Braket::Model::DeviceType::QPU,
                                     std::string{json}, properties),
              QDMI_SUCCESS);
    const auto architecture =
        installParsedArchitecture(session, std::move(properties));
    ASSERT_EQ(architecture->sitesPtr.size(), expected.size());
    for (size_t position = 0; position < expected.size(); ++position) {
      SCOPED_TRACE(position);
      size_t index = std::numeric_limits<size_t>::max();
      size_t sizeRet = 0;
      EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_query_site_property(
                    session, architecture->sitesPtr[position],
                    QDMI_SITE_PROPERTY_INDEX, sizeof(index), &index, &sizeRet),
                QDMI_SUCCESS);
      EXPECT_EQ(sizeRet, sizeof(size_t));
      EXPECT_EQ(index, expected[position]);
    }
  };

  using namespace amazon::braket::qdmi::test;
  checkFixture(IQM_GARNET, {1, 2, 5});
  checkFixture(RIGETTI_ANKAA_12, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11});
}

TEST(DeviceParserOfflineTest,
     NativeAndSupportedViewsShareCanonicalOperationHandles) {
  const GateModelCapabilityParser parser{
      {GateModelCapabilityParser::enrichIqmCalibration}};
  ParsedDeviceProperties properties;
  ASSERT_EQ(parser.parseProperties(
                Aws::Braket::Model::DeviceType::QPU,
                std::string{amazon::braket::qdmi::test::IQM_GARNET},
                properties),
            QDMI_SUCCESS);

  const auto* const nativeCz = properties.operationsPtr.front();
  const auto supportedCz = std::ranges::find_if(
      properties.supportedOperationsPtr,
      [](const auto* operation) { return operation->name_ == "cz"; });
  ASSERT_NE(supportedCz, properties.supportedOperationsPtr.end());
  EXPECT_EQ(nativeCz, *supportedCz);
  EXPECT_EQ(properties.allOperationsPtr.size(), properties.operations.size());
  EXPECT_EQ(properties.sitesMap.at("1")->t1_, 40'000U);
  EXPECT_EQ(properties.sitesMap.at("5")->t2_, 50'000U);
}

TEST(DeviceParserOfflineTest, PreservesMergedOperationSignatureRegistry) {
  const GateModelCapabilityParser parser;
  ParsedDeviceProperties properties;
  ASSERT_EQ(
      parser.parseProperties(
          Aws::Braket::Model::DeviceType::SIMULATOR,
          R"({"paradigm":{"qubitCount":3},"action":{"braket.ir.openqasm.program":{"supportedOperations":["cc_prx","measure_ff","ms","unitary","kraus","gphase","bit_flip","ccnot"]}}})",
          properties),
      QDMI_SUCCESS);

  ASSERT_EQ(properties.operationsMap.size(), 8U);
  EXPECT_EQ(properties.operationsMap.at("ms")->numQubits_, 2U);
  EXPECT_EQ(properties.operationsMap.at("ms")->numParams_, 3U);
  EXPECT_EQ(properties.operationsMap.at("gphase")->numQubits_, 0U);
  EXPECT_EQ(properties.operationsMap.at("gphase")->numParams_, 1U);
  EXPECT_TRUE(properties.operationsMap.at("gphase")->applicableSites_.empty());

  for (const auto* name : {"cc_prx", "measure_ff"}) {
    const auto* operation = properties.operationsMap.at(name);
    EXPECT_EQ(operation->numQubits_, 1U);
    EXPECT_FALSE(operation->numParams_.has_value());
    EXPECT_EQ(operation->applicableSites_.size(), 3U);
  }
  for (const auto* name : {"unitary", "kraus", "bit_flip"}) {
    const auto* operation = properties.operationsMap.at(name);
    EXPECT_FALSE(operation->numQubits_.has_value());
    EXPECT_FALSE(operation->numParams_.has_value());
    EXPECT_TRUE(operation->applicableSites_.empty());
  }
  EXPECT_EQ(properties.operationsMap.at("ccnot")->applicableSites_.size(), 18U);
}

TEST_F(AmazonBraketQDMILocalJobTest,
       PublicQueriesAcceptSupportedAndUnknownOperations) {
  const GateModelCapabilityParser parser;
  ParsedDeviceProperties properties;
  ASSERT_EQ(
      parser.parseProperties(
          Aws::Braket::Model::DeviceType::SIMULATOR,
          R"({"paradigm":{"qubitCount":3},"action":{"braket.ir.openqasm.program":{"supportedOperations":["gphase","unitary","future_gate"]}}})",
          properties),
      QDMI_SUCCESS);
  const auto architecture =
      installParsedArchitecture(session, std::move(properties));

  auto* const gphase = architecture->operationsMap.at("gphase");
  size_t numQubits = 1;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_query_operation_property(
                session, gphase, 0, nullptr, 0, nullptr,
                QDMI_OPERATION_PROPERTY_QUBITSNUM, sizeof(numQubits),
                &numQubits, nullptr),
            QDMI_SUCCESS);
  EXPECT_EQ(numQubits, 0U);

  size_t numParameters = 0;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_query_operation_property(
                session, gphase, 0, nullptr, 0, nullptr,
                QDMI_OPERATION_PROPERTY_PARAMETERSNUM, sizeof(numParameters),
                &numParameters, nullptr),
            QDMI_SUCCESS);
  EXPECT_EQ(numParameters, 1U);

  for (const auto* name : {"unitary", "future_gate"}) {
    auto* const operation = architecture->operationsMap.at(name);
    EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_query_operation_property(
                  session, operation, 0, nullptr, 0, nullptr,
                  QDMI_OPERATION_PROPERTY_QUBITSNUM, sizeof(numQubits),
                  &numQubits, nullptr),
              QDMI_ERROR_NOTSUPPORTED);
    EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_query_operation_property(
                  session, operation, 0, nullptr, 0, nullptr,
                  QDMI_OPERATION_PROPERTY_PARAMETERSNUM, sizeof(numParameters),
                  &numParameters, nullptr),
              QDMI_ERROR_NOTSUPPORTED);
  }
}

TEST_F(AmazonBraketQDMILocalJobTest,
       SupportedOperationHandlesRetainCalibrationQueries) {
  const GateModelCapabilityParser parser{
      {GateModelCapabilityParser::enrichIqmCalibration}};
  ParsedDeviceProperties properties;
  ASSERT_EQ(parser.parseProperties(
                Aws::Braket::Model::DeviceType::QPU,
                std::string{amazon::braket::qdmi::test::IQM_GARNET},
                properties),
            QDMI_SUCCESS);
  const auto architecture =
      installParsedArchitecture(session, std::move(properties));

  auto* const cz = architecture->operationsMap.at("cz");
  const std::array<AMAZON_BRAKET_QDMI_Site, 2> forwardSites{
      architecture->sitesMap.at("1"), architecture->sitesMap.at("2")};
  double fidelity = 0;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_query_operation_property(
                session, cz, forwardSites.size(), forwardSites.data(), 0,
                nullptr, QDMI_OPERATION_PROPERTY_FIDELITY, sizeof(fidelity),
                &fidelity, nullptr),
            QDMI_SUCCESS);
  EXPECT_DOUBLE_EQ(fidelity, 0.987);

  const std::array<AMAZON_BRAKET_QDMI_Site, 2> reverseSites{forwardSites[1],
                                                            forwardSites[0]};
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_query_operation_property(
                session, cz, reverseSites.size(), reverseSites.data(), 0,
                nullptr, QDMI_OPERATION_PROPERTY_FIDELITY, sizeof(fidelity),
                &fidelity, nullptr),
            QDMI_ERROR_NOTSUPPORTED);

  auto* const supportedOnly = architecture->operationsMap.at("h");
  size_t nameSize = 0;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_query_operation_property(
                session, supportedOnly, 0, nullptr, 0, nullptr,
                QDMI_OPERATION_PROPERTY_NAME, 0, nullptr, &nameSize),
            QDMI_SUCCESS);
  EXPECT_EQ(nameSize, 2U);
}

TEST_F(AmazonBraketQDMILocalJobTest,
       RawDevicePropertiesExposeSimulatorExecutableOperations) {
  Aws::Braket::Model::GetDeviceResult result;
  result.SetDeviceName("SV1");
  result.SetProviderName("Amazon Web Services");
  result.SetDeviceType(Aws::Braket::Model::DeviceType::SIMULATOR);
  result.SetDeviceStatus(Aws::Braket::Model::DeviceStatus::ONLINE);
  result.SetDeviceCapabilities(
      std::string{amazon::braket::qdmi::test::SV1}.c_str());
  auto stub = std::make_unique<StubGetDeviceClient>(std::move(result));
  auto* const stubPtr = stub.get();
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setClient(session,
                                                          std::move(stub));

  size_t standardSize = 0;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_query_device_property(
          session, QDMI_DEVICE_PROPERTY_OPERATIONS, 0, nullptr, &standardSize),
      QDMI_SUCCESS);
  size_t supportedSize = 0;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_query_device_property(
                session, AMAZON_BRAKET_QDMI_DEVICE_PROPERTY_SUPPORTEDOPERATIONS,
                0, nullptr, &supportedSize),
            QDMI_SUCCESS);
  EXPECT_EQ(standardSize, supportedSize);

  std::vector<AMAZON_BRAKET_QDMI_Operation> standard(
      standardSize / sizeof(AMAZON_BRAKET_QDMI_Operation));
  std::vector<AMAZON_BRAKET_QDMI_Operation> supported(
      supportedSize / sizeof(AMAZON_BRAKET_QDMI_Operation));
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_query_device_property(
                session, QDMI_DEVICE_PROPERTY_OPERATIONS, standardSize,
                static_cast<void*>(standard.data()), nullptr),
            QDMI_SUCCESS);
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_query_device_property(
                session, AMAZON_BRAKET_QDMI_DEVICE_PROPERTY_SUPPORTEDOPERATIONS,
                supportedSize, static_cast<void*>(supported.data()), nullptr),
            QDMI_SUCCESS);
  EXPECT_EQ(standard, supported);
  EXPECT_GE(stubPtr->calls(), 1U);
  EXPECT_EQ(stubPtr->requestedArn(),
            "arn:aws:braket:::device/quantum-simulator/amazon/sv1");
}

TEST_F(AmazonBraketQDMILocalJobTest,
       DevicePropertyPropagatesInitialGetDeviceFailure) {
  auto stub = std::make_unique<StubGetDeviceClient>(
      Aws::Braket::Model::GetDeviceResult{},
      Aws::Braket::BraketErrors::INTERNAL_FAILURE);
  auto* const stubPtr = stub.get();
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setClient(session,
                                                          std::move(stub));

  size_t numQubits = 0;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_query_device_property(
                session, QDMI_DEVICE_PROPERTY_QUBITSNUM, sizeof(numQubits),
                &numQubits, nullptr),
            QDMI_ERROR_FATAL);
  EXPECT_EQ(stubPtr->calls(), 1U);
}

TEST_F(AmazonBraketQDMILocalJobTest,
       DevicePropertyPropagatesCachedStatusRefreshFailure) {
  Aws::Braket::Model::GetDeviceResult result;
  result.SetDeviceName("SV1");
  result.SetProviderName("Amazon Web Services");
  result.SetDeviceType(Aws::Braket::Model::DeviceType::SIMULATOR);
  result.SetDeviceStatus(Aws::Braket::Model::DeviceStatus::ONLINE);
  result.SetDeviceCapabilities(
      std::string{amazon::braket::qdmi::test::SV1}.c_str());
  auto stub = std::make_unique<StubGetDeviceClient>(
      std::move(result), Aws::Braket::BraketErrors::INTERNAL_FAILURE, true);
  auto* const stubPtr = stub.get();
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setClient(session,
                                                          std::move(stub));

  size_t numQubits = 0;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_query_device_property(
                session, QDMI_DEVICE_PROPERTY_QUBITSNUM, sizeof(numQubits),
                &numQubits, nullptr),
            QDMI_SUCCESS);
  EXPECT_GT(numQubits, 0U);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_query_device_property(
                session, QDMI_DEVICE_PROPERTY_QUBITSNUM, sizeof(numQubits),
                &numQubits, nullptr),
            QDMI_ERROR_FATAL);
  EXPECT_EQ(stubPtr->calls(), 2U);
}

TEST_F(AmazonBraketQDMILocalJobTest,
       DevicePropertyPreservesGetDevicePermissionFailure) {
  auto stub = std::make_unique<StubGetDeviceClient>(
      Aws::Braket::Model::GetDeviceResult{},
      Aws::Braket::BraketErrors::ACCESS_DENIED);
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setClient(session,
                                                          std::move(stub));

  size_t numQubits = 0;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_query_device_property(
                session, QDMI_DEVICE_PROPERTY_QUBITSNUM, sizeof(numQubits),
                &numQubits, nullptr),
            QDMI_ERROR_PERMISSIONDENIED);
}

TEST_F(AmazonBraketQDMILocalJobTest,
       DevicePropertyMapsMissingGetDeviceResource) {
  auto stub = std::make_unique<StubGetDeviceClient>(
      Aws::Braket::Model::GetDeviceResult{},
      Aws::Braket::BraketErrors::RESOURCE_NOT_FOUND);
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setClient(session,
                                                          std::move(stub));

  size_t numQubits = 0;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_query_device_property(
                session, QDMI_DEVICE_PROPERTY_QUBITSNUM, sizeof(numQubits),
                &numQubits, nullptr),
            QDMI_ERROR_NOTFOUND);
}

TEST_F(AmazonBraketQDMILocalJobTest,
       DevicePropertyMapsInvalidGetDeviceRequest) {
  auto stub = std::make_unique<StubGetDeviceClient>(
      Aws::Braket::Model::GetDeviceResult{},
      Aws::Braket::BraketErrors::VALIDATION);
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setClient(session,
                                                          std::move(stub));

  size_t numQubits = 0;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_query_device_property(
                session, QDMI_DEVICE_PROPERTY_QUBITSNUM, sizeof(numQubits),
                &numQubits, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
}

TEST_F(AmazonBraketQDMILocalJobTest,
       RetiredDeviceStatusIsQueryableOnInitialAndCachedFetches) {
  Aws::Braket::Model::GetDeviceResult result;
  result.SetDeviceName("SV1");
  result.SetProviderName("Amazon Web Services");
  result.SetDeviceType(Aws::Braket::Model::DeviceType::SIMULATOR);
  result.SetDeviceStatus(Aws::Braket::Model::DeviceStatus::RETIRED);
  result.SetDeviceCapabilities(
      std::string{amazon::braket::qdmi::test::SV1}.c_str());
  auto stub = std::make_unique<StubGetDeviceClient>(std::move(result));
  auto* const stubPtr = stub.get();
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setClient(session,
                                                          std::move(stub));

  QDMI_Device_Status status = QDMI_DEVICE_STATUS_IDLE;
  for (size_t query = 0; query < 2; ++query) {
    SCOPED_TRACE(query);
    EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_query_device_property(
                  session, QDMI_DEVICE_PROPERTY_STATUS, sizeof(status), &status,
                  nullptr),
              QDMI_SUCCESS);
    EXPECT_EQ(status, QDMI_DEVICE_STATUS_OFFLINE);
  }
  EXPECT_EQ(stubPtr->calls(), 2U);
}

TEST_F(AmazonBraketQDMILocalJobTest,
       DevicePropertyQueriesTranslateClientExceptions) {
  constexpr std::array expectedStatuses{
      QDMI_ERROR_OUTOFMEM,
      QDMI_ERROR_INVALIDARGUMENT,
      QDMI_ERROR_FATAL,
  };
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setClient(
      session, std::make_unique<ThrowingGetDeviceClient>());

  for (size_t call = 0; call < expectedStatuses.size(); ++call) {
    SCOPED_TRACE(call);
    size_t nameSize = 0;
    EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_query_device_property(
                  session, QDMI_DEVICE_PROPERTY_NAME, 0, nullptr, &nameSize),
              expectedStatuses[call]);
  }
}

TEST_F(AmazonBraketQDMILocalJobTest,
       DurationMetadataScalesPublicCalibrationValues) {
  Aws::Braket::Model::GetDeviceResult result;
  result.SetDeviceName("Garnet");
  result.SetProviderName("IQM");
  result.SetDeviceType(Aws::Braket::Model::DeviceType::QPU);
  result.SetDeviceStatus(Aws::Braket::Model::DeviceStatus::ONLINE);
  result.SetDeviceCapabilities(
      std::string{amazon::braket::qdmi::test::IQM_GARNET}.c_str());
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setClient(
      session, std::make_unique<StubGetDeviceClient>(std::move(result)));

  std::array<char, 3> unit{};
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_query_device_property(
                session, QDMI_DEVICE_PROPERTY_DURATIONUNIT, unit.size(),
                unit.data(), nullptr),
            QDMI_SUCCESS);
  EXPECT_STREQ(unit.data(), "us");

  double scaleFactor = 0.0;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_query_device_property(
                session, QDMI_DEVICE_PROPERTY_DURATIONSCALEFACTOR,
                sizeof(scaleFactor), &scaleFactor, nullptr),
            QDMI_SUCCESS);
  EXPECT_DOUBLE_EQ(scaleFactor, 0.001);

  size_t sitesSize = 0;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_query_device_property(
                session, QDMI_DEVICE_PROPERTY_SITES, 0, nullptr, &sitesSize),
            QDMI_SUCCESS);
  std::vector<AMAZON_BRAKET_QDMI_Site> sites(sitesSize /
                                             sizeof(AMAZON_BRAKET_QDMI_Site));
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_query_device_property(
                session, QDMI_DEVICE_PROPERTY_SITES, sitesSize,
                static_cast<void*>(sites.data()), nullptr),
            QDMI_SUCCESS);
  ASSERT_EQ(sites.size(), 3U);

  uint64_t t1 = 0;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_query_site_property(
          session, sites[0], QDMI_SITE_PROPERTY_T1, sizeof(t1), &t1, nullptr),
      QDMI_SUCCESS);
  EXPECT_EQ(t1, 40'000U);
  EXPECT_DOUBLE_EQ(static_cast<double>(t1) * scaleFactor, 40.0);

  uint64_t t2 = 0;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_query_site_property(
          session, sites[2], QDMI_SITE_PROPERTY_T2, sizeof(t2), &t2, nullptr),
      QDMI_SUCCESS);
  EXPECT_EQ(t2, 50'000U);
  EXPECT_DOUBLE_EQ(static_cast<double>(t2) * scaleFactor, 50.0);
}

TEST_F(AmazonBraketQDMILocalJobTest,
       RawQpuPropertiesKeepNativeAndSupportedHandlesStable) {
  Aws::Braket::Model::GetDeviceResult result;
  result.SetDeviceName("Garnet");
  result.SetProviderName("IQM");
  result.SetDeviceType(Aws::Braket::Model::DeviceType::QPU);
  result.SetDeviceStatus(Aws::Braket::Model::DeviceStatus::ONLINE);
  result.SetDeviceCapabilities(
      std::string{amazon::braket::qdmi::test::IQM_GARNET}.c_str());
  auto stub = std::make_unique<StubGetDeviceClient>(std::move(result));
  auto* const stubPtr = stub.get();
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setClient(session,
                                                          std::move(stub));

  size_t nativeSize = 0;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_query_device_property(
          session, QDMI_DEVICE_PROPERTY_OPERATIONS, 0, nullptr, &nativeSize),
      QDMI_SUCCESS);
  std::vector<AMAZON_BRAKET_QDMI_Operation> native(
      nativeSize / sizeof(AMAZON_BRAKET_QDMI_Operation));
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_query_device_property(
                session, QDMI_DEVICE_PROPERTY_OPERATIONS, nativeSize,
                static_cast<void*>(native.data()), nullptr),
            QDMI_SUCCESS);

  size_t supportedSize = 0;
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_query_device_property(
                session, AMAZON_BRAKET_QDMI_DEVICE_PROPERTY_SUPPORTEDOPERATIONS,
                0, nullptr, &supportedSize),
            QDMI_SUCCESS);
  std::vector<AMAZON_BRAKET_QDMI_Operation> supported(
      supportedSize / sizeof(AMAZON_BRAKET_QDMI_Operation));
  ASSERT_EQ(AMAZON_BRAKET_QDMI_device_session_query_device_property(
                session, AMAZON_BRAKET_QDMI_DEVICE_PROPERTY_SUPPORTEDOPERATIONS,
                supportedSize, static_cast<void*>(supported.data()), nullptr),
            QDMI_SUCCESS);

  ASSERT_EQ(native.size(), 2U);
  ASSERT_EQ(supported.size(), 6U);
  EXPECT_EQ(native.front(), supported[2]); // Canonical CZ handle.
  size_t nameSize = 0;
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_query_operation_property(
                session, supported.front(), 0, nullptr, 0, nullptr,
                QDMI_OPERATION_PROPERTY_NAME, 0, nullptr, &nameSize),
            QDMI_SUCCESS);
  EXPECT_EQ(nameSize, 2U); // "h" plus the null terminator.
  EXPECT_GE(stubPtr->calls(), 4U);
}

TEST_F(AmazonBraketQDMILocalJobTest,
       ConcurrentFirstQueriesPublishOneArchitecture) {
  Aws::Braket::Model::GetDeviceResult result;
  result.SetDeviceName("Garnet");
  result.SetProviderName("IQM");
  result.SetDeviceType(Aws::Braket::Model::DeviceType::QPU);
  result.SetDeviceStatus(Aws::Braket::Model::DeviceStatus::ONLINE);
  result.SetDeviceCapabilities(
      std::string{amazon::braket::qdmi::test::IQM_GARNET}.c_str());
  auto stub = std::make_unique<ConcurrentGetDeviceClient>(std::move(result));
  auto* const stubPtr = stub.get();
  AMAZON_BRAKET_QDMI_Device_Session_TestAccess::setClient(session,
                                                          std::move(stub));

  const auto queryNative = [this] {
    std::array<AMAZON_BRAKET_QDMI_Operation, 2> operations{};
    const auto status = static_cast<QDMI_STATUS>(
        AMAZON_BRAKET_QDMI_device_session_query_device_property(
            session, QDMI_DEVICE_PROPERTY_OPERATIONS, sizeof(operations),
            static_cast<void*>(operations.data()), nullptr));
    return std::pair{status, operations};
  };

  auto first = std::async(std::launch::async, queryNative);
  ASSERT_TRUE(stubPtr->waitForFirstCall());
  auto second = std::async(std::launch::async, queryNative);
  const auto [firstStatus, firstOperations] = first.get();
  const auto [secondStatus, secondOperations] = second.get();

  ASSERT_EQ(firstStatus, QDMI_SUCCESS);
  ASSERT_EQ(secondStatus, QDMI_SUCCESS);
  ASSERT_EQ(firstOperations, secondOperations);
  for (auto* const operation : firstOperations) {
    size_t nameSize = 0;
    EXPECT_EQ(AMAZON_BRAKET_QDMI_device_session_query_operation_property(
                  session, operation, 0, nullptr, 0, nullptr,
                  QDMI_OPERATION_PROPERTY_NAME, 0, nullptr, &nameSize),
              QDMI_SUCCESS);
    EXPECT_GT(nameSize, 1U);
  }
}

// =============================================================================
// AmazonBraketQDMILocalJobTest — job getResults() local error paths
// =============================================================================

// getResults() on a non-DONE job → BADSTATE.
TEST_F(AmazonBraketQDMILocalJobTest, JobGetResultsNotDone) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_get_results(
                freshJob, QDMI_JOB_RESULT_SHOTS, 0, nullptr, nullptr),
            QDMI_ERROR_BADSTATE);
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}

// result == QDMI_JOB_RESULT_MAX is checked before the status check →
// INVALIDARGUMENT.
TEST_F(AmazonBraketQDMILocalJobTest, JobGetResultsInvalidResultType) {
  AMAZON_BRAKET_QDMI_Device_Job freshJob = nullptr;
  ASSERT_EQ(
      AMAZON_BRAKET_QDMI_device_session_create_device_job(session, &freshJob),
      QDMI_SUCCESS);
  EXPECT_EQ(AMAZON_BRAKET_QDMI_device_job_get_results(
                freshJob, QDMI_JOB_RESULT_MAX, 0, nullptr, nullptr),
            QDMI_ERROR_INVALIDARGUMENT);
  AMAZON_BRAKET_QDMI_device_job_free(freshJob);
}
