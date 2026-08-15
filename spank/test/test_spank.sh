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

device_arn="${AMAZON_BRAKET_TEST_DEVICE_ARN}"
credentials_file=$(mktemp)
trap 'rm -f "${credentials_file}"' EXIT
cat >"${credentials_file}" <<'EOF'
[default]
aws_access_key_id=explicit-test-access-key
aws_secret_access_key=explicit-test-secret-key
EOF
chmod 600 "${credentials_file}"

echo "=== Verifying an inactive plugin leaves ordinary jobs alone ==="
srun --partition=debug --immediate=5 /bin/true

echo "=== Verifying SPANK options and injected job environment ==="
job_environment=$(
  env \
    -u AWS_ACCESS_KEY_ID \
    -u AWS_SECRET_ACCESS_KEY \
    -u AWS_SESSION_TOKEN \
    AWS_REGION=invalid-region \
    srun \
    --partition=debug \
    --immediate=5 \
    --amazon-braket-device-arn="${device_arn}" \
    --amazon-braket-region=us-east-1 \
    --amazon-braket-reservation-arn=arn:aws:braket:us-east-1:123456789012:reservation/test \
    --amazon-braket-credentials-file="${credentials_file}" \
    /usr/bin/env
)
grep -Fxq "AMAZON_BRAKET_DEVICE_ARN=${device_arn}" <<<"${job_environment}"
grep -Fxq "AWS_REGION=us-east-1" <<<"${job_environment}"
grep -Fxq \
  "AMAZON_BRAKET_RESERVATION_ARN=arn:aws:braket:us-east-1:123456789012:reservation/test" \
  <<<"${job_environment}"
grep -Fxq "AWS_SHARED_CREDENTIALS_FILE=${credentials_file}" <<<"${job_environment}"

echo "=== Verifying the submitted environment overrides administrator defaults ==="
job_environment=$(
  AMAZON_BRAKET_DEVICE_ARN="${device_arn}" srun \
    --partition=debug \
    --immediate=5 \
    /usr/bin/env
)
grep -Fxq "AMAZON_BRAKET_DEVICE_ARN=${device_arn}" <<<"${job_environment}"
grep -Fxq "AWS_REGION=us-east-1" <<<"${job_environment}"

echo "=== Verifying environment activation and administrator defaults ==="
job_environment=$(
  env -u AWS_REGION \
    AMAZON_BRAKET_DEVICE_ARN="${device_arn}" \
    srun \
    --partition=debug \
    --immediate=5 \
    /usr/bin/env
)
grep -Fxq "AMAZON_BRAKET_DEVICE_ARN=${device_arn}" <<<"${job_environment}"
grep -Fxq "AWS_REGION=us-west-2" <<<"${job_environment}"

echo "=== Verifying submitted container credentials reach the provider chain ==="
env \
  -u AWS_ACCESS_KEY_ID \
  -u AWS_SECRET_ACCESS_KEY \
  -u AWS_SESSION_TOKEN \
  AWS_CONTAINER_CREDENTIALS_FULL_URI=http://127.0.0.1:18080/credentials \
  srun \
  --partition=debug \
  --immediate=5 \
  --amazon-braket-device-arn="${device_arn}" \
  /bin/true

echo "=== Verifying an unavailable device rejects the job ==="
if srun \
  --partition=debug \
  --immediate=5 \
  --amazon-braket-device-arn=arn:aws:braket:us-east-1::device/quantum-simulator/amazon/offline-device \
  /bin/true; then
  echo "Unavailable Amazon Braket device unexpectedly passed validation" >&2
  exit 1
fi
