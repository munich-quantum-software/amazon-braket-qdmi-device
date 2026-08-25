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

"""PennyLane specialization for the Amazon Braket QDMI device."""

from __future__ import annotations

import re
import unicodedata
from typing import TYPE_CHECKING, ClassVar

from mqt.core.plugins.pennylane import PennyLaneConfigurationError, QDMIDevice

from . import (
    AMAZON_BRAKET_QDMI_DEVICE_ID,
)
from ._catalogue import register_device

if TYPE_CHECKING:
    from collections.abc import Hashable, Sequence

    import pennylane as qp
    from mqt.core.typing import QDMIJobParameters, QDMISessionParameters

__all__ = [
    "AmazonBraketAqtIbexQ1Device",
    "AmazonBraketDM1Device",
    "AmazonBraketDevice",
    "AmazonBraketIQMEmeraldDevice",
    "AmazonBraketIQMGarnetDevice",
    "AmazonBraketIonQForte1Device",
    "AmazonBraketIonQForteEnterprise1Device",
    "AmazonBraketRigettiAnkaa3Device",
    "AmazonBraketRigettiCepheus1108QDevice",
    "AmazonBraketSV1Device",
]

_S3_BUCKET_NAME = re.compile(r"[a-z0-9][a-z0-9.-]{1,61}[a-z0-9]")


def _s3_destination_uri(value: tuple[str, str] | None) -> str | None:
    """Convert PennyLane's bucket-prefix pair to the native QDMI S3 URI.

    Returns:
        The complete S3 URI, or ``None`` to use the native fallback.

    Raises:
        PennyLaneConfigurationError: If an explicit bucket or prefix is invalid.
    """
    if value is None:
        return None

    bucket, prefix = value
    prefix = prefix.strip("/")
    if _S3_BUCKET_NAME.fullmatch(bucket) is None or ".." in bucket:
        msg = "The Amazon Braket S3 bucket must be a valid 3-63 character bucket name."
        raise PennyLaneConfigurationError(msg)
    if not prefix or any(unicodedata.category(character) == "Cc" for character in prefix):
        msg = "The Amazon Braket S3 result prefix must be non-empty and contain no control characters."
        raise PennyLaneConfigurationError(msg)
    return f"s3://{bucket}/{prefix}"


class AmazonBraketDevice(QDMIDevice):
    """Execute PennyLane circuits through the Amazon Braket QDMI device.

    Args:
        device_arn: ARN of a gate-based Amazon Braket device or simulator. The
            generic device requires an ARN. Catalogue-specific subclasses use
            their packaged ARN when this argument is omitted.
        wires: PennyLane wire labels or number of wires.
        shots: Default shot configuration.
        s3_destination_folder: Optional S3 ``(bucket, prefix)``. If omitted,
            the native device uses the automatic regional result bucket.
        region: Optional AWS region override.
        reservation_arn: Optional Braket reservation ARN.
    """

    qdmi_device_id: ClassVar[str] = AMAZON_BRAKET_QDMI_DEVICE_ID

    def __init__(
        self,
        device_arn: str | None = None,
        wires: int | Sequence[Hashable] | None = None,
        shots: int | Sequence[int | tuple[int, int]] | qp.measurements.Shots | None = 1024,
        *,
        s3_destination_folder: tuple[str, str] | None = None,
        region: str | None = None,
        reservation_arn: str | None = None,
    ) -> None:
        """Configure a fresh Amazon Braket QDMI session.

        Raises:
            PennyLaneConfigurationError: If the device ARN or S3 destination is invalid.
        """
        requires_device_arn = self.qdmi_device_id == AMAZON_BRAKET_QDMI_DEVICE_ID
        if not device_arn and (requires_device_arn or device_arn is not None):
            msg = "Amazon Braket execution requires a non-empty device_arn."
            raise PennyLaneConfigurationError(msg)

        session_parameters: QDMISessionParameters = {}
        if device_arn is not None:
            session_parameters["base_url"] = device_arn
        if region:
            session_parameters["custom2"] = region
        if reservation_arn:
            session_parameters["custom3"] = reservation_arn

        s3_uri = _s3_destination_uri(s3_destination_folder)
        job_parameters: QDMIJobParameters = {} if s3_uri is None else {"custom1": s3_uri}

        register_device(self.qdmi_device_id)
        self._device_arn = device_arn
        self._s3_destination_folder = s3_destination_folder
        super().__init__(
            self.qdmi_device_id,
            wires=wires,
            shots=shots,
            session_parameters=session_parameters,
            job_parameters=job_parameters,
        )

    @property
    def device_arn(self) -> str | None:
        """Explicit ARN override, or ``None`` when using a catalogue default."""
        return self._device_arn

    @property
    def s3_destination_folder(self) -> tuple[str, str] | None:
        """Explicit S3 override, or ``None`` for automatic result-bucket resolution."""
        return self._s3_destination_folder


class AmazonBraketAqtIbexQ1Device(AmazonBraketDevice):
    """PennyLane entry point for the catalogue's AQT Ibex Q1 definition."""

    qdmi_device_id = "amazon.braket.aqt.ibex-q1"


class AmazonBraketIonQForte1Device(AmazonBraketDevice):
    """PennyLane entry point for the catalogue's IonQ Forte 1 definition."""

    qdmi_device_id = "amazon.braket.ionq.forte-1"


class AmazonBraketIonQForteEnterprise1Device(AmazonBraketDevice):
    """PennyLane entry point for the catalogue's IonQ Forte Enterprise 1 definition."""

    qdmi_device_id = "amazon.braket.ionq.forte-enterprise-1"


class AmazonBraketIQMGarnetDevice(AmazonBraketDevice):
    """PennyLane entry point for the catalogue's IQM Garnet definition."""

    qdmi_device_id = "amazon.braket.iqm.garnet"


class AmazonBraketIQMEmeraldDevice(AmazonBraketDevice):
    """PennyLane entry point for the catalogue's IQM Emerald definition."""

    qdmi_device_id = "amazon.braket.iqm.emerald"


class AmazonBraketRigettiAnkaa3Device(AmazonBraketDevice):
    """PennyLane entry point for the catalogue's Rigetti Ankaa 3 definition."""

    qdmi_device_id = "amazon.braket.rigetti.ankaa-3"


class AmazonBraketRigettiCepheus1108QDevice(AmazonBraketDevice):
    """PennyLane entry point for the catalogue's Rigetti Cepheus 1 108Q definition."""

    qdmi_device_id = "amazon.braket.rigetti.cepheus-1-108q"


class AmazonBraketSV1Device(AmazonBraketDevice):
    """PennyLane entry point for the catalogue's SV1 definition."""

    qdmi_device_id = "amazon.braket.sv1"


class AmazonBraketDM1Device(AmazonBraketDevice):
    """PennyLane entry point for the catalogue's DM1 definition."""

    qdmi_device_id = "amazon.braket.dm1"
