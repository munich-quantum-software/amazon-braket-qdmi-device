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

show_logs() {
  echo "=== slurmctld log ==="
  sudo cat /var/log/slurmctld.log || true
  echo "=== slurmd log ==="
  sudo cat /var/log/slurmd.log || true
  echo "=== local Amazon Braket fixture log ==="
  cat /tmp/mock-braket-backend.log || true
  if [[ -n "${bell_output:-}" && -f "${bell_output}" ]]; then
    echo "=== Bell circuit batch output ==="
    cat "${bell_output}" || true
  fi
}
trap show_logs ERR

# These credentials belong to the submitted job environment. slurmd was
# started before this script and therefore cannot satisfy credential validation
# from daemon state.
export AMAZON_BRAKET_TEST_DEVICE_ARN=arn:aws:braket:us-east-1::device/quantum-simulator/amazon/sv1
export AWS_ACCESS_KEY_ID=local-test-access-key
export AWS_SECRET_ACCESS_KEY=local-test-secret-key
export AWS_REGION=us-east-1
export AWS_EC2_METADATA_DISABLED=true
export AWS_ENDPOINT_URL_BRAKET=http://127.0.0.1:18080

echo "=== Verifying self-contained plugin installation ==="
plugin_path=$(find /usr/lib -name amazon-braket-qdmi-spank.so -print -quit)
if [[ -z "${plugin_path}" ]]; then
  echo "Amazon Braket SPANK plugin was not installed" >&2
  exit 1
fi
if ldd "${plugin_path}" | grep -Fq libamazon-braket-qdmi-device; then
  echo "SPANK plugin unexpectedly depends on the shared QDMI device" >&2
  exit 1
fi

/workspace/spank/test/test_spank.sh

echo "=== Running a Bell-circuit batch script through sbatch ==="
bell_output=$(mktemp)
timeout 60s sbatch \
  --wait \
  --output="${bell_output}" \
  /workspace/spank/test/bell_circuit_job.sh
grep -Fxq "Bell circuit prepared in Slurm batch job" "${bell_output}"

request_count=$(
  curl --fail --silent http://127.0.0.1:18080/request-count |
    sed -E 's/.*"count":([0-9]+).*/\1/'
)
if [[ "${request_count}" -lt 5 ]]; then
  echo "Expected at least five Amazon Braket device validations, got ${request_count}" >&2
  exit 1
fi

trap - ERR
echo "=== All real-Slurm SPANK tests passed ==="
