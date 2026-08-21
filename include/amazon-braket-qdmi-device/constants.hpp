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

/** @file
 * @brief Amazon Braket-specific constants and custom QDMI parameters
 */

#pragma once

#include "amazon_braket_qdmi/device.h"

/** Environment variables used as fallback session parameters. */
#define AMAZON_BRAKET_QDMI_DEVICE_ENV_DEVICE_ARN "AMAZON_BRAKET_DEVICE_ARN"
#define AMAZON_BRAKET_QDMI_DEVICE_ENV_REGION "AWS_REGION"
#define AMAZON_BRAKET_QDMI_DEVICE_ENV_RESERVATION_ARN                          \
  "AMAZON_BRAKET_RESERVATION_ARN"
#define AMAZON_BRAKET_QDMI_DEVICE_ENV_TASK_RESULTS_S3_URI                      \
  "AMZN_BRAKET_TASK_RESULTS_S3_URI"

/**
 * @brief OpenQASM operations accepted by the Amazon Braket device.
 * @details Returns an array of `AMAZON_BRAKET_QDMI_Operation` handles. For a
 * QPU, this set can be broader than the hardware-native operations returned by
 * `QDMI_DEVICE_PROPERTY_OPERATIONS`. Equal operation names use equal handles
 * in both views.
 */
#define AMAZON_BRAKET_QDMI_DEVICE_PROPERTY_SUPPORTEDOPERATIONS                 \
  QDMI_DEVICE_PROPERTY_CUSTOM1

/**
 * @brief Amazon Braket-specific device session parameters.
 * @details These extend the standard QDMI parameters using the CUSTOM slots.
 */

/**
 * @brief `char*` (string) Amazon Braket device ARN.
 * @details Required parameter. This service-specific name aliases the standard
 * QDMI base URL parameter, which carries the device ARN for this
 * implementation.
 */
#define AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN                  \
  QDMI_DEVICE_SESSION_PARAMETER_BASEURL

/**
 * @brief `char*` (string) AWS Region.
 * @details Optional parameter. If not set, the region is extracted from the
 * device ARN or uses us-east-1 as a default.
 */
#define AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_REGION                     \
  QDMI_DEVICE_SESSION_PARAMETER_CUSTOM2

/**
 * @brief `char*` (string) AWS Braket Reservation ARN.
 * @details Optional parameter. When set, device status reporting ignores the
 * public execution-window availability check because the session targets a
 * reserved time window. Jobs created from the session inherit this reservation
 * ARN unless a job-level reservation ARN is set.
 */
#define AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_RESERVATION_ARN            \
  QDMI_DEVICE_SESSION_PARAMETER_CUSTOM3

/**
 * @brief Amazon Braket-specific device job parameters.
 * @details These extend the standard QDMI job parameters using the CUSTOM
 * slots.
 */

/**
 * @brief `char*` (string) S3 destination for this job's results.
 * @details Optional complete URI, for example `s3://bucket/prefix`. If it is
 * not set, the provider uses `AMZN_BRAKET_TASK_RESULTS_S3_URI`, then the
 * standard regional Braket default bucket with the prefix `tasks`.
 */
#define AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3URI                    \
  QDMI_DEVICE_JOB_PARAMETER_CUSTOM1

/**
 * @brief `char*` (string) AWS Braket Reservation ARN.
 * @details Optional parameter. When set, the quantum task is submitted into
 * the specified reserved time window instead of the public on-demand queue.
 * The reservation must be in the same region as the device.
 */
#define AMAZON_BRAKET_QDMI_DEVICE_JOB_PARAMETER_RESERVATION_ARN                \
  QDMI_DEVICE_JOB_PARAMETER_CUSTOM3

/**
 * @brief `char*` (string) resolved S3 directory for this job's results.
 * @details The value is available after Amazon Braket returns the task metadata
 * and has the form `s3://bucket/directory`.
 */
#define AMAZON_BRAKET_QDMI_DEVICE_JOB_PROPERTY_OUTPUTS3URI                     \
  QDMI_DEVICE_JOB_PROPERTY_CUSTOM1
