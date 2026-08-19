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

"""Exercise MQT Core's Slurm connector from inside an allocated job."""

from mqt.core.qdmi import slurm


def main() -> None:
    """Open the licensed device and verify its representative metadata."""
    device = slurm.open_device_from_license()
    assert device.name() == "Local SV1"
    assert device.qubits_num() == 2


if __name__ == "__main__":
    main()
