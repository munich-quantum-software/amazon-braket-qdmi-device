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
  cat /var/log/slurmctld.log /var/log/slurmd.log /tmp/mock-braket-backend.log || true
}
trap show_logs ERR

echo "=== Starting local Amazon Braket fixture ==="
/opt/spank-venv/bin/python /workspace/spank/test/mock_braket_backend.py \
  >/tmp/mock-braket-backend.log 2>&1 &
mock_pid=$!
trap 'kill "${mock_pid}" 2>/dev/null || true' EXIT
for _ in {1..50}; do
  if curl --fail --silent http://127.0.0.1:18080/health >/dev/null; then
    break
  fi
  sleep 0.1
done
curl --fail --silent http://127.0.0.1:18080/health >/dev/null

echo "=== Starting Munge and Slurm ==="
sudo service munge start
sudo /usr/sbin/slurmctld
sudo mkdir -p /sys/fs/cgroup/system.slice
# Provider configuration belongs to the job. Keep it out of Slurm daemons.
sudo env \
  -u AWS_ACCESS_KEY_ID \
  -u AWS_SECRET_ACCESS_KEY \
  -u AWS_SESSION_TOKEN \
  -u AWS_PROFILE \
  -u AWS_CONFIG_FILE \
  -u AWS_SHARED_CREDENTIALS_FILE \
  -u AWS_ENDPOINT_URL_BRAKET \
  -u MQT_CORE_QDMI_CONFIG_FILE \
  AWS_EC2_METADATA_DISABLED=true \
  /usr/sbin/slurmd -N localhost

echo "=== Waiting for the local Slurm node ==="
for _ in {1..30}; do
  if sinfo -h -n localhost -o "%t" | grep -qE "idle|alloc"; then
    break
  fi
  sudo scontrol update nodename=localhost state=resume 2>/dev/null || true
  sleep 1
done
sinfo -h -n localhost -o "%t" | grep -qE "idle|alloc"

"$@"
