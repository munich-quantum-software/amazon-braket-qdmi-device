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

"""Select the OpenQASM receiver advertised by the native Braket device."""

from __future__ import annotations

from typing import TYPE_CHECKING

from mqt.core.mlir import TerminalReadout
from mqt.core.qdmi import CustomProperty

if TYPE_CHECKING:
    from mqt.core.qdmi import Device


def serialization_options(device: Device) -> tuple[TerminalReadout, bool]:
    """Return terminal readout and physical addressing for the device.

    Raises:
        ValueError: If the device does not advertise a supported receiver.
    """
    name = device.query_custom_property(CustomProperty.CUSTOM2, str)
    if name == "simulator":
        return TerminalReadout.ALL, False
    if name == "native":
        return TerminalReadout.MEASURED, True
    msg = f"Unsupported Braket OpenQASM receiver: {name!r}."
    raise ValueError(msg)
