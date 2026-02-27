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

#include "amazon_braket_qdmi/constants.h"

/**
 * @brief Amazon Braket-specific device session parameters.
 * @details These extend the standard QDMI parameters using the CUSTOM slots.
 */

/**
 * @brief `char*` (string) Amazon Braket Device ARN.
 * @details Required parameter. Must be set before session initialization.
 */
#define QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN                                \
  QDMI_DEVICE_SESSION_PARAMETER_CUSTOM1

/**
 * @brief `char*` (string) AWS Region.
 * @details Optional parameter. If not set, the region is extracted from the
 * device ARN or uses us-east-1 as a default.
 */
#define QDMI_DEVICE_SESSION_PARAMETER_REGION                                   \
  QDMI_DEVICE_SESSION_PARAMETER_CUSTOM2

/**
 * @brief `char*` (string) AWS Access Key ID.
 * @details Optional parameter for explicit credential specification.
 * Can be used together with AWS_SECRET_ACCESS_KEY and AWS_SESSION_TOKEN.
 */
#define QDMI_DEVICE_SESSION_PARAMETER_AWS_ACCESS_KEY_ID                        \
  QDMI_DEVICE_SESSION_PARAMETER_CUSTOM3

/**
 * @brief `char*` (string) AWS Secret Access Key.
 * @details Optional parameter for explicit credential specification.
 * Must be used together with AWS_ACCESS_KEY_ID.
 */
#define QDMI_DEVICE_SESSION_PARAMETER_AWS_SECRET_ACCESS_KEY                    \
  QDMI_DEVICE_SESSION_PARAMETER_CUSTOM4

/**
 * @brief `char*` (string) AWS Session Token.
 * @details Optional parameter for temporary credentials (STS, SSO).
 * Used together with AWS_ACCESS_KEY_ID and AWS_SECRET_ACCESS_KEY.
 * If not set but access key/secret are provided, uses long-term credentials.
 */
#define QDMI_DEVICE_SESSION_PARAMETER_AWS_SESSION_TOKEN                        \
  QDMI_DEVICE_SESSION_PARAMETER_CUSTOM5

/**
 * @brief Amazon Braket-specific device job parameters.
 * @details These extend the standard QDMI job parameters using the CUSTOM
 * slots.
 */

/**
 * @brief `char*` (string) S3 bucket for this job's results.
 * @details Required parameter. Must be set before submitting the QDMI job
 * (which creates an Amazon Braket quantum task).
 * The bucket must be in the same region as the device and accessible with
 * the configured AWS credentials.
 */
#define QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3BUCKET                               \
  QDMI_DEVICE_JOB_PARAMETER_CUSTOM1

/**
 * @brief `char*` (string) S3 prefix for this job's results.
 * @details Optional parameter. If not set, a timestamp is used as the prefix
 * (milliseconds since epoch). The prefix organizes results within the bucket.
 */
#define QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3PREFIX                               \
  QDMI_DEVICE_JOB_PARAMETER_CUSTOM2

/**
 * @brief `char*` (string) AWS Braket Reservation ARN.
 * @details Optional parameter. When set, the quantum task is submitted into
 * the specified reserved time window instead of the public on-demand queue.
 * The reservation must be in the same region as the device.
 */
#define QDMI_DEVICE_JOB_PARAMETER_RESERVATION_ARN                              \
  QDMI_DEVICE_JOB_PARAMETER_CUSTOM3

// CUSTOM4-5 reserved for future use
