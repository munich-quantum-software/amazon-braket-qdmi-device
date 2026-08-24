#!/usr/bin/env bash
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: GPL-3.0-or-later
#
# This program is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
# Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program. If not, see <https://www.gnu.org/licenses/>.

set -euo pipefail

readonly partition=debug
readonly reservation_arn=arn:aws:braket:us-east-1:123456789012:reservation/test

run() {
  srun --partition="${partition}" --immediate=5 "$@"
}

expect_not_executed() {
  local sentinel=/tmp/spank-rejection-sentinel
  rm -f "${sentinel}"
  "$@" /usr/bin/touch "${sentinel}" || true
  if [[ -e "${sentinel}" ]]; then
    echo "Rejected workload unexpectedly executed: $*" >&2
    exit 1
  fi
}

echo "=== Verifying ordinary jobs remain untouched ==="
run /bin/true
ordinary_environment=$(run --licenses=ordinary.one /usr/bin/env)
! grep -Fq "AWS_PROFILE=" <<<"${ordinary_environment}"
! grep -Fq "MQT_CORE_QDMI_CONFIG_FILE=" <<<"${ordinary_environment}"

echo "=== Verifying concrete catalogue licenses activate injection ==="
for license in amazon.braket.sv1 amazon.braket.dm1; do
  job_environment=$(run --licenses="${license}" /usr/bin/env)
  grep -Eq "^SLURM_JOB_LICENSES=${license}(:1)?$" <<<"${job_environment}"
  grep -Fxq "AWS_PROFILE=administrator-default" <<<"${job_environment}"
  grep -Fxq \
    "MQT_CORE_QDMI_CONFIG_FILE=/usr/local/lib/amazon-braket-qdmi-device.qdmi.json" \
    <<<"${job_environment}"
done

echo "=== Verifying option, submitted environment, and default precedence ==="
job_environment=$(
  AWS_PROFILE=submitted-profile \
    run \
      --licenses=amazon.braket.sv1 \
      --amazon-braket-qdmi-config-file=/tmp/option-catalogue.json \
      --amazon-braket-profile=option-profile \
      --amazon-braket-config-file=/workspace/spank/test/aws_config \
      --amazon-braket-shared-credentials-file=/tmp/reference-only \
      --amazon-braket-task-results-s3-uri=s3://option-bucket/tasks \
      --amazon-braket-reservation-arn="${reservation_arn}" \
      /usr/bin/env
)
grep -Fxq "MQT_CORE_QDMI_CONFIG_FILE=/tmp/option-catalogue.json" \
  <<<"${job_environment}"
grep -Fxq "AWS_PROFILE=option-profile" <<<"${job_environment}"
grep -Fxq "AWS_CONFIG_FILE=/workspace/spank/test/aws_config" <<<"${job_environment}"
grep -Fxq "AWS_SHARED_CREDENTIALS_FILE=/tmp/reference-only" <<<"${job_environment}"
grep -Fxq "AMZN_BRAKET_TASK_RESULTS_S3_URI=s3://option-bucket/tasks" \
  <<<"${job_environment}"
grep -Fxq "AMAZON_BRAKET_RESERVATION_ARN=${reservation_arn}" \
  <<<"${job_environment}"

job_environment=$(
  AWS_PROFILE=submitted-profile \
    run --licenses=amazon.braket.sv1 /usr/bin/env
)
grep -Fxq "AWS_PROFILE=submitted-profile" <<<"${job_environment}"

echo "=== Verifying malformed contexts fail closed ==="
oversized_profile=$(printf 'x%.0s' {1..5000})
export AWS_PROFILE="${oversized_profile}"
expect_not_executed run --licenses=amazon.braket.sv1
unset AWS_PROFILE

expect_not_executed run --amazon-braket-profile=option-profile
expect_not_executed run --licenses=amazon.braket.default

echo "=== Verifying device selection and credentials are not plugin options ==="
help=$(srun --help)
! grep -Fq -- "--amazon-braket-access-key" <<<"${help}"
! grep -Fq -- "--amazon-braket-secret-key" <<<"${help}"
! grep -Fq -- "--amazon-braket-device-arn" <<<"${help}"
! grep -Fq -- "--amazon-braket-region" <<<"${help}"
