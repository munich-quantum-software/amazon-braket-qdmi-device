/*
 * Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
 * All rights reserved.
 *
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License
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
 * Format: arn:aws:braket:<region>::device/<provider>/<device-name>
 * or: arn:aws:braket:::<region>:device/<provider>/<device-name>
 *
 * Examples:
 * - "arn:aws:braket:::device/quantum-simulator/amazon/sv1"
 * - "arn:aws:braket:us-east-1::device/qpu/ionq/Harmony"
 * - "arn:aws:braket:eu-north-1::device/qpu/iqm/Garnet"
 */
#define QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN                                \
  QDMI_DEVICE_SESSION_PARAMETER_CUSTOM1

/**
 * @brief `char*` (string) AWS Region.
 * @details Optional parameter. If not set, the region is extracted from the
 * device ARN or uses AWS SDK default resolution (environment variables, AWS
 * config files, etc.).
 *
 * Examples: "us-east-1", "us-west-2", "eu-north-1"
 */
#define QDMI_DEVICE_SESSION_PARAMETER_REGION                                   \
  QDMI_DEVICE_SESSION_PARAMETER_CUSTOM2

/**
 * @brief `char*` (string) AWS Access Key ID.
 * @details Optional parameter for explicit credential specification.
 * If not set, uses default AWS credential chain.
 * Can be used together with AWS_SECRET_ACCESS_KEY and AWS_SESSION_TOKEN.
 *
 * Example: "AKIAIOSFODNN7EXAMPLE"
 */
#define QDMI_DEVICE_SESSION_PARAMETER_AWS_ACCESS_KEY_ID                        \
  QDMI_DEVICE_SESSION_PARAMETER_CUSTOM3

/**
 * @brief `char*` (string) AWS Secret Access Key.
 * @details Optional parameter for explicit credential specification.
 * If not set, uses default AWS credential chain.
 * Must be used together with AWS_ACCESS_KEY_ID.
 *
 * Example: "wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY"
 */
#define QDMI_DEVICE_SESSION_PARAMETER_AWS_SECRET_ACCESS_KEY                    \
  QDMI_DEVICE_SESSION_PARAMETER_CUSTOM4

/**
 * @brief `char*` (string) AWS Session Token.
 * @details Optional parameter for temporary credentials (STS, SSO).
 * Used together with AWS_ACCESS_KEY_ID and AWS_SECRET_ACCESS_KEY.
 * If not set but access key/secret are provided, uses long-term credentials.
 *
 * Example: "IQoJb3JpZ2luX2VjEOT//////////..."
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
 *
 * Example: "amazon-braket-my-bucket"
 */
#define QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3BUCKET                               \
  QDMI_DEVICE_JOB_PARAMETER_CUSTOM1

/**
 * @brief `char*` (string) S3 prefix for this job's results.
 * @details Optional parameter. If not set, a timestamp is used as the prefix
 * (milliseconds since epoch). The prefix organizes results within the bucket.
 *
 * Example: "my-experiment/run-42/"
 */
#define QDMI_DEVICE_JOB_PARAMETER_OUTPUTS3PREFIX                               \
  QDMI_DEVICE_JOB_PARAMETER_CUSTOM2

// CUSTOM3-5 reserved for future use
