#!/bin/sh
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

set -eu

uv run --no-project python - <<'CATALOGUE'
import json
import os
from pathlib import Path
from amazon.braket.qdmi import AMAZON_BRAKET_QDMI_CATALOG_PATH

catalogue = AMAZON_BRAKET_QDMI_CATALOG_PATH
if os.environ["PROVIDER_INSTALL_MODE"] == "native":
    catalogue = Path("/opt/provider-native/lib/amazon-braket-qdmi-device.qdmi.json")
configuration = json.loads(catalogue.read_text())
for definition in configuration["qdmi"]["devices"]:
    definition["library"] = str((catalogue.parent / definition["library"]).resolve())
Path("/opt/provider-catalogue.json").write_text(json.dumps(configuration))
CATALOGUE
