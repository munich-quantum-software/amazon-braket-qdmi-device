#ifndef AMAZON_BRAKET_QDMI_CONSTANTS_H
#define AMAZON_BRAKET_QDMI_CONSTANTS_H

/* ============================================================================
 * Amazon Braket-Specific Extensions
 * ============================================================================
 * These extend the standard QDMI types with AWS-specific functionality.
 */

/* AWS-specific device properties (extensions beyond standard QDMI)
 * These use values 100+ to avoid conflicts with standard QDMI properties */
#define AMAZON_BRAKET_PROPERTY_PROVIDER                                        \
  100 /* string: Hardware provider ("IonQ", "Rigetti", etc.) */
#define AMAZON_BRAKET_PROPERTY_DEVICEARN                                       \
  101 /* string: AWS ARN of the device                                         \
       */
#define AMAZON_BRAKET_PROPERTY_DEVICETYPE                                      \
  102 /* string: Device type ("QPU", "SIMULATOR") */

/* AWS-specific session parameters - use 100+ to avoid conflicts */
#define AMAZON_BRAKET_SESSION_PARAMETER_DEVICEARN                              \
  100 /* string: Device ARN (required, region extracted automatically) */
#define AMAZON_BRAKET_SESSION_PARAMETER_S3BUCKET                               \
  101 /* string: S3 bucket for results (required for job submission) */
#define AMAZON_BRAKET_SESSION_PARAMETER_REGION                                 \
  102 /* string: AWS region override (optional, auto-extracted from ARN) */

/* AWS-specific job property */
#define AMAZON_BRAKET_JOB_PROPERTY_TASKARN                                     \
  100 /* string: Amazon Braket task ARN */

#endif // AMAZON_BRAKET_QDMI_CONSTANTS_H
