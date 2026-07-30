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

#SBATCH --partition=debug
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --amazon-braket-device-arn=arn:aws:braket:us-east-1::device/quantum-simulator/amazon/sv1
#SBATCH --amazon-braket-region=us-east-1

set -euo pipefail

bell_program='OPENQASM 3.0;
include "stdgates.inc";
bit[2] c;
qubit[2] q;
h q[0];
cnot q[0], q[1];
c = measure q;'

[[ "${AMAZON_BRAKET_DEVICE_ARN:-}" == "${AMAZON_BRAKET_TEST_DEVICE_ARN}" ]]
[[ "${AWS_REGION:-}" == "us-east-1" ]]
grep -Fq "h q[0];" <<<"${bell_program}"
grep -Fq "cnot q[0], q[1];" <<<"${bell_program}"
grep -Fq "c = measure q;" <<<"${bell_program}"
echo "Bell circuit prepared in Slurm batch job"
