"""Python wrapper for exposing the Amazon Braket QDMI device library."""

from importlib.metadata import distribution
from pathlib import Path

from ._version import version as __version__

__all__ = ["AMAZON_BRAKET_QDMI_CMAKE_DIR", "AMAZON_BRAKET_QDMI_INCLUDE_DIR", "AMAZON_BRAKET_QDMI_LIBRARY_PATH", "__version__"]


def __dir__() -> list[str]:
    return __all__


dist = distribution("amazon-braket-qdmi")
located_include_dir = dist.locate_file("amazon/braket/qdmi/data/include/amazon-braket-qdmi-device")
resolved_include_dir = Path(str(located_include_dir)).resolve(strict=True)

AMAZON_BRAKET_QDMI_DATA = resolved_include_dir.parents[1]
assert AMAZON_BRAKET_QDMI_DATA.exists(), f"AMAZON_BRAKET_QDMI_DATA does not exist: {AMAZON_BRAKET_QDMI_DATA}"

AMAZON_BRAKET_QDMI_LIBRARY_DIR = AMAZON_BRAKET_QDMI_DATA / "lib"
if not AMAZON_BRAKET_QDMI_LIBRARY_DIR.exists():
    AMAZON_BRAKET_QDMI_LIBRARY_DIR = AMAZON_BRAKET_QDMI_DATA / "lib64"
assert AMAZON_BRAKET_QDMI_LIBRARY_DIR.exists(), f"AMAZON_BRAKET_QDMI_LIBRARY_DIR does not exist: {AMAZON_BRAKET_QDMI_LIBRARY_DIR}"

# the library is the sole file in the lib directory
library_files = list(AMAZON_BRAKET_QDMI_LIBRARY_DIR.glob("*amazon-braket-qdmi-device*"))
if not library_files:
    msg = f"No Amazon Braket QDMI library found in: {AMAZON_BRAKET_QDMI_LIBRARY_DIR}"
    raise FileNotFoundError(msg)
AMAZON_BRAKET_QDMI_LIBRARY_PATH = library_files[0]

AMAZON_BRAKET_QDMI_INCLUDE_DIR = AMAZON_BRAKET_QDMI_DATA / "include"
assert AMAZON_BRAKET_QDMI_INCLUDE_DIR.exists(), f"AMAZON_BRAKET_QDMI_INCLUDE_DIR does not exist: {AMAZON_BRAKET_QDMI_INCLUDE_DIR}"

AMAZON_BRAKET_QDMI_CMAKE_DIR = AMAZON_BRAKET_QDMI_DATA / "share" / "cmake"
assert AMAZON_BRAKET_QDMI_CMAKE_DIR.exists(), f"AMAZON_BRAKET_QDMI_CMAKE_DIR does not exist: {AMAZON_BRAKET_QDMI_CMAKE_DIR}"

del dist, located_include_dir, resolved_include_dir
