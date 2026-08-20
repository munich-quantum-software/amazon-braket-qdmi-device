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
import subprocess
import sys
from importlib.metadata import entry_points
from pathlib import Path
from types import ModuleType

import pytest

from amazon.braket.qdmi import (
    AMAZON_BRAKET_QDMI_CATALOG_PATH,
    AMAZON_BRAKET_QDMI_CMAKE_DIR,
    AMAZON_BRAKET_QDMI_DEVICE_ID,
    AMAZON_BRAKET_QDMI_INCLUDE_DIR,
    AMAZON_BRAKET_QDMI_LIBRARY_PATH,
    AMAZON_BRAKET_QDMI_PREFIX,
    __version__,
)

EXPECTED_PENNYLANE_ENTRY_POINTS = {
    "amazon.braket.default": "AmazonBraketDevice",
    "amazon.braket.aqt.ibex-q1": "AmazonBraketAqtIbexQ1Device",
    "amazon.braket.ionq.forte-1": "AmazonBraketIonQForte1Device",
    "amazon.braket.ionq.forte-enterprise-1": "AmazonBraketIonQForteEnterprise1Device",
    "amazon.braket.iqm.garnet": "AmazonBraketIQMGarnetDevice",
    "amazon.braket.iqm.emerald": "AmazonBraketIQMEmeraldDevice",
    "amazon.braket.rigetti.ankaa-3": "AmazonBraketRigettiAnkaa3Device",
    "amazon.braket.rigetti.cepheus-1-108q": "AmazonBraketRigettiCepheus1108QDevice",
    "amazon.braket.sv1": "AmazonBraketSV1Device",
    "amazon.braket.dm1": "AmazonBraketDM1Device",
}


def test_version_exists() -> None:
    """Test that __version__ is defined and non-empty."""
    assert isinstance(__version__, str)
    assert len(__version__) > 0


def test_qdmi_device_metadata() -> None:
    """Test that the stable device metadata matches the native library."""
    assert AMAZON_BRAKET_QDMI_DEVICE_ID == "amazon.braket.default"
    assert AMAZON_BRAKET_QDMI_PREFIX == "AMAZON_BRAKET"


def test_installed_catalogue() -> None:
    """Test the generic entry and concrete device definitions."""
    catalogue = json.loads(AMAZON_BRAKET_QDMI_CATALOG_PATH.read_text(encoding="utf-8"))
    devices = {device["id"]: device for device in catalogue["qdmi"]["devices"]}
    assert set(devices) == {
        "amazon.braket.default",
        "amazon.braket.aqt.ibex-q1",
        "amazon.braket.ionq.forte-1",
        "amazon.braket.ionq.forte-enterprise-1",
        "amazon.braket.iqm.garnet",
        "amazon.braket.iqm.emerald",
        "amazon.braket.rigetti.ankaa-3",
        "amazon.braket.rigetti.cepheus-1-108q",
        "amazon.braket.sv1",
        "amazon.braket.dm1",
    }
    assert devices[AMAZON_BRAKET_QDMI_DEVICE_ID].get("session", {}) == {}
    for device_id, device in devices.items():
        assert device["prefix"] == AMAZON_BRAKET_QDMI_PREFIX
        if device_id != AMAZON_BRAKET_QDMI_DEVICE_ID:
            assert device["session"]["base-url"].startswith("arn:aws:braket:")

    catalogue_library = AMAZON_BRAKET_QDMI_CATALOG_PATH.parent / devices[AMAZON_BRAKET_QDMI_DEVICE_ID]["library"]
    assert catalogue_library.is_file()


def test_include_dir_exists() -> None:
    """Test that AMAZON_BRAKET_QDMI_INCLUDE_DIR exists and is a directory."""
    assert AMAZON_BRAKET_QDMI_INCLUDE_DIR.exists()
    assert AMAZON_BRAKET_QDMI_INCLUDE_DIR.is_dir()


def test_include_dir_has_amazon_braket_qdmi_headers() -> None:
    """Test that the include directory contains Amazon Braket QDMI headers."""
    amazon_braket_qdmi_include = AMAZON_BRAKET_QDMI_INCLUDE_DIR / "amazon_braket_qdmi"
    assert amazon_braket_qdmi_include.exists()
    assert amazon_braket_qdmi_include.is_dir()


@pytest.mark.parametrize(
    "header",
    [
        "amazon-braket-qdmi-device/constants.hpp",
        "amazon_braket_qdmi/device.h",
    ],
)
def test_include_dir_has_public_headers(header: str) -> None:
    """Test that the include directory contains each public header."""
    assert (AMAZON_BRAKET_QDMI_INCLUDE_DIR / header).is_file()


def test_cmake_dir_exists() -> None:
    """Test that AMAZON_BRAKET_QDMI_CMAKE_DIR exists and is a directory."""
    assert AMAZON_BRAKET_QDMI_CMAKE_DIR.exists()
    assert AMAZON_BRAKET_QDMI_CMAKE_DIR.is_dir()


