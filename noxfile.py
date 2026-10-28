#!/usr/bin/env -S uv run --script --quiet
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

# /// script
# dependencies = ["nox"]
# ///

"""Nox sessions."""

from __future__ import annotations

import argparse
import contextlib
import os
import shutil
import tempfile
from pathlib import Path
from typing import TYPE_CHECKING

import nox

if TYPE_CHECKING:
    from collections.abc import Generator, Sequence

nox.needs_version = ">=2026.8.10"
nox.options.default_venv_backend = "uv"


PYTHON_ALL_VERSIONS = ["3.11", "3.12", "3.13", "3.14", "3.15"]

if os.environ.get("CI", None):
    nox.options.error_on_missing_interpreters = True


@contextlib.contextmanager
def preserve_lockfile() -> Generator[None, None, None]:
    """Preserve the lockfile by moving it to a temporary directory."""
    with tempfile.TemporaryDirectory() as temp_dir_name:
        shutil.move("uv.lock", f"{temp_dir_name}/uv.lock")
        try:
            yield
        finally:
            shutil.move(f"{temp_dir_name}/uv.lock", "uv.lock")


@nox.session(reuse_venv=True, default=True)
def lint(session: nox.Session) -> None:
    """Run the linter."""
    if shutil.which("prek") is None:
        session.install("prek")

    session.run("prek", "run", "--all-files", *session.posargs, external=True)


def _run_tests(
    session: nox.Session,
    *,
    install_args: Sequence[str] = (),
    pytest_run_args: Sequence[str] = (),
) -> None:
    env = {"UV_PROJECT_ENVIRONMENT": session.virtualenv.location}
    session.run(
        "uv",
        "run",
        "--no-editable",
        "--no-dev",
        "--group",
        "test",
        *install_args,
        "pytest",
        *pytest_run_args,
        *session.posargs,
        "--cov-config=pyproject.toml",
        env=env,
    )


@nox.session(python=PYTHON_ALL_VERSIONS, reuse_venv=True, default=True)
def tests(session: nox.Session) -> None:
    """Run the test suite."""
    _run_tests(session)


@nox.session(python=PYTHON_ALL_VERSIONS, reuse_venv=True, venv_backend="uv", default=True)
def minimums(session: nox.Session) -> None:
    """Test the minimum versions of dependencies."""
    with preserve_lockfile():
        _run_tests(
            session,
            install_args=["--resolution=lowest-direct"],
            pytest_run_args=["-Wdefault"],
        )
        env = {"UV_PROJECT_ENVIRONMENT": session.virtualenv.location}
        session.run("uv", "tree", "--frozen", env=env)


@nox.session(python="3.14", reuse_venv=True)
def docs(session: nox.Session) -> None:
    """Build the documentation. Pass ``-b linkcheck`` to check links."""
    parser = argparse.ArgumentParser()
    parser.add_argument("-b", dest="builder", default="html", help="Build target (default: html)")
    args, posargs = parser.parse_known_args(session.posargs)

    env = {"UV_PROJECT_ENVIRONMENT": session.virtualenv.location}
    session.run(
        "uv",
        "sync",
        "--no-editable",
        "--no-dev",
        "--group",
        "docs",
        env=env,
    )
    output = Path(f"docs/_build/{args.builder}").resolve()
    output.mkdir(parents=True, exist_ok=True)
    include = session.run("uv", "run", "--no-sync", "amazon-braket-qdmi", "--include_dir", env=env, silent=True)
    with session.chdir("docs"):
        session.run(
            "doxygen",
            "Doxyfile",
            env={
                "AMAZON_BRAKET_QDMI_DOCS_OUTPUT": str(output),
                "AMAZON_BRAKET_QDMI_DOCS_INCLUDE": include.strip(),
            },
            external=True,
        )
    session.run(
        "uv",
        "run",
        "--no-sync",
        "sphinx-build",
        "-n",
        "-T",
        "-W",
        f"-b={args.builder}",
        "docs",
        f"docs/_build/{args.builder}",
        *posargs,
        env=env,
    )


if __name__ == "__main__":
    nox.main()
