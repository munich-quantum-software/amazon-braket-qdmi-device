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

"""Sphinx configuration file."""

from __future__ import annotations

import sys
from importlib import metadata
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent / "_ext"))

try:
    version = metadata.version("amazon-braket-qdmi")
except metadata.PackageNotFoundError:
    msg = "amazon-braket-qdmi must be installed to build the documentation"
    raise ModuleNotFoundError(msg) from None

release = version.split("+")[0]

project = "Amazon Braket QDMI Device"
author = "Munich Quantum Software Company GmbH"
copyright = "2025 - 2026 Munich Quantum Software Company GmbH"  # ruff: ignore[builtin-variable-shadowing]
language = "en"
master_doc = "index"

extensions = [
    "native_api",
    "myst_nb",
    "sphinx_copybutton",
    "sphinx_design",
    "sphinx.ext.intersphinx",
]

source_suffix = [".rst", ".md"]
exclude_patterns = [
    "_build",
    "**.ipynb_checkpoints",
    "**.jupyter_cache",
    "**.jupyter_execute",
    "Thumbs.db",
    ".DS_Store",
]

html_theme = "furo"
html_theme_options = {
    "source_repository": "https://github.com/munich-quantum-software/amazon-braket-qdmi-device/",
    "source_branch": "main",
    "source_directory": "docs/",
    "navigation_with_keys": True,
}

intersphinx_mapping = {
    "python": ("https://docs.python.org/3", None),
    "mqt-core": ("https://mqt.readthedocs.io/projects/core/en/stable", None),
}

myst_enable_extensions = [
    "amsmath",
    "colon_fence",
    "deflist",
    "dollarmath",
    "substitution",
]
myst_heading_anchors = 3
myst_substitutions = {"version": version}

nb_execution_mode = "cache"
nb_execution_raise_on_error = True

copybutton_prompt_text = r"(?:\(\.?venv\) )?(?:\[.*\] )?\$ "
copybutton_prompt_is_regexp = True
copybutton_line_continuation_character = "\\"
