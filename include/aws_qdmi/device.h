/*------------------------------------------------------------------------------
Copyright 2025 AWS QDMI Device Implementation

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.

SPDX-License-Identifier: MIT
------------------------------------------------------------------------------*/

/** @file
 * @brief AWS Braket implementation of the QDMI device interface.
 * 
 * This header provides an AWS Braket-specific implementation of the QDMI 
 * specification. It enables any QDMI-compliant quantum software to target 
 * AWS Braket devices by simply linking against this library.
 * 
 * KEY CONCEPTS: 
 * - ARN: Amazon Resource Name - unique identifier for AWS resources
 * - Session: A connection to a quantum device or simulator
 * - Job: A quantum circuit/program submitted for execution (OpenQASM only)
 * - Site: A physical qubit (quantum bit) in the processor
 * - Operation: A quantum gate that can be applied to qubits
 * 
 * TYPICAL WORKFLOW:
 * 1. Initialize: AWS_QDMI_device_initialize()
 * 2. Create session: AWS_QDMI_device_session_alloc()
 * 3. Configure session: AWS_QDMI_device_session_set_parameter() (region, device ARN)
 * 4. Initialize session: AWS_QDMI_device_session_init()
 * 5. Query device: AWS_QDMI_device_session_query_device_property()
 * 6. Create job: AWS_QDMI_device_session_create_device_job()
 * 7. Configure job: AWS_QDMI_device_job_set_parameter() (circuit, shots)
 * 8. Submit job: AWS_QDMI_device_job_submit()
 * 9. Wait for completion: AWS_QDMI_device_job_wait()
 * 10. Get results: AWS_QDMI_device_job_get_results()
 * 11. Cleanup: AWS_QDMI_device_job_free(), AWS_QDMI_device_session_free()
 * 
 * QDMI Project: https://github.com/Munich-Quantum-Software-Stack/QDMI
 */

#pragma once

#ifdef __cplusplus
#include <cstddef>

