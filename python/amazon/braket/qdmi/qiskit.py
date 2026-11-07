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

"""Qiskit integration for the Amazon Braket QDMI device."""

from __future__ import annotations

from typing import TYPE_CHECKING

try:
    from mqt.core.plugins.qiskit.backend import QDMIBackend
    from mqt.core.qdmi.builtin_driver import open_device
except ImportError as error:
    msg = "Install 'amazon-braket-qdmi[qiskit]' to use the Qiskit backend."
    raise ImportError(msg) from error

from . import AMAZON_BRAKET_QDMI_DEVICE_ID
from ._session import session_parameters

if TYPE_CHECKING:
    from mqt.core.qdmi import Device as QDMIDeviceHandle

__all__ = ["AmazonBraketBackend"]


def __dir__() -> list[str]:
    return __all__


class AmazonBraketBackend(QDMIBackend):
    """Qiskit backend for an Amazon Braket QDMI catalogue device.

    Args:
        device_id: Stable ID from the packaged Amazon Braket device catalogue.
        device: An already-open Amazon Braket QDMI device.
        device_arn: Optional ARN override. The generic device requires an ARN.
        region: Optional AWS Region override.
        reservation_arn: Optional Amazon Braket reservation ARN.
    """

    def __init__(
        self,
        device_id: str | None = None,
        *,
        device: QDMIDeviceHandle | None = None,
        device_arn: str | None = None,
        region: str | None = None,
        reservation_arn: str | None = None,
    ) -> None:
        """Open the selected Amazon Braket device and adapt it for Qiskit.

        Raises:
            ValueError: If the generic device has no ARN.
        """
        if device is not None:
            if device_id is not None or any((device_arn, region, reservation_arn)):
                msg = "An already-open device cannot be combined with a device ID or session overrides."
                raise ValueError(msg)
            super().__init__(device=device)
            return

        resolved_device_id = device_id or AMAZON_BRAKET_QDMI_DEVICE_ID
        if resolved_device_id == AMAZON_BRAKET_QDMI_DEVICE_ID and not device_arn:
            msg = "The generic Amazon Braket device requires a non-empty device_arn."
            raise ValueError(msg)

        device = open_device(
            resolved_device_id,
            **session_parameters(device_arn=device_arn, region=region, reservation_arn=reservation_arn),
        )
        super().__init__(device=device, device_id=resolved_device_id)
