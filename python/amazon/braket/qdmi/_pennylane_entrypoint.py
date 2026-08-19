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

"""Lazy PennyLane entry point for the Python 3.10-compatible base wheel."""

from __future__ import annotations

import sys
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from .pennylane import AmazonBraketDevice

_DEVICE_CLASSES = {
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
}


def __getattr__(name: str) -> type[AmazonBraketDevice]:
    """Load the optional device only when PennyLane resolves its entry point.

    Returns:
        The Amazon Braket PennyLane device class.

    Raises:
        AttributeError: If an unknown module attribute is requested.
        ImportError: If the interpreter or optional dependencies are unsupported.
    """
    if name not in _DEVICE_CLASSES:
        raise AttributeError(name)
    if sys.version_info < (3, 11):
        msg = "The Amazon Braket PennyLane integration requires Python 3.11 or newer."
        raise ImportError(msg)
    try:
        from . import pennylane  # ruff: ignore[import-outside-top-level]
    except ImportError as error:
        msg = "Install 'amazon-braket-qdmi[pennylane]' to use the PennyLane device."
        raise ImportError(msg) from error
    return getattr(pennylane, name)
