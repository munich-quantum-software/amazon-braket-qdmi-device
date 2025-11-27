/*------------------------------------------------------------------------------
Copyright 2025 AWS QDMI Device Implementation

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.

SPDX-License-Identifier: MIT
------------------------------------------------------------------------------*/

/** @file
 * @brief Defines AWS Braket-specific types for the QDMI device interface.
 */

#pragma once

/* AWS-specific version for this implementation */
#define AWS_QDMI_VERSION "0.1.0"

#ifdef __cplusplus
extern "C" {
#endif

// The following clang-tidy warning cannot be addressed because this header is
// used from both C and C++ code.
// NOLINTBEGIN(modernize-use-using)

/**
 * @brief A handle for a site on an AWS Braket device.
 * @details An opaque pointer to an AWS-specific implementation of the QDMI 
 * site concept. A site represents a physical qubit location on the device.
 */
typedef struct AWS_QDMI_Site_impl_d *AWS_QDMI_Site;

/**
 * @brief A handle for an operation on an AWS Braket device.
 * @details An opaque pointer to an AWS-specific implementation of the QDMI 
 * operation concept. An operation represents a quantum gate or instruction.
 */
typedef struct AWS_QDMI_Operation_impl_d *AWS_QDMI_Operation;

// NOLINTEND(modernize-use-using)

#ifdef __cplusplus
} // extern "C"
#endif
