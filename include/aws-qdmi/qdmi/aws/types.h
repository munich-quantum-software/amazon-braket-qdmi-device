/** @file
 * @brief Defines AWS Braket-specific types for the QDMI device interface.
 */

#pragma once

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
typedef struct AWS_QDMI_Site_impl_d* AWS_QDMI_Site;

/**
 * @brief A handle for an operation on an AWS Braket device.
 * @details An opaque pointer to an AWS-specific implementation of the QDMI
 * operation concept. An operation represents a quantum gate or instruction.
 */
typedef struct AWS_QDMI_Operation_impl_d* AWS_QDMI_Operation;

// NOLINTEND(modernize-use-using)

#ifdef __cplusplus
} // extern "C"
#endif
