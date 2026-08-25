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

"""Register the packaged Amazon Braket device catalogue with MQT Core."""

from __future__ import annotations

import json

from mqt.core.qdmi import driver

from . import (
    AMAZON_BRAKET_QDMI_CATALOG_PATH,
    AMAZON_BRAKET_QDMI_LIBRARY_PATH,
    AMAZON_BRAKET_QDMI_PREFIX,
)


def catalogue_session(device_id: str) -> dict[str, str]:
    """Return the packaged session defaults for one stable device ID.

    Raises:
        RuntimeError: If the packaged catalogue is malformed or lacks the device.
    """
    try:
        catalogue = json.loads(AMAZON_BRAKET_QDMI_CATALOG_PATH.read_text(encoding="utf-8"))
        devices = catalogue["qdmi"]["devices"]
        definition = next(device for device in devices if device["id"] == device_id)
        session = definition.get("session", {})
    except (KeyError, OSError, StopIteration, TypeError, ValueError) as error:
        msg = f"The packaged Amazon Braket QDMI catalogue has no valid definition for {device_id!r}."
        raise RuntimeError(msg) from error

    if not isinstance(session, dict) or not all(
        isinstance(key, str) and isinstance(value, str) for key, value in session.items()
    ):
        msg = f"The packaged Amazon Braket QDMI session for {device_id!r} is invalid."
        raise RuntimeError(msg)
    return session


def register_device(device_id: str) -> None:
    """Register one packaged definition without replacing user configuration."""
    session = catalogue_session(device_id)
    driver.register_device_if_absent(
        driver.DeviceDefinition(
            device_id,
            AMAZON_BRAKET_QDMI_LIBRARY_PATH,
            AMAZON_BRAKET_QDMI_PREFIX,
            base_url=session.get("base-url"),
            token=session.get("token"),
            auth_file=session.get("auth-file"),
            auth_url=session.get("auth-url"),
            username=session.get("username"),
            password=session.get("password"),
            device_config=session.get("device-config"),
            device_config_file=session.get("device-config-file"),
            custom1=session.get("custom1"),
            custom2=session.get("custom2"),
            custom3=session.get("custom3"),
            custom4=session.get("custom4"),
            custom5=session.get("custom5"),
        )
    )
