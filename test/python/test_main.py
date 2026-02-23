# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH
# All rights reserved.
#
# Licensed under the Apache License v2.0 with LLVM Exceptions (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# https://github.com/munich-quantum-software/amazon-braket-qdmi-
# device/blob/main/LICENSE.md
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

from typing import TYPE_CHECKING

from amazon.braket.qdmi import (
    AMAZON_BRAKET_QDMI_CMAKE_DIR,
    AMAZON_BRAKET_QDMI_INCLUDE_DIR,
    AMAZON_BRAKET_QDMI_LIBRARY_PATH,
    __version__,
)

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
