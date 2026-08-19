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

expect_rejection() {
  if "$@"; then
    echo "Command unexpectedly succeeded: $*" >&2
    exit 1
  fi
}

/workspace/spank/test/test_spank.sh

echo "=== Verifying the plugin remains a standalone environment injector ==="
plugin_path=$(find /usr/lib -name amazon-braket-qdmi-spank.so -print -quit)
[[ -n "${plugin_path}" ]]
! ldd "${plugin_path}" | grep -Eq 'amazon-braket|aws-cpp-sdk|mqt-core'

export AWS_EC2_METADATA_DISABLED=true
export AWS_ENDPOINT_URL_BRAKET=http://127.0.0.1:18080
readonly connector=/workspace/spank/test/connector_probe.py
readonly python=/opt/spank-venv/bin/python

echo "=== Verifying MQT Core opens and authenticates the licensed device ==="
env -u AWS_ACCESS_KEY_ID -u AWS_SECRET_ACCESS_KEY -u AWS_SESSION_TOKEN \
  -u AWS_SHARED_CREDENTIALS_FILE \
  srun \
    --partition=debug \
    --immediate=5 \
    --licenses=amazon.braket.sv1 \
    --amazon-braket-profile=spank-test \
    --amazon-braket-config-file=/workspace/spank/test/aws_config \
    "${python}" "${connector}"

echo "=== Verifying authentication failures stop the workload early ==="
rm -f /tmp/connector-sentinel
expect_rejection env \
  AWS_ACCESS_KEY_ID=invalid-access-key \
  AWS_SECRET_ACCESS_KEY=invalid-secret-key \
  AWS_SESSION_TOKEN=invalid-session-token \
  srun \
    --partition=debug \
    --immediate=5 \
    --licenses=amazon.braket.sv1 \
    /bin/bash -c \
    '"$1" "$2" && touch /tmp/connector-sentinel' _ "${python}" "${connector}"
[[ ! -e /tmp/connector-sentinel ]]

echo "=== Verifying retired devices fail the connector status check ==="
rm -f /tmp/connector-sentinel
expect_rejection env \
  -u AWS_ACCESS_KEY_ID -u AWS_SECRET_ACCESS_KEY -u AWS_SESSION_TOKEN \
  -u AWS_SHARED_CREDENTIALS_FILE \
  srun \
    --partition=debug \
    --immediate=5 \
    --licenses=amazon.braket.dm1 \
    --amazon-braket-profile=spank-test \
    --amazon-braket-config-file=/workspace/spank/test/aws_config \
    /bin/bash -c \
    '"$1" "$2" && touch /tmp/connector-sentinel' _ "${python}" "${connector}"
[[ ! -e /tmp/connector-sentinel ]]

echo "=== Verifying MQT Core owns compound-license validation ==="
expect_rejection env \
  -u AWS_ACCESS_KEY_ID -u AWS_SECRET_ACCESS_KEY -u AWS_SESSION_TOKEN \
  -u AWS_SHARED_CREDENTIALS_FILE \
  srun \
    --partition=debug \
    --immediate=5 \
    --licenses=amazon.braket.sv1,ordinary.one \
    --amazon-braket-profile=spank-test \
    --amazon-braket-config-file=/workspace/spank/test/aws_config \
    "${python}" "${connector}"

echo "=== Verifying signed fixture traffic ==="
fixture_state=$(curl --fail --silent http://127.0.0.1:18080/state)
FIXTURE_STATE="${fixture_state}" "${python}" -c '
import json
import os

state = json.loads(os.environ["FIXTURE_STATE"])
assert state["get_device"] >= 2, state
assert state["signed_requests"] >= 2, state
assert state["signature_failures"] >= 1, state
'

echo "=== Verifying configuration references stay out of Slurm daemons ==="
srun \
  --partition=debug \
  --immediate=5 \
  --licenses=amazon.braket.sv1 \
  --amazon-braket-profile=spank-test \
  /bin/sleep 5 &
step_job=$!
for _ in {1..50}; do
  if pgrep -x slurmstepd >/dev/null; then
    break
  fi
  sleep 0.1
done
for process in slurmd slurmstepd; do
  pid=$(pgrep -x "${process}" | head -1)
  [[ -n "${pid}" ]]
  daemon_environment=$(
    sudo /bin/sh -c 'tr "\0" "\n" <"$1"' _ "/proc/${pid}/environ"
  )
  ! grep -Eq \
    'AWS_PROFILE|AWS_CONFIG_FILE|AWS_SHARED_CREDENTIALS_FILE|AWS_ACCESS_KEY_ID|AWS_SECRET_ACCESS_KEY|AWS_SESSION_TOKEN|AMZN_BRAKET_TASK_RESULTS_S3_URI|AMAZON_BRAKET_RESERVATION_ARN|MQT_CORE_QDMI_CONFIG_FILE' \
    <<<"${daemon_environment}"
done
wait "${step_job}"

echo "=== All real-Slurm Amazon Braket tests passed ==="