@pytest.mark.parametrize(
    "cmake_file",
    [
        "amazon-braket-qdmi-device-config.cmake",
        "amazon-braket-qdmi-device-config-version.cmake",
        "amazon-braket-qdmi-device-targets.cmake",
    ],
)
def test_cmake_dir_has_package_files(cmake_file: str) -> None:
    """Test that the CMake directory contains the installed package files."""
    package_dir = AMAZON_BRAKET_QDMI_CMAKE_DIR / "amazon-braket-qdmi-device"
    assert (package_dir / cmake_file).is_file()


def test_library_path_exists() -> None:
    """Test that AMAZON_BRAKET_QDMI_LIBRARY_PATH exists and is a file."""
    assert AMAZON_BRAKET_QDMI_LIBRARY_PATH.exists()
    assert AMAZON_BRAKET_QDMI_LIBRARY_PATH.is_file()


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


def test_pennylane_entry_point_mapping() -> None:
    """Map the complete catalogue to the ten stable lazy entry points."""
    catalogue = json.loads(AMAZON_BRAKET_QDMI_CATALOG_PATH.read_text(encoding="utf-8"))
    catalogue_ids = {device["id"] for device in catalogue["qdmi"]["devices"]}
    amazon_entry_points = {
        entry_point.name: entry_point
        for entry_point in entry_points(group="pennylane.plugins")
        if entry_point.name.startswith("amazon.braket.")
    }
    expected_targets = {
        name: f"amazon.braket.qdmi._pennylane_entrypoint:{class_name}"
        for name, class_name in EXPECTED_PENNYLANE_ENTRY_POINTS.items()
    }
    assert set(amazon_entry_points) == catalogue_ids
    assert all(
        entry_point.value.startswith("amazon.braket.qdmi._pennylane_entrypoint:AmazonBraket")
        for entry_point in amazon_entry_points.values()
    )
    assert set(expected_targets) == catalogue_ids
    assert {name: entry_point.value for name, entry_point in amazon_entry_points.items()} == expected_targets

    if sys.version_info < (3, 11):
        for entry_point in amazon_entry_points.values():
            with pytest.raises(ImportError):
                entry_point.load()


def test_base_install_does_not_import_optional_pennylane(
    tmp_path: Path,
) -> None:
    """Import the base package and lazy shim while optional imports are blocked."""
    script = """
import importlib
import importlib.abc
import sys
from importlib.metadata import entry_points


class OptionalImportBlocker(importlib.abc.MetaPathFinder):
    def find_spec(self, fullname, path=None, target=None):
        blocked = ("pennylane", "mqt.core")
        if any(fullname == name or fullname.startswith(f"{name}.") for name in blocked):
            raise ImportError(f"blocked optional import: {fullname}")
        return None


sys.meta_path.insert(0, OptionalImportBlocker())
base_package = importlib.import_module("amazon.braket.qdmi")
shim = importlib.import_module("amazon.braket.qdmi._pennylane_entrypoint")
assert base_package.__name__ == "amazon.braket.qdmi"
assert shim.__name__ == "amazon.braket.qdmi._pennylane_entrypoint"

amazon_entry_points = [
    entry_point
    for entry_point in entry_points(group="pennylane.plugins")
    if entry_point.name.startswith("amazon.braket.")
]
assert len(amazon_entry_points) == 10
for entry_point in amazon_entry_points:
    try:
        entry_point.load()
    except ImportError:
        pass
    else:
        raise AssertionError(f"optional entry point unexpectedly loaded: {entry_point.name}")
"""
    result = subprocess.run(  # ruff: ignore[subprocess-without-shell-equals-true]
        [sys.executable, "-I", "-c", script],
        cwd=tmp_path,
        check=False,
        capture_output=True,
        text=True,
    )
    assert result.returncode == 0, result.stderr


@pytest.mark.skipif(sys.version_info >= (3, 11), reason="Python floor applies only to Python 3.10")
def test_python_floor_precedes_available_optional_plugin(monkeypatch: pytest.MonkeyPatch) -> None:
    """Reject every PennyLane entry point on Python 3.10 even if its target exists."""
    fake_pennylane = ModuleType("amazon.braket.qdmi.pennylane")
    sentinels = {class_name: object() for class_name in EXPECTED_PENNYLANE_ENTRY_POINTS.values()}
    for class_name, sentinel in sentinels.items():
        setattr(fake_pennylane, class_name, sentinel)
    monkeypatch.setitem(sys.modules, "amazon.braket.qdmi.pennylane", fake_pennylane)

    expected_targets = {
        name: f"amazon.braket.qdmi._pennylane_entrypoint:{class_name}"
        for name, class_name in EXPECTED_PENNYLANE_ENTRY_POINTS.items()
    }
    amazon_entry_points = {
        entry_point.name: entry_point
        for entry_point in entry_points(group="pennylane.plugins")
        if entry_point.name.startswith("amazon.braket.")
    }
    assert {name: entry_point.value for name, entry_point in amazon_entry_points.items()} == expected_targets
    for name, entry_point in amazon_entry_points.items():
        with pytest.raises(ImportError):
            entry_point.load()
        assert (
            getattr(fake_pennylane, EXPECTED_PENNYLANE_ENTRY_POINTS[name])
            is sentinels[EXPECTED_PENNYLANE_ENTRY_POINTS[name]]
        )
