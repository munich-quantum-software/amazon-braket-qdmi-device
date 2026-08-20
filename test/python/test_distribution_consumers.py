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

"""Consumer tests for the installed native distribution."""

from __future__ import annotations

import ctypes
import json
import os
import shutil
import subprocess
import sys
from importlib.metadata import distribution
from pathlib import Path
from typing import TYPE_CHECKING

from amazon.braket.qdmi import (
    AMAZON_BRAKET_QDMI_CMAKE_DIR,
    AMAZON_BRAKET_QDMI_LIBRARY_PATH,
)

if TYPE_CHECKING:
    from collections.abc import Mapping

INITIALIZE_SYMBOL = "AMAZON_BRAKET_QDMI_device_initialize"


def _load_native_library(path: Path) -> ctypes.CDLL:
    """Load a packaged library without invoking its QDMI interface.

    Returns:
        The loaded native library.
    """
    if sys.platform == "win32":
        with os.add_dll_directory(path.parent):
            return ctypes.CDLL(str(path))
    return ctypes.CDLL(str(path))


def _copy_repaired_wheel_sidecars(relocated_runtime: Path) -> list[Path]:
    """Copy this wheel's repaired sidecar libraries into the relocated runtime.

    Returns:
        Relocated repair-sidecar directories that were copied.
    """
    if sys.platform != "linux":
        return []

    dist = distribution("amazon-braket-qdmi")
    sidecar_names = sorted({
        path.parts[0] for path in (dist.files or ()) if path.parts and path.parts[0].endswith(".libs")
    })
    relocated_sidecars: list[Path] = []
    for sidecar_name in sidecar_names:
        sidecar_dir = Path(str(dist.locate_file(sidecar_name)))
        if not sidecar_dir.is_dir():
            continue
        relocated_sidecar = relocated_runtime / sidecar_dir.name
        shutil.copytree(sidecar_dir, relocated_sidecar)
        assert any(path.is_file() for path in relocated_sidecar.iterdir())
        relocated_sidecars.append(relocated_sidecar)
    return relocated_sidecars


def _relocated_native_loader_environment(relocated_sidecars: list[Path]) -> dict[str, str]:
    """Return an isolated environment for the relocated native loader.

    Returns:
        The child process environment.
    """
    environment = os.environ.copy()
    environment.pop("LD_LIBRARY_PATH", None)
    if relocated_sidecars:
        environment["LD_LIBRARY_PATH"] = os.pathsep.join(str(path) for path in relocated_sidecars)
    return environment


def _run(command: list[str], *, env: Mapping[str, str] | None = None) -> None:
    """Run a trusted local consumer command with useful failure output."""
    result = subprocess.run(  # ruff: ignore[subprocess-without-shell-equals-true]
        command,
        check=False,
        capture_output=True,
        env=env,
        text=True,
    )
    assert result.returncode == 0, result.stdout + result.stderr


def test_installed_library_exports_qdmi_initialize() -> None:
    """Load the installed library and resolve a public QDMI symbol."""
    library = _load_native_library(AMAZON_BRAKET_QDMI_LIBRARY_PATH)
    assert getattr(library, INITIALIZE_SYMBOL) is not None