extern "C" {
#else
#include <stddef.h>
#endif

// The following clang-tidy warning cannot be addressed because this header is
// used from both C and C++ code.
// NOLINTBEGIN(performance-enum-size,modernize-use-using,modernize-redundant-void-arg)

/* Include QDMI standard headers - they have their own extern "C" guards */
#include "qdmi/constants.h" // IWYU pragma: export
#include "qdmi/types.h"     // IWYU pragma: export

/* Include AWS-specific types */
#include "aws_qdmi/types.h" // IWYU pragma: export

/** @defgroup aws_qdmi_device_interface AWS QDMI Device Interface
 *  @brief AWS Braket implementation of the QDMI device interface.
 *  @{
 */

/* ============================================================================
 * AWS Braket-Specific Extensions
 * ============================================================================
 * These extend the standard QDMI types with AWS-specific functionality.
 */

/* AWS-specific device properties (extensions beyond standard QDMI)
 * These use values 100+ to avoid conflicts with standard QDMI properties */
#define QDMI_DEVICE_PROPERTY_PROVIDER 100   /* string: Hardware provider ("IonQ", "Rigetti", etc.) */
#define QDMI_DEVICE_PROPERTY_DEVICEARN 101  /* string: AWS ARN of the device */
#define QDMI_DEVICE_PROPERTY_DEVICETYPE 102 /* string: Device type ("QPU", "SIMULATOR") */

/* AWS-specific program format */
#define QDMI_PROGRAM_FORMAT_BRAKET_IR 100   /* Braket's native IR format (JSON-based) */

/* AWS-specific session parameters - use 100+ to avoid conflicts */
#define QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN 100 /* string: Device ARN (required, region extracted automatically) */
#define QDMI_DEVICE_SESSION_PARAMETER_S3BUCKET 101  /* string: S3 bucket for results (required for job submission) */
#define QDMI_DEVICE_SESSION_PARAMETER_REGION 102    /* string: AWS region override (optional, auto-extracted from ARN) */

/* AWS-specific job property */
#define QDMI_DEVICE_JOB_PROPERTY_TASKARN 100       /* string: AWS Braket task ARN */

/* ============================================================================
 * AWS QDMI Device Session Interface
 * ============================================================================
 */

/**
 * @brief A handle for a device session.
 * @details An opaque pointer to a type defined by the AWS Braket device 
 * implementation that encapsulates all information about a session.
 */
typedef struct AWS_QDMI_Device_Session_impl_d *AWS_QDMI_Device_Session;

/**
 * @brief A handle for a device job.
 * @details An opaque pointer to a type defined by the AWS Braket device 
 * implementation that encapsulates all information about a job.
 */
typedef struct AWS_QDMI_Device_Job_impl_d *AWS_QDMI_Device_Job;

/* ============================================================================
 * AWS Braket QDMI Implementation API
 * ============================================================================
 * The following functions implement the QDMI specification for AWS Braket.
 */

/**
 * @brief Initialize the AWS QDMI device.
 * @return QDMI_SUCCESS on success, error code otherwise.
 */
int AWS_QDMI_device_initialize(void);

/**
 * @brief Finalize the AWS QDMI device.
 * @return QDMI_SUCCESS on success, error code otherwise.
 */
int AWS_QDMI_device_finalize(void);

/**
 * @brief Allocate a new device session.
 * @param session Pointer to store the allocated session.
 * @return QDMI_SUCCESS on success, error code otherwise.
 */
int AWS_QDMI_device_session_alloc(AWS_QDMI_Device_Session* session);

/**
 * @brief Initialize a device session.
 * @param session The session to initialize.
 * @return QDMI_SUCCESS on success, error code otherwise.
 */
int AWS_QDMI_device_session_init(AWS_QDMI_Device_Session session);

/**
 * @brief Free a device session.
 * @param session The session to free.
 */
void AWS_QDMI_device_session_free(AWS_QDMI_Device_Session session);

/**
 * @brief Set a session parameter.
 * @param session The session.
 * @param param The parameter to set.
 * @param size Size of the value.
 * @param value Pointer to the value.
 * @return QDMI_SUCCESS on success, error code otherwise.
 */
int AWS_QDMI_device_session_set_parameter(AWS_QDMI_Device_Session session,
                                           QDMI_Device_Session_Parameter param,
                                           size_t size, const void* value);

/**
 * @brief Query a device property through a session.
 * @param session The session.
 * @param prop The property to query.
 * @param size Size of the value buffer.
 * @param value Pointer to store the value (can be NULL to query size).
 * @param sizeRet Pointer to store the actual size (can be NULL).
 * @return QDMI_SUCCESS on success, error code otherwise.
 */
int AWS_QDMI_device_session_query_device_property(
    AWS_QDMI_Device_Session session, QDMI_Device_Property prop, size_t size,
    void* value, size_t* sizeRet);

/**
 * @brief Create a device job within a session.
 * @param session The session.
 * @param job Pointer to store the created job.
 * @return QDMI_SUCCESS on success, error code otherwise.
 */
int AWS_QDMI_device_session_create_device_job(AWS_QDMI_Device_Session session,
                                               AWS_QDMI_Device_Job* job);

/**
 * @brief Free a device job.
 * @param job The job to free.
 */
void AWS_QDMI_device_job_free(AWS_QDMI_Device_Job job);

/**
 * @brief Set a job parameter.
 * @param job The job.
 * @param param The parameter to set.
 * @param size Size of the value.
 * @param value Pointer to the value.
 * @return QDMI_SUCCESS on success, error code otherwise.
 */
int AWS_QDMI_device_job_set_parameter(AWS_QDMI_Device_Job job,
                                       QDMI_Device_Job_Parameter param,
                                       size_t size, const void* value);

/**
 * @brief Query a job property.
 * @param job The job.
 * @param prop The property to query.
 * @param size Size of the value buffer.
 * @param value Pointer to store the value (can be NULL to query size).
 * @param sizeRet Pointer to store the actual size (can be NULL).
 * @return QDMI_SUCCESS on success, error code otherwise.
 */
int AWS_QDMI_device_job_query_property(AWS_QDMI_Device_Job job,
                                        QDMI_Device_Job_Property prop,
                                        size_t size, void* value,
                                        size_t* sizeRet);

/**
 * @brief Submit a job to the device.
 * @param job The job to submit.
 * @return QDMI_SUCCESS on success, error code otherwise.
 */
int AWS_QDMI_device_job_submit(AWS_QDMI_Device_Job job);

/**
 * @brief Cancel a running job.
 * @param job The job to cancel.
 * @return QDMI_SUCCESS on success, error code otherwise.
 */
int AWS_QDMI_device_job_cancel(AWS_QDMI_Device_Job job);

/**
 * @brief Check the status of a job.
 * @param job The job.
 * @param status Pointer to store the job status.
 * @return QDMI_SUCCESS on success, error code otherwise.
 */
int AWS_QDMI_device_job_check(AWS_QDMI_Device_Job job,
                               QDMI_Job_Status* status);

/**
 * @brief Wait for a job to complete.
 * @param job The job.
 * @param timeout Timeout in milliseconds (0 = infinite).
 * @return QDMI_SUCCESS on success, error code otherwise.
 */
int AWS_QDMI_device_job_wait(AWS_QDMI_Device_Job job, size_t timeout);

/**
 * @brief Get job results.
 * @param job The job.
 * @param result The result type to retrieve.
 * @param size Size of the data buffer.
 * @param data Pointer to store the data (can be NULL to query size).
 * @param sizeRet Pointer to store the actual size (can be NULL).
 * @return QDMI_SUCCESS on success, error code otherwise.
 */
int AWS_QDMI_device_job_get_results(AWS_QDMI_Device_Job job,
                                     QDMI_Job_Result result, size_t size,
                                     void* data, size_t* sizeRet);

/**
 * @brief Query a site property.
 * @param session The session.
 * @param site The site to query.
 * @param prop The property to query.
 * @param size Size of the value buffer.
 * @param value Pointer to store the value (can be NULL to query size).
 * @param sizeRet Pointer to store the actual size (can be NULL).
 * @return QDMI_SUCCESS on success, error code otherwise.
 */
int AWS_QDMI_device_session_query_site_property(
    AWS_QDMI_Device_Session session, AWS_QDMI_Site site,
    QDMI_Site_Property prop, size_t size, void* value, size_t* sizeRet);

/**
 * @brief Query an operation property.
 * @param session The session.
 * @param operation The operation to query.
 * @param num_sites Number of sites (for site-specific queries).
 * @param sites Array of sites (can be NULL).
 * @param num_params Number of parameters (for parameter-specific queries).
 * @param params Array of parameters (can be NULL).
 * @param prop The property to query.
 * @param size Size of the value buffer.
 * @param value Pointer to store the value (can be NULL to query size).
 * @param sizeRet Pointer to store the actual size (can be NULL).
 * @return QDMI_SUCCESS on success, error code otherwise.
 */
int AWS_QDMI_device_session_query_operation_property(
    AWS_QDMI_Device_Session session, AWS_QDMI_Operation operation,
    size_t num_sites, const AWS_QDMI_Site* sites, size_t num_params,
    const double* params, QDMI_Operation_Property prop, size_t size,
    void* value, size_t* sizeRet);

#ifdef __cplusplus
}
#endif
