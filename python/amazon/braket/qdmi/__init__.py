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

"""Python wrapper for exposing the Amazon Braket QDMI device library."""

# ruff: file-ignore[non-empty-init-module]

import sys
from importlib.metadata import distribution
from pathlib import Path

from ._version import version as __version__

__all__ = [
    "AMAZON_BRAKET_QDMI_CATALOG_PATH",
    "AMAZON_BRAKET_QDMI_CMAKE_DIR",
    "AMAZON_BRAKET_QDMI_DEVICE_ID",
    "AMAZON_BRAKET_QDMI_INCLUDE_DIR",
    "AMAZON_BRAKET_QDMI_LIBRARY_PATH",
    "AMAZON_BRAKET_QDMI_PREFIX",
    "__version__",
]

AMAZON_BRAKET_QDMI_DEVICE_ID = "amazon.braket.default"
AMAZON_BRAKET_QDMI_PREFIX = "AMAZON_BRAKET"


def __dir__() -> list[str]:
    return __all__


dist = distribution("amazon-braket-qdmi")
located_include_dir = dist.locate_file("amazon/braket/qdmi/data/include/amazon_braket_qdmi")
resolved_include_dir = Path(str(located_include_dir)).resolve(strict=True)

_AMAZON_BRAKET_QDMI_DATA = resolved_include_dir.parents[1]
if not _AMAZON_BRAKET_QDMI_DATA.exists():
    msg = f"AMAZON_BRAKET_QDMI_DATA does not exist: {_AMAZON_BRAKET_QDMI_DATA}"
    raise FileNotFoundError(msg)


def _resolve_library_dir() -> Path:
    """Return the directory containing the packaged Amazon Braket QDMI shared library.

    Raises:
        FileNotFoundError: If the expected library directory does not exist.
    """
    if sys.platform == "win32":
        library_dir = _AMAZON_BRAKET_QDMI_DATA / "bin"
        if library_dir.exists():
            return library_dir
        msg = (
            f"Expected 'bin' directory for Amazon Braket QDMI library on Windows, but it does not exist: {library_dir}"
        )
        raise FileNotFoundError(msg)

    library_dir = _AMAZON_BRAKET_QDMI_DATA / "lib"
    if library_dir.exists():
        return library_dir

    library_dir = _AMAZON_BRAKET_QDMI_DATA / "lib64"
    if library_dir.exists():
        return library_dir

    msg = (
        "Expected 'lib' or 'lib64' directory for Amazon Braket QDMI library on Unix-like systems, "
        f"but neither exists: {_AMAZON_BRAKET_QDMI_DATA}"
    )
    raise FileNotFoundError(msg)


_AMAZON_BRAKET_QDMI_LIBRARY_DIR = _resolve_library_dir()
_AMAZON_BRAKET_QDMI_CATALOG_NAME = "amazon-braket-qdmi-device.qdmi.json"

# Ignore the adjacent QDMI catalogue when locating the native library.
library_files = [
    path
    for path in _AMAZON_BRAKET_QDMI_LIBRARY_DIR.glob("*amazon-braket-qdmi-device*")
    if path.name != _AMAZON_BRAKET_QDMI_CATALOG_NAME
]
if not library_files:
    msg = f"No Amazon Braket QDMI library found in: {_AMAZON_BRAKET_QDMI_LIBRARY_DIR}"
    raise FileNotFoundError(msg)
AMAZON_BRAKET_QDMI_LIBRARY_PATH = min(library_files, key=lambda p: len(p.name))

AMAZON_BRAKET_QDMI_CATALOG_PATH = _AMAZON_BRAKET_QDMI_LIBRARY_DIR / _AMAZON_BRAKET_QDMI_CATALOG_NAME
if not AMAZON_BRAKET_QDMI_CATALOG_PATH.exists():
    msg = f"AMAZON_BRAKET_QDMI_CATALOG_PATH does not exist: {AMAZON_BRAKET_QDMI_CATALOG_PATH}"
    raise FileNotFoundError(msg)

AMAZON_BRAKET_QDMI_INCLUDE_DIR = _AMAZON_BRAKET_QDMI_DATA / "include"
if not AMAZON_BRAKET_QDMI_INCLUDE_DIR.exists():
    msg = f"AMAZON_BRAKET_QDMI_INCLUDE_DIR does not exist: {AMAZON_BRAKET_QDMI_INCLUDE_DIR}"
    raise FileNotFoundError(msg)

AMAZON_BRAKET_QDMI_CMAKE_DIR = _AMAZON_BRAKET_QDMI_DATA / "share" / "cmake"
if not AMAZON_BRAKET_QDMI_CMAKE_DIR.exists():
    msg = f"AMAZON_BRAKET_QDMI_CMAKE_DIR does not exist: {AMAZON_BRAKET_QDMI_CMAKE_DIR}"
    raise FileNotFoundError(msg)

del dist, located_include_dir, resolved_include_dir
