# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Tests for the Python distribution of the Amazon Braket QDMI device library."""

from __future__ import annotations

from pathlib import Path

from amazon.braket.qdmi import AMAZON_BRAKET_QDMI_CMAKE_DIR, AMAZON_BRAKET_QDMI_INCLUDE_DIR, AMAZON_BRAKET_QDMI_LIBRARY_PATH, __version__


def test_version_exists() -> None:
    """Test that __version__ is defined and non-empty."""
    assert __version__
    assert isinstance(__version__, str)
    assert len(__version__) > 0


def test_include_dir_exists() -> None:
    """Test that AMAZON_BRAKET_QDMI_INCLUDE_DIR exists and is a directory."""
    assert AMAZON_BRAKET_QDMI_INCLUDE_DIR.exists()
    assert AMAZON_BRAKET_QDMI_INCLUDE_DIR.is_dir()
    assert "include" in str(AMAZON_BRAKET_QDMI_INCLUDE_DIR)


def test_include_dir_has_amazon_braket_qdmi_headers() -> None:
    """Test that the include directory contains MY QDMI headers."""
    AMAZON_BRAKET_QDMI_include = AMAZON_BRAKET_QDMI_INCLUDE_DIR / "amazon_braket_qdmi"
    assert AMAZON_BRAKET_QDMI_include.exists()
    assert AMAZON_BRAKET_QDMI_include.is_dir()


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


def test_paths_are_absolute() -> None:
    """Test that all paths are absolute."""
    assert AMAZON_BRAKET_QDMI_INCLUDE_DIR.is_absolute()
    assert AMAZON_BRAKET_QDMI_CMAKE_DIR.is_absolute()
    assert AMAZON_BRAKET_QDMI_LIBRARY_PATH.is_absolute()