def test_cmake_consumer_copies_relocatable_qdmi_runtime(tmp_path: Path) -> None:
    """Build an installed MQT consumer and relocate its copied QDMI runtime."""
    cmake = shutil.which("cmake")
    ninja = shutil.which("ninja")
    assert cmake is not None

    source_dir = tmp_path / "source"
    build_dir = tmp_path / "build"
    source_dir.mkdir()
    (source_dir / "CMakeLists.txt").write_text(
        """\
cmake_minimum_required(VERSION 3.24)
project(installed-distribution-consumer LANGUAGES C)

find_package(mqt-core CONFIG REQUIRED)
find_package(amazon-braket-qdmi-device CONFIG REQUIRED)

add_executable(installed-distribution-consumer main.c)
set_target_properties(
  installed-distribution-consumer
  PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/runtime")
target_link_libraries(
  installed-distribution-consumer PRIVATE amazon-braket-qdmi-device)
mqt_copy_qdmi_runtime(
  installed-distribution-consumer amazon-braket-qdmi-device)
""",
        encoding="utf-8",
    )
    (source_dir / "main.c").write_text(
        """\
#include <amazon_braket_qdmi/device.h>

static int (*volatile initialize_device)(void) =
    &AMAZON_BRAKET_QDMI_device_initialize;

int main(void) {
  return initialize_device == 0;
}
""",
        encoding="utf-8",
    )

    mqt_core_cmake_dir = distribution("mqt-core").locate_file("mqt/core/share/cmake")
    cmake_prefix_path = f"{AMAZON_BRAKET_QDMI_CMAKE_DIR};{mqt_core_cmake_dir}"
    configure_command = [
        cmake,
        "-S",
        str(source_dir),
        "-B",
        str(build_dir),
        "-DCMAKE_BUILD_TYPE=Release",
        f"-DCMAKE_PREFIX_PATH={cmake_prefix_path}",
    ]
    if ninja is not None:
        configure_command.extend(["-G", "Ninja", f"-DCMAKE_MAKE_PROGRAM={ninja}"])
    _run(configure_command)
    _run([cmake, "--build", str(build_dir), "--config", "Release"])

    executable_name = (
        "installed-distribution-consumer.exe" if sys.platform == "win32" else "installed-distribution-consumer"
    )
    executables = [path for path in (build_dir / "runtime").rglob(executable_name) if path.is_file()]
    assert len(executables) == 1

    relocated_runtime = tmp_path / "relocated-runtime"
    shutil.copytree(executables[0].parent, relocated_runtime)
    relocated_sidecars = _copy_repaired_wheel_sidecars(relocated_runtime)
    catalogues = list(relocated_runtime.glob("*.qdmi.json"))
    assert len(catalogues) == 1
    catalogue = json.loads(catalogues[0].read_text(encoding="utf-8"))
    devices = catalogue["qdmi"]["devices"]
    assert devices
    assert all(device["prefix"] == "AMAZON_BRAKET" for device in devices)
    library_names = {device["library"] for device in devices}
    assert len(library_names) == 1

    library_name = Path(library_names.pop())
    assert not library_name.is_absolute()
    relocated_library = relocated_runtime / library_name
    assert relocated_library.is_file()

    mqt_environment = os.environ.copy()
    mqt_environment["MQT_CORE_QDMI_CONFIG_FILE"] = str(catalogues[0])
    mqt_environment.pop("MQT_CORE_QDMI_CONFIG_JSON", None)
    _run(
        [
            sys.executable,
            "-c",
            ("from mqt.core.qdmi import driver; assert 'amazon.braket.default' in driver.registered_device_ids()"),
        ],
        env=mqt_environment,
    )

    _run(
        [
            sys.executable,
            "-I",
            "-c",
            """\
import ctypes
import os
from pathlib import Path
import sys

library_path = os.fsdecode(sys.argv[1])
symbol = sys.argv[2]
relocated_runtime = Path(os.fsdecode(sys.argv[3])).resolve()
for entry in os.environ.get("LD_LIBRARY_PATH", "").split(os.pathsep):
    if entry:
        assert Path(entry).resolve().is_relative_to(relocated_runtime)
if sys.platform == "win32":
    with os.add_dll_directory(os.path.dirname(library_path)):
        library = ctypes.CDLL(library_path)
        assert getattr(library, symbol) is not None
else:
    library = ctypes.CDLL(library_path)
    assert getattr(library, symbol) is not None
assert not any(name == "amazon" or name.startswith("amazon.") for name in sys.modules)
""",
            str(relocated_library),
            INITIALIZE_SYMBOL,
            str(relocated_runtime),
        ],
        env=_relocated_native_loader_environment(relocated_sidecars),
    )
