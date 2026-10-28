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

from mqt.core.mlir import SerializationProfile
from mqt.core.qdmi import CustomProperty

if TYPE_CHECKING:
    from mqt.core.qdmi import Device


def serialization_profile(device: Device) -> SerializationProfile:
    """Return the native receiver profile.

    Raises:
        ValueError: If the device does not advertise a supported receiver.
    """
    name = device.query_custom_property(CustomProperty.CUSTOM2, str)
    profiles = {
        "simulator": SerializationProfile.BRAKET_SIMULATOR,
        "native": SerializationProfile.BRAKET_NATIVE,
    }
    if name not in profiles:
        msg = f"Unsupported Braket OpenQASM receiver profile: {name!r}."
        raise ValueError(msg)
    return profiles[name]
