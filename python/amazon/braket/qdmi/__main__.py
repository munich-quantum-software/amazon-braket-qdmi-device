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

"""Command line interface for the Amazon Braket QDMI device library."""

import argparse
import sys
from functools import partial

from . import (
    AMAZON_BRAKET_QDMI_CATALOG_PATH,
    AMAZON_BRAKET_QDMI_CMAKE_DIR,
    AMAZON_BRAKET_QDMI_INCLUDE_DIR,
    AMAZON_BRAKET_QDMI_LIBRARY_PATH,
    __version__,
)


def main() -> None:
    """Entry point for the amazon-braket-qdmi command line interface.

    This function is called when running the `amazon-braket-qdmi` script.

    .. code-block:: bash

        amazon-braket-qdmi [--version] [--include_dir] [--cmake_dir] [--lib_path] [--catalog_path]

    It provides the following command line options:

    - :code:`--version`: Print the version and exit.
    - :code:`--include_dir`: Print the path to the amazon-braket-qdmi C/C++ include directory.
    - :code:`--cmake_dir`: Print the path to the amazon-braket-qdmi CMake module directory.
    - :code:`--lib_path`: Print the path to the amazon-braket-qdmi shared library.
    - :code:`--catalog_path`: Print the path to the installed QDMI device catalogue.
    """
    make_parser = partial(
        argparse.ArgumentParser,
        prog="amazon-braket-qdmi",
        description="Command line interface for the Amazon Braket QDMI device library.",
    )
    if sys.version_info >= (3, 14):
        make_parser = partial(make_parser, suggest_on_error=True)

    parser = make_parser()
    group = parser.add_mutually_exclusive_group()
    group.add_argument(
        "--version",
        action="version",
        version=f"{__version__}",
    )
    group.add_argument(
        "--include_dir",
        action="store_true",
        help="Print the path to the amazon-braket-qdmi C/C++ include directory",
    )
    group.add_argument(
        "--cmake_dir",
        action="store_true",
        help="Print the path to the amazon-braket-qdmi CMake module directory",
    )
    group.add_argument(
        "--lib_path",
        action="store_true",
        help="Print the path to the amazon-braket-qdmi shared library",
    )
    group.add_argument(
        "--catalog_path",
        action="store_true",
        help="Print the path to the installed Amazon Braket QDMI device catalogue",
    )

    args = parser.parse_args()

    if args.include_dir:
        print(AMAZON_BRAKET_QDMI_INCLUDE_DIR)
    elif args.cmake_dir:
        print(AMAZON_BRAKET_QDMI_CMAKE_DIR)
    elif args.lib_path:
        print(AMAZON_BRAKET_QDMI_LIBRARY_PATH)
    elif args.catalog_path:
        print(AMAZON_BRAKET_QDMI_CATALOG_PATH)
    else:
        parser.print_help()


if __name__ == "__main__":
    main()
