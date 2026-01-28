"""Command line interface for the Amazon Braket QDMI device library."""

import argparse
import sys
from functools import partial

from . import AMAZON_BRAKET_QDMI_CMAKE_DIR, AMAZON_BRAKET_QDMI_INCLUDE_DIR, AMAZON_BRAKET_QDMI_LIBRARY_PATH, __version__


def main() -> None:
    """Entry point for the amazon-braket-qdmi command line interface.

    This function is called when running the `amazon-braket-qdmi` script.

    .. code-block:: bash

        amazon-braket-qdmi [--version] [--include_dir] [--cmake_dir] [--lib_path]

    It provides the following command line options:

    - :code:`--version`: Print the version and exit.
    - :code:`--include_dir`: Print the path to the amazon-braket-qdmi C/C++ include directory.
    - :code:`--cmake_dir`: Print the path to the amazon-braket-qdmi CMake module directory.
    - :code:`--lib_path`: Print the path to the amazon-braket-qdmi shared library.
    """
    make_parser = partial(
        argparse.ArgumentParser, prog="amazon-braket-qdmi", description="Command line interface for the Amazon Braket QDMI device library."
    )
    if sys.version_info >= (3, 14):
        make_parser = partial(make_parser, suggest_on_error=True)

    parser = make_parser()
    parser.add_argument(
        "--version",
        action="version",
        version=f"{__version__}",
    )
    parser.add_argument(
        "--include_dir",
        action="store_true",
        help="Print the path to the amazon-braket-qdmi C/C++ include directory",
    )
    parser.add_argument(
        "--cmake_dir",
        action="store_true",
        help="Print the path to the amazon-braket-qdmi CMake module directory",
    )
    parser.add_argument(
        "--lib_path",
        action="store_true",
        help="Print the path to the amazon-braket-qdmi shared library",
    )

    args = parser.parse_args()

    if args.include_dir:
        print(AMAZON_BRAKET_QDMI_INCLUDE_DIR)
    elif args.cmake_dir:
        print(AMAZON_BRAKET_QDMI_CMAKE_DIR)
    elif args.lib_path:
        print(AMAZON_BRAKET_QDMI_LIBRARY_PATH)
    else:
        parser.print_help()


if __name__ == "__main__":
    main()
