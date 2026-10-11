#!/bin/sh
# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# Licensed under the Apache License v2.0 with LLVM Exceptions (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# https://llvm.org/LICENSE.txt
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
#
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

set -eu

python3 - "${1:-/opt/provider-catalogue.json}" <<'CATALOGUE'
import json
import os
import sys
from pathlib import Path
from amazon.braket.qdmi import AMAZON_BRAKET_QDMI_CATALOG_PATH

catalogue = AMAZON_BRAKET_QDMI_CATALOG_PATH
if os.environ["PROVIDER_INSTALL_MODE"] == "native":
    catalogue = Path("/opt/provider-native/lib/amazon-braket-qdmi-device.qdmi.json")
configuration = json.loads(catalogue.read_text())
for definition in configuration["qdmi"]["devices"]:
    definition["library"] = str((catalogue.parent / definition["library"]).resolve())
    definition["enabled"] = definition["id"] == "amazon.braket.sv1"
Path(sys.argv[1]).write_text(json.dumps(configuration))
CATALOGUE
