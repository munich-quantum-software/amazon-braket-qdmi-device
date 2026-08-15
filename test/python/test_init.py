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

"""Tests for the Python distribution of the Amazon Braket QDMI device library."""

from __future__ import annotations

import json
from pathlib import Path

from amazon.braket.qdmi import (
    AMAZON_BRAKET_QDMI_CATALOG_PATH,
    AMAZON_BRAKET_QDMI_CMAKE_DIR,
    AMAZON_BRAKET_QDMI_DEVICE_IDS,
    AMAZON_BRAKET_QDMI_INCLUDE_DIR,
    AMAZON_BRAKET_QDMI_LIBRARY_PATH,
    AMAZON_BRAKET_QDMI_PREFIX,
    __version__,
)


def test_version_exists() -> None:
    """Test that __version__ is defined and non-empty."""
    assert __version__
    assert isinstance(__version__, str)
    assert len(__version__) > 0


def test_qdmi_device_metadata() -> None:
    """Test that the stable device metadata matches the native library."""
    assert AMAZON_BRAKET_QDMI_CATALOG_PATH.exists()
    assert AMAZON_BRAKET_QDMI_PREFIX == "AMAZON_BRAKET"


def test_installed_catalog() -> None:
    """Test the exact persistent device IDs, ARNs, and regions."""
    catalog = json.loads(AMAZON_BRAKET_QDMI_CATALOG_PATH.read_text(encoding="utf-8"))
    devices = catalog["qdmi"]["devices"]
    expected = {
        "amazon.braket.aqt.ibex-q1": ("arn:aws:braket:eu-north-1::device/qpu/aqt/Ibex-Q1", "eu-north-1"),
        "amazon.braket.ionq.forte-1": ("arn:aws:braket:us-east-1::device/qpu/ionq/Forte-1", "us-east-1"),
        "amazon.braket.ionq.forte-enterprise-1": (
            "arn:aws:braket:us-east-1::device/qpu/ionq/Forte-Enterprise-1",
            "us-east-1",
        ),
        "amazon.braket.iqm.garnet": ("arn:aws:braket:eu-north-1::device/qpu/iqm/Garnet", "eu-north-1"),
        "amazon.braket.iqm.emerald": ("arn:aws:braket:eu-north-1::device/qpu/iqm/Emerald", "eu-north-1"),
        "amazon.braket.rigetti.ankaa-3": (
            "arn:aws:braket:us-west-1::device/qpu/rigetti/Ankaa-3",
            "us-west-1",
        ),
        "amazon.braket.rigetti.cepheus-1-108q": (
            "arn:aws:braket:us-west-1::device/qpu/rigetti/Cepheus-1-108Q",
            "us-west-1",
        ),
        "amazon.braket.sv1": ("arn:aws:braket:::device/quantum-simulator/amazon/sv1", "us-east-1"),
        "amazon.braket.dm1": ("arn:aws:braket:::device/quantum-simulator/amazon/dm1", "us-east-1"),
    }
    assert {device["id"] for device in devices} == set(expected)
    assert tuple(device["id"] for device in devices) == AMAZON_BRAKET_QDMI_DEVICE_IDS
    for device in devices:
        session = device["session"]
        assert (session["base-url"], session["custom2"]) == expected[device["id"]]


def test_include_dir_exists() -> None:
    """Test that AMAZON_BRAKET_QDMI_INCLUDE_DIR exists and is a directory."""
    assert AMAZON_BRAKET_QDMI_INCLUDE_DIR.exists()
    assert AMAZON_BRAKET_QDMI_INCLUDE_DIR.is_dir()
    assert "include" in str(AMAZON_BRAKET_QDMI_INCLUDE_DIR)


def test_include_dir_has_amazon_braket_qdmi_headers() -> None:
    """Test that the include directory contains Amazon Braket QDMI headers."""
    amazon_braket_qdmi_include = AMAZON_BRAKET_QDMI_INCLUDE_DIR / "amazon_braket_qdmi"
    assert amazon_braket_qdmi_include.exists()
    assert amazon_braket_qdmi_include.is_dir()


def test_cmake_dir_exists() -> None:
    """Test that AMAZON_BRAKET_QDMI_CMAKE_DIR exists and is a directory."""
    assert AMAZON_BRAKET_QDMI_CMAKE_DIR.exists()
    assert AMAZON_BRAKET_QDMI_CMAKE_DIR.is_dir()
    assert "cmake" in str(AMAZON_BRAKET_QDMI_CMAKE_DIR)


def test_library_path_exists() -> None:
    """Test that AMAZON_BRAKET_QDMI_LIBRARY_PATH exists and is a file."""
    assert AMAZON_BRAKET_QDMI_LIBRARY_PATH.exists()
    assert AMAZON_BRAKET_QDMI_LIBRARY_PATH.is_file()
    assert "amazon-braket-qdmi-device" in str(AMAZON_BRAKET_QDMI_LIBRARY_PATH)


def test_paths_are_pathlib_objects() -> None:
    """Test that all path variables are pathlib.Path objects."""
    assert isinstance(AMAZON_BRAKET_QDMI_INCLUDE_DIR, Path)
    assert isinstance(AMAZON_BRAKET_QDMI_CMAKE_DIR, Path)
    assert isinstance(AMAZON_BRAKET_QDMI_LIBRARY_PATH, Path)
    assert isinstance(AMAZON_BRAKET_QDMI_CATALOG_PATH, Path)


def test_paths_are_absolute() -> None:
    """Test that all paths are absolute."""
    assert AMAZON_BRAKET_QDMI_INCLUDE_DIR.is_absolute()
    assert AMAZON_BRAKET_QDMI_CMAKE_DIR.is_absolute()
    assert AMAZON_BRAKET_QDMI_LIBRARY_PATH.is_absolute()
    assert AMAZON_BRAKET_QDMI_CATALOG_PATH.is_absolute()
