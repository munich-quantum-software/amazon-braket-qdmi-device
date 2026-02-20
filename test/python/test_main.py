# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# SPDX-License-Identifier: MIT
#
# Licensed under the MIT License

"""Tests for the Python distribution of the Amazon Braket QDMI device library."""

from __future__ import annotations

from typing import TYPE_CHECKING

from amazon.braket.qdmi import AMAZON_BRAKET_QDMI_CMAKE_DIR, AMAZON_BRAKET_QDMI_INCLUDE_DIR, AMAZON_BRAKET_QDMI_LIBRARY_PATH, __version__

if TYPE_CHECKING:
    from pytest_console_scripts import ScriptRunner


def test_cli_help(script_runner: ScriptRunner) -> None:
    """Test CLI with --help."""
    result = script_runner.run(["amazon-braket-qdmi", "--help"])
    assert result.success
    assert "Command line interface" in result.stdout
    assert "--include_dir" in result.stdout


def test_cli_version(script_runner: ScriptRunner) -> None:
    """Test CLI with --version."""
    result = script_runner.run(["amazon-braket-qdmi", "--version"])
    assert result.success
    assert __version__ in result.stdout


def test_cli_include_dir(script_runner: ScriptRunner) -> None:
    """Test CLI with --include_dir."""
    result = script_runner.run(["amazon-braket-qdmi", "--include_dir"])
    assert result.success
    assert str(AMAZON_BRAKET_QDMI_INCLUDE_DIR) in result.stdout


def test_cli_cmake_dir(script_runner: ScriptRunner) -> None:
    """Test CLI with --cmake_dir."""
    result = script_runner.run(["amazon-braket-qdmi", "--cmake_dir"])
    assert result.success
    assert str(AMAZON_BRAKET_QDMI_CMAKE_DIR) in result.stdout


def test_cli_lib_path(script_runner: ScriptRunner) -> None:
    """Test CLI with --lib_path."""
    result = script_runner.run(["amazon-braket-qdmi", "--lib_path"])
    assert result.success
    assert str(AMAZON_BRAKET_QDMI_LIBRARY_PATH) in result.stdout
