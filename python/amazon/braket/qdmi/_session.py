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

"""Amazon Braket session settings shared by the Python adapters."""

from __future__ import annotations

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from mqt.core.typing import QDMISessionParameters


def session_parameters(
    *,
    device_arn: str | None = None,
    region: str | None = None,
    reservation_arn: str | None = None,
) -> QDMISessionParameters:
    """Translate explicit connection overrides to native QDMI parameters.

    Returns:
        Parameters that leave omitted settings to the native device.
    """
    parameters: QDMISessionParameters = {}
    if device_arn is not None:
        parameters["base_url"] = device_arn
    if region is not None:
        parameters["custom2"] = region
    if reservation_arn is not None:
        parameters["custom3"] = reservation_arn
    return parameters
