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

echo "=== Starting local Amazon Braket fixture ==="
uv run --script /workspace/spank/test/mock_braket_backend.py \
  >/tmp/mock-braket-backend.log 2>&1 &
mock_pid=$!
trap 'kill "$mock_pid" 2>/dev/null || true' EXIT
for _ in {1..50}; do
  if curl --fail --silent http://127.0.0.1:18080/health >/dev/null; then
    break
  fi
  sleep 0.1
done
if ! curl --fail --silent http://127.0.0.1:18080/health >/dev/null; then
  echo "ERROR: Local Amazon Braket fixture did not start" >&2
  cat /tmp/mock-braket-backend.log >&2
  exit 1
fi

echo "=== Starting Munge and Slurm ==="
sudo service munge start
sudo /usr/sbin/slurmctld
# The Slurm daemon intentionally receives no AWS credentials. The local
# endpoint keeps all device discovery deterministic and offline.
sudo env \
  -u AWS_ACCESS_KEY_ID \
  -u AWS_SECRET_ACCESS_KEY \
  -u AWS_SESSION_TOKEN \
  -u AWS_SHARED_CREDENTIALS_FILE \
  -u AWS_REGION \
  AWS_EC2_METADATA_DISABLED=true \
  AWS_ENDPOINT_URL_BRAKET=http://127.0.0.1:18080 \
  /usr/sbin/slurmd -N localhost

echo "=== Waiting for the local Slurm node ==="
for _ in {1..30}; do
  if sinfo -h -n localhost -o "%t" | grep -qE "idle|alloc"; then
    break
  fi
  sudo scontrol update nodename=localhost state=resume 2>/dev/null || true
  sleep 1
done
if ! sinfo -h -n localhost -o "%t" | grep -qE "idle|alloc"; then
  echo "ERROR: Slurm node did not become ready" >&2
  exit 1
fi

"$@"
