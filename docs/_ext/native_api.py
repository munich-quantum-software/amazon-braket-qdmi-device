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

"""Build and publish standalone Doxygen HTML without Breathe."""

from __future__ import annotations

import shutil
import subprocess
from pathlib import Path
from typing import TYPE_CHECKING

from sphinx.errors import ExtensionError

if TYPE_CHECKING:
    from sphinx.application import Sphinx
    from sphinx.util.typing import ExtensionMetadata


def _doxygen_output(app: Sphinx) -> Path:
    """Return the configured Doxygen output directory."""
    return Path(app.srcdir) / "_build" / "doxygen"


def build_doxygen(app: Sphinx) -> None:
    """Generate the standalone native API documentation.

    Raises:
        ExtensionError: If Doxygen is unavailable or generation fails.
    """
    doxygen = shutil.which("doxygen")
    if doxygen is None:
        msg = "Doxygen is required to build the native API documentation"
        raise ExtensionError(msg)
    try:
        subprocess.run(  # ruff: ignore[subprocess-without-shell-equals-true]
            [doxygen, "Doxyfile"],
            cwd=app.srcdir,
            check=True,
        )
    except (OSError, subprocess.CalledProcessError) as error:
        msg = "Unable to generate the native API documentation"
        raise ExtensionError(msg) from error


def publish_doxygen_html(app: Sphinx, exception: Exception | None) -> None:
    """Copy Doxygen HTML next to a successful Sphinx HTML build."""
    if exception is not None or app.builder.format != "html":
        return
    shutil.copytree(_doxygen_output(app) / "html", Path(app.outdir) / "cpp", dirs_exist_ok=True)


def setup(app: Sphinx) -> ExtensionMetadata:
    """Register the Doxygen build and publication hooks.

    Returns:
        Metadata declaring the extension safe for parallel Sphinx builds.
    """
    app.connect("builder-inited", build_doxygen)
    app.connect("build-finished", publish_doxygen_html)
    return {"parallel_read_safe": True, "parallel_write_safe": True}
