# SpecAudit: `distribution-catalog-and-python-shell`

## Audit identity

- **Scope:** `distribution-catalog-and-python-shell`
- **Scope code:** `DIST`
- **Campaign baseline B:** `d9c95021451c10614ce2b0c6348480ca96742b9c`
- **Evidence SHA E:** `d9c95021451c10614ce2b0c6348480ca96742b9c`
- **Audit date:** `2026-08-20`
- **Source boundary:** CMake project, install, export, catalogue, wheel,
  package-metadata, Python root, CLI, lazy entry-point shim, and public-header
  distribution declarations in `CMakeLists.txt`, `src/CMakeLists.txt`, `cmake/`,
  `pyproject.toml`, `noxfile.py`, `.pre-commit-config.yaml`,
  `.license-tools-config.json`, and `python/amazon/braket/qdmi/`.
- **Test boundary:** the four configured repository checks; distribution-owned
  CMake configure/build assertions; `test/python/test_init.py`,
  `test/python/test_main.py`, and the collection gates in `test_pennylane.py`
  and `test_pennylane_qaoa.py`; and the compile effects of public-header
  includes in `test_device.cpp`, `test_device_unit.cpp`, and `test_live.cpp`.
- **Promise boundary:** official CMake, PyPA, Python, Amazon Braket, pinned
  QDMI, pinned MQT Core, published repository documentation and metadata, the
  exact human review request retained as S26, and the S29 defender amendment.

Every `file:line` citation below was read in the detached evidence worktree at
`E`. The authoring worktree changes only this audit file.

## Scope and ownership

This audit owns installation and package metadata, generated-header and
catalogue distribution, catalogue identity and literals, native and wheel
layout, Python artifact discovery and exports, console-script behavior, and
installed PennyLane entry-point metadata. Mixed tests are split by assertion.

Native QDMI call semantics, AWS authentication and storage behavior, device
metadata, QuantumTask behavior, adapter construction and execution, and SPANK
remain in their named adjacent scopes. The live C++ file is in scope only for
offline target availability and header compilation under S6, S7, and S29. No
live test, test binary, AWS API, task, or S3 action is evidence here.

The ledger was frozen as S1-S28 before the census. A blind defender later found
the published live-target promise; S29 was added explicitly and only A0070 and
A0071 were re-prosecuted against it. No test-derived premise was added to the
ledger.

### External source pins and references

These stable reference IDs make every rung-1 citation below resolvable without
the campaign cartography. Versioned documentation is used where the repository
declares a version. GitHub consumers are pinned to exact commits. The Amazon
Braket device catalogue is time-varying official documentation, so its evidence
date is part of `X04` and S13 is a release-snapshot promise, not a perpetual
availability claim.

| Ref   | Exact external source                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         |
| :---- | :---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `X01` | [CMake 3.24 `install(TARGETS)`](https://cmake.org/cmake/help/v3.24/command/install.html#installing-targets), [GNUInstallDirs](https://cmake.org/cmake/help/v3.24/module/GNUInstallDirs.html), [`install(EXPORT)`](https://cmake.org/cmake/help/v3.24/command/install.html#installing-exports), [CMakePackageConfigHelpers](https://cmake.org/cmake/help/v3.24/module/CMakePackageConfigHelpers.html), and [`EXPORT_PROPERTIES`](https://cmake.org/cmake/help/v3.24/prop_tgt/EXPORT_PROPERTIES.html).                                                                                                                                                                          |
| `X02` | QDMI commit [`e80020f7ace5c0a716142378c812f30f86263c4e`](https://github.com/Munich-Quantum-Software-Stack/qdmi/tree/e80020f7ace5c0a716142378c812f30f86263c4e), specifically [`PrefixHandling.cmake`](https://github.com/Munich-Quantum-Software-Stack/qdmi/blob/e80020f7ace5c0a716142378c812f30f86263c4e/cmake/PrefixHandling.cmake#L18-L75) and [`prefix_defs.txt`](https://github.com/Munich-Quantum-Software-Stack/qdmi/blob/e80020f7ace5c0a716142378c812f30f86263c4e/cmake/prefix_defs.txt).                                                                                                                                                                              |
| `X03` | MQT Core commit [`0fe651210c52dbcf67e49e567ef67e1c9a33d809`](https://github.com/munich-quantum-toolkit/core/tree/0fe651210c52dbcf67e49e567ef67e1c9a33d809), specifically the [catalogue schema](https://github.com/munich-quantum-toolkit/core/blob/0fe651210c52dbcf67e49e567ef67e1c9a33d809/docs/qdmi/configuration.md#L19-L78), [relocation/runtime-copy contract](https://github.com/munich-quantum-toolkit/core/blob/0fe651210c52dbcf67e49e567ef67e1c9a33d809/docs/qdmi/configuration.md#L214-L266), and [`AddMQTQDMIDevice.cmake`](https://github.com/munich-quantum-toolkit/core/blob/0fe651210c52dbcf67e49e567ef67e1c9a33d809/cmake/AddMQTQDMIDevice.cmake#L150-L204). |
| `X04` | Official [Amazon Braket supported-device catalogue](https://docs.aws.amazon.com/braket/latest/developerguide/braket-devices.html) and [simulator task documentation](https://docs.aws.amazon.com/braket/latest/developerguide/braket-submit-tasks-simulators.html), read `2026-08-19`.                                                                                                                                                                                                                                                                                                                                                                                        |
| `X05` | scikit-build-core `0.11.6` [configuration reference](https://scikit-build-core.readthedocs.io/en/0.11.6/configuration/index.html) for `wheel.packages`, `install.components`, `wheel.install-dir`, generated files, and `wheel.py-api`.                                                                                                                                                                                                                                                                                                                                                                                                                                       |
| `X06` | PyPA [pyproject specification](https://packaging.python.org/en/latest/specifications/pyproject-toml/), [Core Metadata](https://packaging.python.org/en/latest/specifications/core-metadata/), [wheel format](https://packaging.python.org/en/latest/specifications/binary-distribution-format/), [compatibility tags](https://packaging.python.org/en/latest/specifications/platform-compatibility-tags/), and [entry points](https://packaging.python.org/en/latest/specifications/entry-points/), read `2026-08-19`.                                                                                                                                                        |
| `X07` | Python 3.10 [importlib.metadata](https://docs.python.org/3.10/library/importlib.metadata.html#distributions) and [`pathlib.Path`](https://docs.python.org/3.10/library/pathlib.html#pathlib.Path.resolve) contracts, the lowest supported interpreter reference.                                                                                                                                                                                                                                                                                                                                                                                                              |
| `X08` | PennyLane stable [plugin discovery contract](https://docs.pennylane.ai/en/stable/development/plugins.html#identifying-and-installing-your-device), read `2026-08-19`.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         |

## Spec ledger

| ID    | Promise                                                                                                                                                 | Rung  | Evidence citation                                                                                                                                                     | Affected surface                   |
| :---- | :------------------------------------------------------------------------------------------------------------------------------------------------------ | :---: | :-------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :--------------------------------- |
| `S1`  | CMake and Python publish project/distribution identity, version `1.0.1`, Python >=3.10, CMake >=3.24, and C++20.                                        |   2   | `CMakeLists.txt:18-25`; `pyproject.toml:22-27`; `README.md:44-50`; `docs/installation.md:3-10`                                                                        | Identity, version, build floors    |
| `S2`  | Python 3.10-3.14, macOS, Windows, POSIX/Linux, and typed-package support are advertised.                                                                |   2   | `README.md:1-3`; `pyproject.toml:37-55`; `python/amazon/braket/qdmi/py.typed:1-2`                                                                                     | Compatibility and typing           |
| `S3`  | Runtime and Development components install native artifacts through configurable GNUInstallDirs destinations.                                           |   1   | `src/CMakeLists.txt:148-196`; `X01`                                                                                                                                   | Native artifacts and components    |
| `S4`  | `find_package(amazon-braket-qdmi-device REQUIRED)` exposes the literal installed target `amazon-braket-qdmi-device`.                                    |   2   | `docs/installation.md:50-67`; `cmake/amazon-braket-qdmi-device-config.cmake.in:18-30`; `src/CMakeLists.txt:192-196`                                                   | Installed CMake consumer           |
| `S5`  | The lowercase CONFIG includes an unnamespaced export and a `SameMinorVersion`, architecture-sensitive version file.                                     |   1   | `src/CMakeLists.txt:148-167,192-196`; `cmake/amazon-braket-qdmi-device-config.cmake.in:18-30`; `X01`                                                                  | Package selection and export       |
| `S6`  | Development installation preserves the public generated-header paths and `constants.hpp` under the configured include directory.                        |   1   | `src/CMakeLists.txt:20-35,89-107,169-180`; `X01`                                                                                                                      | Installed C/C++ headers            |
| `S7`  | The pinned QDMI generator emits `AMAZON_BRAKET`-prefixed public headers and symbols.                                                                    |   1   | `cmake/ExternalDependencies.cmake:42-56`; `src/CMakeLists.txt:18-35`; `include/amazon-braket-qdmi-device/constants.hpp:20-26`; `X02`                                  | Generated public declarations      |
| `S8`  | The installed target exports device ID, prefix, and manifest properties consumed by pinned MQT Core runtime-copy support.                               |   1   | `src/CMakeLists.txt:130-140`; `X01`; `X03`                                                                                                                            | Exported QDMI protocol             |
| `S9`  | The stable generic ID, symbol prefix, and manifest name are `amazon.braket.default`, `AMAZON_BRAKET`, and `amazon-braket-qdmi-device.qdmi.json`.        |   2   | `README.md:27-30`; `docs/installation.md:69-73`; `src/CMakeLists.txt:112-115,130-140`; `python/amazon/braket/qdmi/__init__.py:28-39`                                  | Stable identity literals           |
| `S10` | The installed schema-v1 catalogue has supported unique records and catalogue-relative library resolution.                                               |   1   | `cmake/amazon-braket-qdmi-device.qdmi.json.in:1-100`; `src/CMakeLists.txt:112-128,182-190`; `X03`                                                                     | Catalogue validity and relocation  |
| `S11` | The enabled generic record has the stable ID/prefix/library and no fixed ARN or Region defaults.                                                        |   2   | `cmake/amazon-braket-qdmi-device.qdmi.json.in:1-10`; `docs/device_catalog.md:1-10`; `README.md:27-32`                                                                 | Generic catalogue record           |
| `S12` | The catalogue publishes the exact enabled generic-plus-nine roster, stable IDs, ARNs, Regions, library, and prefix.                                     |   2   | `cmake/amazon-braket-qdmi-device.qdmi.json.in:11-100`; `docs/device_catalog.md:8-19`; `README.md:27-32`                                                               | Concrete catalogue inventory       |
| `S13` | Concrete ARN and Region literals match the official Amazon Braket catalogue at E.                                                                       |   1   | `cmake/amazon-braket-qdmi-device.qdmi.json.in:11-100`; `X04`                                                                                                          | Provider literal validity          |
| `S14` | The catalogue is a release snapshot; generic configuration is supplied at runtime and concrete defaults are packaged.                                   |   2   | `docs/device_catalog.md:3-6,21-23`; `CHANGELOG.md:20-23`                                                                                                              | Snapshot and configuration meaning |
| `S15` | Pinned MQT Core can copy the provider runtime beside a consumer and discovers it according to static/dynamic Driver placement rules.                    |   1   | `docs/installation.md:69-88`; `X03`                                                                                                                                   | Runtime-copy and discovery         |
| `S16` | The wheel carries Python plus Runtime and Development CMake components below `amazon/braket/qdmi/data`.                                                 |   1   | `pyproject.toml:18-20,116-126`; `src/CMakeLists.txt:148-196`; `X05`; `X06`                                                                                            | Wheel composition                  |
| `S17` | Static `pyproject.toml` fields map to standardized project/Core Metadata.                                                                               |   1   | `pyproject.toml:22-81`; `X06`                                                                                                                                         | Distribution metadata              |
| `S18` | The build generates a string version module, includes it in the sdist, exports it as `__version__`, and prints it through the CLI.                      |   2   | `pyproject.toml:128-144`; `python/amazon/braket/qdmi/_version.pyi:18`; `python/amazon/braket/qdmi/__init__.py:26-35`; `python/amazon/braket/qdmi/__main__.py:60-64`   | Version API and CLI                |
| `S19` | `wheel.py-api = "py310"` participates in the installer-facing wheel tag.                                                                                |   1   | `pyproject.toml:22-25,116-126`; `X05`; `X06`                                                                                                                          | Wheel compatibility tags           |
| `S20` | The package root publishes seven named values; path exports denote existing installed native, catalogue, include, and CMake artifacts.                  |   2   | `docs/installation.md:90-102`; `python/amazon/braket/qdmi/__init__.py:26-57,86-113`                                                                                   | Public Python artifact API         |
| `S21` | Standard-library distribution lookup and strict path resolution locate installed resources; glob order is not promised.                                 |   1   | `python/amazon/braket/qdmi/__init__.py:22-50,56-100`; `X07`                                                                                                           | Installed resource resolution      |
| `S22` | Installation registers `amazon-braket-qdmi` to `amazon.braket.qdmi.__main__:main`.                                                                      |   1   | `pyproject.toml:63-64`; `X06`                                                                                                                                         | Console script                     |
| `S23` | The CLI exposes five mutually exclusive public options, prints their corresponding values, and otherwise prints help.                                   |   2   | `python/amazon/braket/qdmi/__main__.py:33-101`; `pyproject.toml:63-64`                                                                                                | CLI inventory and output           |
| `S24` | PennyLane is optional, needs Python >=3.11, and the Python-3.10 base package does not eagerly require it.                                               |   2   | `pyproject.toml:66-69`; `docs/pennylane.md:8-22`; `_pennylane_entrypoint.py:18-26`                                                                                    | Optional dependency boundary       |
| `S25` | Ten `pennylane.plugins` names map one-to-one to named lazy attributes for the catalogue roster.                                                         |   2   | `pyproject.toml:71-81`; `python/amazon/braket/qdmi/_pennylane_entrypoint.py:28-39`; `CHANGELOG.md:13-16`; `docs/pennylane.md:106-144`; `X06`; `X08`                   | Installed plugin metadata          |
| `S26` | Human review requested that the existing `build = "cp3*"` selector remain unchanged.                                                                    |   3   | `pyproject.toml:243`; [PR #168 review discussion `r3752390159`](https://github.com/munich-quantum-software/amazon-braket-qdmi-device/pull/168#discussion_r3752390159) | Cibuildwheel selector              |
| `S27` | scikit-build output remains isolated from the direct CMake build tree.                                                                                  |   2   | `CHANGELOG.md:73-77`; `pyproject.toml:116-121`                                                                                                                        | Build-output isolation             |
| `S28` | Native and Python artifacts use `Apache-2.0 WITH LLVM-exception`, with that expression and `LICENSE` in package metadata.                               |   2   | `README.md:73-78`; `docs/support.md:16-20`; `pyproject.toml:34-35`                                                                                                    | License and metadata               |
| `S29` | `BUILD_AMAZON_BRAKET_LIVE_TESTS=ON` makes the opt-in target and CTest registration available, without promising exact source inventory or live results. |   2   | `docs/development.md:13-26`; `docs/installation.md:130-137`                                                                                                           | Offline live-target availability   |

S1-S28 were the test-blind freeze. S29 is the sole amendment. Searches found no
promise for discovery order, glob count, shortest-name selection, literal
`lib`/`lib64`, exact diagnostics/help prose, record ordering, a complete wheel
filename/release matrix, or automatic discovery merely from catalogue adjacency.

## Assertion census

All IDs have prefix `DIST-d9c95021451c-`. Result keys are expanded immediately
after the table. `H01`-`H10` are the anchor records below; `V1`-`V11` are the
ranked verdicts. Every row has one owner, class, ledger mapping, and
disposition.

| Assertion ID              | Assertion and evidence citation                                                                                                             | Owner  | Class          | Ledger IDs       | Verdict/anchor and executed evidence                       |
| :------------------------ | :------------------------------------------------------------------------------------------------------------------------------------------ | :----- | :------------- | :--------------- | :--------------------------------------------------------- |
| `DIST-d9c95021451c-A0001` | `.pre-commit-config.yaml:59-63`: project metadata passes the configured validator.                                                          | `DIST` | Anchored       | `S17`            | `H01`; `T1-001R1`, `T2-001`; `R01`,`R02`                   |
| `DIST-d9c95021451c-A0002` | `.pre-commit-config.yaml:65-70`: `uv.lock` is current with dependency declarations.                                                         | `DIST` | Anchored       | `S17,S24`        | `H01`; `T1-002R1`, `T2-002`; `R01`,`R02`                   |
| `DIST-d9c95021451c-A0003` | `.pre-commit-config.yaml:89-95`; `.license-tools-config.json:2-40`: included core files carry configured author/license headers.            | `DIST` | Over-specified | `S28`            | `V9`; `T1-003R1`, `T2-003`; `R01`,`R02`                    |
| `DIST-d9c95021451c-A0004` | `.pre-commit-config.yaml:162-168`: configured Python sources and consumers pass `ty`.                                                       | `DIST` | Anchored       | `S2`             | `H01`; `T1-004R1`, `T2-004`; `R01`,`R02`                   |
| `DIST-d9c95021451c-A0005` | `CMakeLists.txt:35-40`: installed CONFIG package is findable at the project version.                                                        | `DIST` | Anchored       | `S1,S4,S5`       | `H02`; `T2-023`; `R09`                                     |
| `DIST-d9c95021451c-A0006` | `CMakeLists.txt:47-49`: every interface file-set header compiles.                                                                           | `DIST` | Anchored       | `S6,S7`          | `H02`; `T1-005R1`, `T2-005`; `R01`,`R02`                   |
| `DIST-d9c95021451c-A0007` | `CMakeLists.txt:87-92`: exported device ID is `amazon.braket.default`.                                                                      | `DIST` | Anchored       | `S8,S9`          | `H02`; `T2-024`; `R09`                                     |
| `DIST-d9c95021451c-A0008` | `CMakeLists.txt:88-95`: exported prefix is `AMAZON_BRAKET`.                                                                                 | `DIST` | Anchored       | `S7,S8,S9`       | `H02`; `T2-024P1`; `R06`                                   |
| `DIST-d9c95021451c-A0009` | `CMakeLists.txt:89-98`: exported manifest name is the stable catalogue filename.                                                            | `DIST` | Anchored       | `S8,S9`          | `H02`; `T2-024M1`; `R06`                                   |
| `DIST-d9c95021451c-A0010` | `noxfile.py:82-116`: nox installs the project with an exact no-build-isolation mechanism before pytest.                                     | `DIST` | Over-specified | `S16,S27`        | `V4`; `T1-006`, `T2-006R2`; `R03`,`R05`                    |
| `DIST-d9c95021451c-A0011` | `noxfile.py:43,119-122`: installed tests select Python 3.10 through 3.14.                                                                   | `DIST` | Anchored       | `S2`             | `H03`; `T2-025R1`; `R06`                                   |
| `DIST-d9c95021451c-A0012` | `noxfile.py:125-135`: lowest-direct dependencies install and run the suite.                                                                 | `DIST` | Anchored       | `S17,S24`        | `H03`; `T1-007`, `T2-007`; `R03`,`R02`                     |
| `DIST-d9c95021451c-A0013` | `test/CMakeLists.txt:31-43`: installed imported target links the public test executable.                                                    | `DIST` | Anchored       | `S4,S5`          | `H02`; `T2-023`; `R09`                                     |
| `DIST-d9c95021451c-A0014` | `test/python/test_init.py:30-38`: seven root exports import successfully.                                                                   | `DIST` | Anchored       | `S18,S20`        | `H03`; `T2-026R1`; `R06`                                   |
| `DIST-d9c95021451c-A0015` | `test/python/test_init.py:43`: `__version__` is truthy.                                                                                     | `DIST` | Redundant      | `S18`            | `V1`; `T1-008`, `T2-008`, `T3-001`; `R03`,`R02`,`R14`      |
| `DIST-d9c95021451c-A0016` | `test/python/test_init.py:44`: `__version__` is a `str`.                                                                                    | `DIST` | Anchored       | `S18`            | `H04`; `T2-027`; `R06`                                     |
| `DIST-d9c95021451c-A0017` | `test/python/test_init.py:45`: `__version__` is non-empty.                                                                                  | `DIST` | Anchored       | `S18`            | `H04`; `T1-008`, `T2-008`, `T3-001`; `R03`,`R02`,`R14`     |
| `DIST-d9c95021451c-A0018` | `test/python/test_init.py:50`: Python device ID equals the stable ID.                                                                       | `DIST` | Anchored       | `S9`             | `H04`; `T2-028`; `R06`                                     |
| `DIST-d9c95021451c-A0019` | `test/python/test_init.py:51`: Python prefix equals `AMAZON_BRAKET`.                                                                        | `DIST` | Anchored       | `S9`             | `H04`; `T2-029`; `R06`                                     |
| `DIST-d9c95021451c-A0020` | `test/python/test_init.py:56`: installed catalogue is readable JSON.                                                                        | `DIST` | Anchored       | `S10,S16,S20`    | `H05`; `T2-030R2`; `R10`                                   |
| `DIST-d9c95021451c-A0021` | `test/python/test_init.py:57`: catalogue has `qdmi.devices` and record IDs.                                                                 | `DIST` | Anchored       | `S10`            | `H05`; `T2-031R1`; `R10`                                   |
| `DIST-d9c95021451c-A0022` | `test/python/test_init.py:58-69`: ID set is exactly generic plus nine concrete IDs.                                                         | `DIST` | Anchored       | `S11,S12`        | `H05`; `T2-032R1`; `R10`                                   |
| `DIST-d9c95021451c-A0023` | `test/python/test_init.py:70`: generic record has no `session` member.                                                                      | `DIST` | Over-specified | `S11,S14`        | `V7`; `T1-009`, `T2-009`, `T3-002R1`; `R03`,`R02`,`R15`    |
| `DIST-d9c95021451c-A0024` | `test/python/test_init.py:71-72`: every catalogue prefix is the stable prefix.                                                              | `DIST` | Anchored       | `S9,S11,S12`     | `H05`; `T2-029`; `R06`                                     |
| `DIST-d9c95021451c-A0025` | `test/python/test_init.py:73-74`: every concrete base URL has the Braket ARN prefix.                                                        | `DIST` | Anchored       | `S12,S13`        | `H05`; `T2-033R1`; `R10`                                   |
| `DIST-d9c95021451c-A0026` | `test/python/test_init.py:79`: exported include path exists.                                                                                | `DIST` | Anchored       | `S6,S16,S20,S21` | `H06`; `T1-010`, `T2-010`; `R03`,`R02`                     |
| `DIST-d9c95021451c-A0027` | `test/python/test_init.py:80`: exported include path is a directory.                                                                        | `DIST` | Anchored       | `S6,S16,S20`     | `H06`; `T1-011`, `T2-011`; `R01`,`R04`                     |
| `DIST-d9c95021451c-A0028` | `test/python/test_init.py:81`: include path string contains `include`.                                                                      | `DIST` | Over-specified | `S20`            | `V8`; `T1-012`, `T2-012I1`, `T3-003R2`; `R01`,`R16`,`R17`  |
| `DIST-d9c95021451c-A0029` | `test/python/test_init.py:84-87`: generated-header child exists.                                                                            | `DIST` | Anchored       | `S6,S7,S16,S20`  | `H06`; `T1-010`, `T2-010`; `R03`,`R02`                     |
| `DIST-d9c95021451c-A0030` | `test/python/test_init.py:84-88`: generated-header child is a directory.                                                                    | `DIST` | Anchored       | `S6,S7,S16,S20`  | `H06`; `T2-010`; `R02`                                     |
| `DIST-d9c95021451c-A0031` | `test/python/test_init.py:93`: exported CMake path exists.                                                                                  | `DIST` | Anchored       | `S5,S16,S20`     | `H06`; `T1-010`, `T2-010`; `R03`,`R02`                     |
| `DIST-d9c95021451c-A0032` | `test/python/test_init.py:94`: exported CMake path is a directory.                                                                          | `DIST` | Anchored       | `S5,S16,S20`     | `H06`; `T2-010`; `R02`                                     |
| `DIST-d9c95021451c-A0033` | `test/python/test_init.py:95`: CMake path string contains `cmake`.                                                                          | `DIST` | Over-specified | `S20`            | `V8`; `T1-012`, `T2-012C1`; `R01`,`R16`                    |
| `DIST-d9c95021451c-A0034` | `test/python/test_init.py:100`: exported native-library path exists.                                                                        | `DIST` | Anchored       | `S3,S16,S20`     | `H06`; `T1-010`, `T2-010`; `R03`,`R02`                     |
| `DIST-d9c95021451c-A0035` | `test/python/test_init.py:101`: exported native-library path is a file.                                                                     | `DIST` | Anchored       | `S3,S16,S20`     | `H06`; `T2-010`; `R02`                                     |
| `DIST-d9c95021451c-A0036` | `test/python/test_init.py:102`: library path string contains the project name.                                                              | `DIST` | Over-specified | `S3,S4,S20`      | `V8`; `T1-012`, `T2-012O4`, `T3-004`; `R01`,`R16`,`R17`    |
| `DIST-d9c95021451c-A0037` | `test/python/test_init.py:107`: include export is `pathlib.Path`.                                                                           | `DIST` | Anchored       | `S20`            | `H07`; `T1-013`, `T2-013R1`; `R01`,`R07`                   |
| `DIST-d9c95021451c-A0038` | `test/python/test_init.py:108`: CMake export is `pathlib.Path`.                                                                             | `DIST` | Anchored       | `S20`            | `H07`; `T1-013`, `T2-013R1`; `R01`,`R07`                   |
| `DIST-d9c95021451c-A0039` | `test/python/test_init.py:109`: library export is `pathlib.Path`.                                                                           | `DIST` | Anchored       | `S20`            | `H07`; `T1-013`, `T2-013R1`; `R01`,`R07`                   |
| `DIST-d9c95021451c-A0040` | `test/python/test_init.py:110`: catalogue export is `pathlib.Path`.                                                                         | `DIST` | Anchored       | `S20`            | `H07`; `T1-013`, `T2-013R1`; `R01`,`R07`                   |
| `DIST-d9c95021451c-A0041` | `test/python/test_init.py:115`: include export is absolute.                                                                                 | `DIST` | Anchored       | `S20,S21`        | `H07`; `T1-014`, `T2-014R1`; `R01`,`R07`                   |
| `DIST-d9c95021451c-A0042` | `test/python/test_init.py:116`: CMake export is absolute.                                                                                   | `DIST` | Anchored       | `S20,S21`        | `H07`; `T1-014`, `T2-014R1`; `R01`,`R07`                   |
| `DIST-d9c95021451c-A0043` | `test/python/test_init.py:117`: library export is absolute.                                                                                 | `DIST` | Anchored       | `S20,S21`        | `H07`; `T1-014`, `T2-014R1`; `R01`,`R07`                   |
| `DIST-d9c95021451c-A0044` | `test/python/test_init.py:118`: catalogue export is absolute.                                                                               | `DIST` | Anchored       | `S20,S21`        | `H07`; `T1-014`, `T2-014R1`; `R01`,`R07`                   |
| `DIST-d9c95021451c-A0045` | `test/python/test_init.py:123-130`: plugin names equal catalogue IDs.                                                                       | `DIST` | Anchored       | `S12,S25`        | `H05`; `T2-032R1`; `R10`                                   |
| `DIST-d9c95021451c-A0046` | `test/python/test_init.py:131-134`: plugin targets use the expected lazy target prefix.                                                     | `DIST` | Anchored       | `S25`            | `H05`; `T2-034`; `R11`                                     |
| `DIST-d9c95021451c-A0047` | `test/python/test_init.py:136-137`: lazy shim dictionary lacks a `pennylane` key.                                                           | `DIST` | Over-specified | `S24,S25`        | `V6`; `T1-015`, `T2-015R1`; `R01`,`R06`                    |
| `DIST-d9c95021451c-A0048` | `test/python/test_main.py:24-30`: CLI tests import four paths and version.                                                                  | `DIST` | Anchored       | `S18,S20`        | `H08`; `T2-026R1`; `R06`                                   |
| `DIST-d9c95021451c-A0049` | `test/python/test_main.py:38-39`: `--help` exits successfully.                                                                              | `DIST` | Anchored       | `S22,S23`        | `H08`; `T2-035`; `R11`                                     |
| `DIST-d9c95021451c-A0050` | `test/python/test_main.py:40`: help contains `Command line interface`.                                                                      | `DIST` | Over-specified | `S23`            | `V2`; `T1-016`, `T2-016`; `R22`,`R07`                      |
| `DIST-d9c95021451c-A0051` | `test/python/test_main.py:41`: help contains `--include_dir`.                                                                               | `DIST` | Anchored       | `S23`            | `H08`; `T2-036`; `R11`                                     |
| `DIST-d9c95021451c-A0052` | `test/python/test_main.py:46-47`: `--version` exits successfully.                                                                           | `DIST` | Anchored       | `S22,S23`        | `H08`; `T2-035`; `R11`                                     |
| `DIST-d9c95021451c-A0053` | `test/python/test_main.py:48`: version output contains `__version__`.                                                                       | `DIST` | Anchored       | `S18,S23`        | `H08`; `T2-037`; `R11`                                     |
| `DIST-d9c95021451c-A0054` | `test/python/test_main.py:53-54`: `--include_dir` exits successfully.                                                                       | `DIST` | Anchored       | `S22,S23`        | `H08`; `T2-038`; `R11`                                     |
| `DIST-d9c95021451c-A0055` | `test/python/test_main.py:55`: include option prints the exported path.                                                                     | `DIST` | Anchored       | `S20,S23`        | `H08`; `T2-038V1`; `R12`                                   |
| `DIST-d9c95021451c-A0056` | `test/python/test_main.py:60-61`: `--cmake_dir` exits successfully.                                                                         | `DIST` | Anchored       | `S22,S23`        | `H08`; `T2-039`; `R12`                                     |
| `DIST-d9c95021451c-A0057` | `test/python/test_main.py:62`: CMake option prints the exported path.                                                                       | `DIST` | Anchored       | `S20,S23`        | `H08`; `T2-039V1`; `R12`                                   |
| `DIST-d9c95021451c-A0058` | `test/python/test_main.py:67-68`: `--lib_path` exits successfully.                                                                          | `DIST` | Anchored       | `S22,S23`        | `H08`; `T2-040`; `R12`                                     |
| `DIST-d9c95021451c-A0059` | `test/python/test_main.py:69`: library option prints the exported path.                                                                     | `DIST` | Anchored       | `S20,S23`        | `H08`; `T2-040V1`; `R12`                                   |
| `DIST-d9c95021451c-A0060` | `test/python/test_main.py:74-75`: `--catalog_path` exits successfully.                                                                      | `DIST` | Anchored       | `S22,S23`        | `H08`; `T2-041`; `R12`                                     |
| `DIST-d9c95021451c-A0061` | `test/python/test_main.py:76`: catalogue option prints the exported path.                                                                   | `DIST` | Anchored       | `S20,S23`        | `H08`; `T2-041V1`; `R12`                                   |
| `DIST-d9c95021451c-A0062` | `test/python/test_pennylane.py:27-28`: pre-3.11 collection skips with exact local reason.                                                   | `DIST` | Over-specified | `S24`            | `V5`; `T1-017R1`, `T2-017`; `R21`,`R07`                    |
| `DIST-d9c95021451c-A0063` | `test/python/test_pennylane.py:30-33`: missing optional import skips the module.                                                            | `DIST` | Anchored       | `S24`            | `H09`; `T1-018`, `T2-018R1`, `T3-005R3`; `R22`,`R07`,`R18` |
| `DIST-d9c95021451c-A0064` | `test/python/test_pennylane_qaoa.py:32-33`: pre-3.11 QAOA collection skips with exact local reason.                                         | `DIST` | Over-specified | `S24`            | `V5`; `T1-019R1`, `T2-019`; `R21`,`R07`                    |
| `DIST-d9c95021451c-A0065` | `test/python/test_pennylane_qaoa.py:35-38`: missing optional import skips QAOA.                                                             | `DIST` | Anchored       | `S24`            | `H09`; `T1-020`, `T2-020`; `R22`,`R07`                     |
| `DIST-d9c95021451c-A0066` | `test/test_device.cpp:41`: installed/public consumer compiles `constants.hpp`.                                                              | `DIST` | Anchored       | `S6`             | `H10`; `T2-022`; `R08`                                     |
| `DIST-d9c95021451c-A0067` | `test/test_device.cpp:42`: public consumer directly includes generated `device.h`.                                                          | `DIST` | Over-specified | `S6,S7`          | `V3`; `T1-021R1`, `T2-021`; `R13`,`R07`                    |
| `DIST-d9c95021451c-A0068` | `test/test_device_unit.cpp:37`: source unit compiles `constants.hpp`.                                                                       | `DIST` | Anchored       | `S6`             | `H10`; `T1-022R1`, `T2-022`; `R13`,`R08`                   |
| `DIST-d9c95021451c-A0069` | `test/test_device_unit.cpp:38`: source unit directly includes generated `device.h`.                                                         | `DIST` | Over-specified | `S7`             | `V3`; `T1-021R1`, `T2-021`; `R13`,`R07`                    |
| `DIST-d9c95021451c-A0070` | `test/test_live.cpp:20`: opt-in live target compiles `constants.hpp`.                                                                       | `DIST` | Anchored       | `S6,S29`         | `H10`; `T1-023`, `T2-022`, `T3-006R2`; `R13`,`R08`,`R19`   |
| `DIST-d9c95021451c-A0071` | `test/test_live.cpp:21`: opt-in live target directly includes generated `device.h`.                                                         | `DIST` | Over-specified | `S7,S29`         | `V3`; `T1-021R1`, `T2-021`; `R13`,`R07`                    |
| `DIST-d9c95021451c-A0072` | `test/python/test_init.py:138-141`: Python 3.10 entry-point loading raises within the `ImportError` exception domain, including subclasses. | `DIST` | Over-specified | `S24,S25`        | `V10`; `T2-042R1`; `R23`                                   |
| `DIST-d9c95021451c-A0073` | `test/python/test_init.py:138-141`: the Python-floor error lexically matches `Python 3.11`.                                                 | `DIST` | Over-specified | `S24,S25`        | `V10`; `T2-043R1`; `R23`                                   |
| `DIST-d9c95021451c-A0074` | `CMakeLists.txt:63-65`: live ON/tests OFF is rejected with a fatal configure error rather than another no-silent-omission policy.           | `DIST` | Over-specified | `S29`            | `V11`; `T2-044`; `R24`                                     |

Result-key paths, relative to the campaign scope directory:

- `R01`: `evidence-results/results-amendment01-R1-T1-015.md`
- `R02`: `evidence-results/results-T2-001-T2-010.md`
- `R03`: `evidence-results/results-T1-001-T1-010.md`
- `R04`: `evidence-results/results-T2-006R1-T2-012-pause.md`
- `R05`: `evidence-results/results-amendment03-R2-T2-014-pause.md`
- `R06`: `evidence-results/results-amendment07-T2-024P1-T2-031.md`
- `R07`: `evidence-results/results-amendments04-06-T2-012R2-T2-021.md`
- `R08`: `evidence-results/results-T2-022.md`
- `R09`: `evidence-results/results-T2-023-T2-026.md`
- `R10`: `evidence-results/results-amendments08-09-coupled-reruns.md`
- `R11`: `evidence-results/results-T2-034-T2-038.md`
- `R12`: `evidence-results/results-amendment10-T2-039-T2-041-value-splits.md`
- `R13`: `evidence-results/results-reactivation-T1-021R1-T1-023.md`
- `R14`: `evidence-results/results-T3-001.md`
- `R15`: `evidence-results/results-T3-002R1.md`
- `R16`: `evidence-results/results-amendment13-T2-splits.md`
- `R17`: `evidence-results/results-amendment13-T3-relocation-pairs.md`
- `R18`: `evidence-results/results-T3-005R3-final.md`
- `R19`: `evidence-results/results-T3-006R2-final.md`
- `R20`: `evidence-results/results-T3-007-final.md`
- `R21`: `evidence-results/results-amendment02-R1-T1-022.md`
- `R22`: `evidence-results/results-amendment01-T1-016-T1-020.md`
- `R23`: `census-amendment/extension01-results.md`
- `R24`: `census-amendment/final-reconciliation-paused.md`

Class totals are 57 Anchored, 16 Over-specified, 1 Redundant, zero
Contract-free, and zero Coverage-driven assertions.

## Summary

Verdicts are ranked by useful simplification per unit of consumer risk. A
verdict is an evidence-backed proposal, not a maintainer decision.

| Verdict | Assertion IDs       | Class          | Remedy                                                                                              | Tier  | Unlock                                                                         | Risk   | Decision                       | Resolution                          |
| :------ | :------------------ | :------------- | :-------------------------------------------------------------------------------------------------- | :---: | :----------------------------------------------------------------------------- | :----- | :----------------------------- | :---------------------------------- |
| `V1`    | `A0015`             | Redundant      | Delete truthiness check; retain A0016/A0017.                                                        |  T3   | One duplicate test predicate; no production change.                            | Low    | Accepted                       | Applied                             |
| `V2`    | `A0050`             | Over-specified | Replace one prose phrase with semantic help inventory.                                              |  T2   | CLI prose editing only.                                                        | Low    | Accepted                       | Applied                             |
| `V3`    | `A0067,A0069,A0071` | Over-specified | Retain direct test-TU includes; add explicit standalone header consumers and a reaching pair.       | T1/T2 | Standalone reachability is covered without fighting include-cleaner ownership. | Medium | Accepted                       | Narrowed                            |
| `V4`    | `A0010`             | Over-specified | Keep installed-wheel testing; stop requiring one uv isolation switch.                               |  T2   | Nox mechanism simplification only.                                             | Medium | Rejected                       | Not applicable                      |
| `V5`    | `A0062,A0064`       | Over-specified | Preserve pre-import version gates; stop pinning local reason text.                                  |  T2   | Reason wording only; no floor change.                                          | Medium | Accepted                       | Applied                             |
| `V6`    | `A0047`             | Over-specified | Add a base-wheel import blocker, then replace the dictionary proxy.                                 |  T2   | Private shim namespace only.                                                   | High   | Accepted                       | Applied                             |
| `V7`    | `A0023`             | Over-specified | Assert the promised forbidden defaults, not member absence.                                         |  T3   | Catalogue representation only after parser evidence.                           | High   | Accepted                       | Applied                             |
| `V8`    | `A0028,A0033,A0036` | Over-specified | Replace lexical substrings with real installed consumers.                                           | T2/T3 | Test-only lexical freedom; no layout or filename change.                       | High   | Accepted                       | Applied                             |
| `V9`    | `A0003`             | Over-specified | Retain license enforcement; stop forcing one author only after a license-only fault.                |  T2   | Authorship-policy flexibility only.                                            | Medium | Rejected                       | Not applicable                      |
| `V10`   | `A0072,A0073`       | Over-specified | Add exact S25 mapping and sentinel Python-floor oracles before narrowing exception type or wording. |  T2   | No gate/type/text change is cleared yet.                                       | High   | A0072 Rejected; A0073 Accepted | A0072 Not applicable; A0073 Applied |
| `V11`   | `A0074`             | Over-specified | Preserve no silent omission; choose and test a documented superproject-safe option policy.          |  T2   | Fatal refusal is not the only possible mechanism; CACHE FORCE is not approved. | High   | Accepted                       | Applied                             |

## Verdicts

### V1. One version truthiness predicate duplicates non-emptiness - Redundant

**Assertions.** `DIST-d9c95021451c-A0015` at `test/python/test_init.py:43`;
retained same-class assertions A0016 and A0017 are at lines 44-45.

**Promise.** S18 promises a generated string version exported as `__version__`;
it does not require both truthiness and length predicates.

**Provenance.** Commit `33d8487b12fad74ed674db08bc312d0823b5ab7b` introduced
A0015 and A0017 together after the generated version export already existed.
This is a candidate signal only; the verdict comes from the experiment.

**Experiment.** `T1-008`, `T2-008`, and paired `T3-001`; results R03, R02, and
R14.

```text
command             : REPRO_IDS=T3-001 bash <(awk '/^### Commands$/{s=1;next}/^#### Complete canonical dispatch matrix$/{exit}s&&/^```sh$/{c=1;next}s&&c&&/^```$/{c=0;print "";next}s&&c{print}' .agent/audits/distribution-catalog-and-python-shell.md)
campaign baseline B : d9c95021451c10614ce2b0c6348480ca96742b9c
evidence SHA E      : d9c95021451c10614ce2b0c6348480ca96742b9c
test selection       : uvx nox -s tests-3.14 -- test/python/test_init.py
expected observation : empty version has the same complete killed set without A0015
observed failures    : test/python/test_init.py::test_version_exists on both sides
accused assertion    : omitted only on the narrowed side
same-class oracles   : A0017, with A0016 retaining type
coverage before      : 174 statements/34 covered; 50 branches/7 covered
coverage after       : identical
probe-owned edit     : restored
post-command status  : clean, detached at E; both worktrees removed
```

**T3 comparison.** Current and A0015-omitted worktrees received the identical
empty-generated-version fault. Both exact environments coupled the empty
installed version and both complete killed sets were exactly
`{test_version_exists}`. Baselines passed.

**Adjudication.** A0015 is redundant with retained A0017 for the non-empty
version equivalence class. A0016 remains an independent string-type anchor.

**Remedy.** Delete only `assert __version__`; retain the type and explicit
non-empty predicates.

**Unlock.** One duplicate test predicate; no production deletion or version
format change.

**Risk and release impact.** Low. Failure localization changes by one line; the
contract-breaking empty version remains killed. This does not block a release
after the retained tests pass.

**Maintainer decision.** `Accepted`

**Decision reason.** The truthiness predicate duplicates the retained explicit
non-empty check. Remove only A0015 and keep A0016 and A0017.

**Resolution state.** `Applied`

**Closure evidence.** Commit `454c203191dc3517e838079a0f90ac6d2656ce28` removed
only A0015. A0016 and A0017 remain at R. The exact-R Python matrix and lint
passed. Closure remains conditional on the non-squash merge described in
Reconciliation.

### V2. CLI help should not freeze one sentence - Over-specified

**Assertions.** `DIST-d9c95021451c-A0050` at `test/python/test_main.py:40`.

**Promise.** S23 promises useful help and the public option inventory, while its
bound explicitly excludes exact rendering and wording.

**Provenance.** Commit `33d8487b12fad74ed674db08bc312d0823b5ab7b` added the
phrase assertion after the argparse description already existed. There was no
human request for the exact sentence.

**Experiment.** `T1-016` omitted the phrase without coverage change. `T2-016`
replaced it with the synonymous `Amazon Braket QDMI command-line interface.` and
failed only `test_cli_help`; results R22 and R07.

```text
command             : REPRO_IDS=T1-016,T2-016 bash <(awk '/^### Commands$/{s=1;next}/^#### Complete canonical dispatch matrix$/{exit}s&&/^```sh$/{c=1;next}s&&c&&/^```$/{c=0;print "";next}s&&c{print}' .agent/audits/distribution-catalog-and-python-shell.md)
campaign baseline B : d9c95021451c10614ce2b0c6348480ca96742b9c
evidence SHA E      : d9c95021451c10614ce2b0c6348480ca96742b9c
test selection       : uvx nox -s tests-3.14 -- test/python/test_main.py
expected observation : synonymous prose trips only the lexical help assertion
observed failures    : test/python/test_main.py::test_cli_help
accused assertion    : failed
same-class oracles   : none for the exact phrase; A0049/A0051 retain help behavior
coverage before      : 174 statements/54 covered; 50 branches/16 covered
coverage after       : identical in T1-016
probe-owned edit     : restored
post-command status  : clean, detached at E
```

**T3 comparison.** Not run; the isolated T2 fault and S23 bound settle the
lexical class.

**Adjudication.** The help contract is real, but exact prose is tighter than
S23. This is not Contract-free because useful help and stable options remain
promised.

**Remedy.** Replace the sentence assertion with a parameterized check for the
five S23 option tokens plus successful help status. Do not replace it with a
different prose fragment.

**Unlock.** CLI help may be reworded or localized; option names and semantics
remain fixed.

**Risk and release impact.** Low. A token-only check can admit unhelpful prose,
so review help text as user documentation. No production API is removed.

**Maintainer decision.** `Accepted`

**Decision reason.** Exact help prose is not a contract. Replace only the phrase
check with semantic public-option coverage while retaining successful help
behavior.

**Resolution state.** `Applied`

**Closure evidence.** Commit `e08d323f40eb38dd6ba629dcabcdb42feff075b5` added
the complete semantic option inventory before commit
`454c203191dc3517e838079a0f90ac6d2656ce28` removed the prose assertion. The
exact-R Python matrix passed. Closure remains conditional on the non-squash
merge described in Reconciliation.

### V3. Test TUs pin a direct generated-header include mechanism - Over-specified

**Assertions.** `DIST-d9c95021451c-A0067` at `test/test_device.cpp:42`, A0069 at
`test/test_device_unit.cpp:38`, and A0071 at `test/test_live.cpp:21`.

**Promise.** S6 and S7 promise installed generated headers; S29 promises the
opt-in target, not its exact include inventory. The adjacent `constants.hpp`
include intentionally includes generated `device.h` at
`include/amazon-braket-qdmi-device/constants.hpp:26`.

**Provenance.** A0067 arrived with prefix generation, A0069 with the source
unit, and A0071 with the live target. These split/co-introduction facts do not
decide whether a direct include is required in each TU.

**Experiment.** `T1-021R1` built installed, source, and PRE_TEST live targets
with the three direct lines removed. `T2-021` killed a generator-prefix fault in
producer, source, and live build contexts, although its installed-consumer fault
compile was not reached; results R13 and R07.

```text
command             : REPRO_IDS=T1-021R1,T2-021 bash <(awk '/^### Commands$/{s=1;next}/^#### Complete canonical dispatch matrix$/{exit}s&&/^```sh$/{c=1;next}s&&c&&/^```$/{c=0;print "";next}s&&c{print}' .agent/audits/distribution-catalog-and-python-shell.md)
campaign baseline B : d9c95021451c10614ce2b0c6348480ca96742b9c
evidence SHA E      : d9c95021451c10614ce2b0c6348480ca96742b9c
test selection       : configure PRE_TEST; build only amazon-braket-qdmi-device-live-test
expected observation : direct-include omission preserves target compilation
observed failures    : none in T1-021R1 omission builds
accused assertion    : omitted in the narrowed builds
same-class oracles   : adjacent constants.hpp includes generated device.h
coverage before      : compile-only; not measured
coverage after       : compile-only; not measured
probe-owned edit     : restored
post-command status  : clean, detached at E; temporary worktrees removed
```

**T3 comparison.** `T3-007` failed before the selected live TU in both sides.
The fresh reaching attempt `T3-008` then stopped non-adjudicably in its current
fault arm before any narrowed arm. Neither result is admitted as same-class or
redundancy evidence.

**Adjudication.** S6, S7, and S29 protect generated-header availability and the
three build contexts, not a direct include in these exact TUs. The successful
omission builds show that the source inventory is tighter than those promises.
The missing reaching comparison prevents a Redundant verdict and prevents
deletion now. The three assertions are Over-specified mechanism constraints.

**Remedy.** Retain the three includes now. Before removing any one, add an
independent installed consumer that includes only generated `device.h`, retain
the source/live `constants.hpp` consumers, and run a fresh paired fault that
reaches exactly `test_device.cpp`, `test_device_unit.cpp`, and `test_live.cpp`
with identical current/narrowed failed-TU sets. Then remove only the three
direct test-source lines; never remove or relocate the generated header.

**Unlock.** No removal is cleared by this audit. A future reaching comparison
may permit local test-source include cleanup only.

**Risk and release impact.** Medium. Exact-E omission builds pass, but no
admissible fault proves that the retained transitive route catches the same
contract break in all three selected TUs and installed mode.

**Maintainer decision.** `Accepted`

**Decision reason.** The direct includes are test-source mechanism constraints.
Keep resolution bounded to the three local include lines and the minimum
replacement and reaching evidence. Do not add a general header framework or
change generated headers, public headers, production layout, or live behavior.

**Resolution state.** `Narrowed`

**Closure evidence.** Commit `fa98c4d8b016fcce45495e7d9896cd6d36451b84` added
the dedicated generated C header consumer; commit
`2232c69e2dce7aae515e0864b7c49ada91552dbb` then removed only the three direct
test-TU includes. At exact R, fresh compile-only evidence made
`test_device.cpp`, `test_device_unit.cpp`, and `test_live.cpp` all reach and
reject the same generated-header sentinel through the retained transitive
include. The dedicated source and installed C consumer also rejected the
sentinel; `test_measurement.cpp` remained the negative control. Baseline and
recovery compiles passed. No test discovery, binary, live behavior, network, or
AWS action ran. Hosted C++ lint job `96476973181` then proved the source cleanup
conflicted with the repository's include-cleaner policy: `test/test_device.cpp`
uses QDMI symbols and must directly include the generated public header that
provides them. The resolution is therefore narrowed to keep the standalone
source and installed generated-header consumers while restoring the three direct
test-TU includes. A later C++ lint rerun on the same interim source then flagged
the additive `test/test_generated_device_header.c` include as unused because the
consumer only included the generated header. The final source follow-up makes
that C translation unit take the address of the generated
`AMAZON_BRAKET_QDMI_device_initialize` declaration. This preserves compile/link
reachability without running QDMI initialization, suppressing lint, changing
generated headers, or touching live behavior. Closure remains conditional on the
non-squash merge described in Reconciliation.

### V4. Nox pins an internal build-isolation mechanism - Over-specified

**Assertions.** `DIST-d9c95021451c-A0010` at `noxfile.py:82-116`.

**Promise.** S16 requires installed wheel contents and S27 requires output
isolation. Neither promises `uv sync --no-build-isolation-package` as the
permanent mechanism.

**Provenance.** `_run_tests` and the no-build-isolation path came from
`a78f5924a3dce039c00ae6d83c0e8905f1b9d47d`; a later commit changed only the
project literal. Packaging predates that orchestration.

**Experiment.** `T1-006` passed with the mechanism omitted and identical
coverage. In corrected `T2-006R2`, a broken wheel install directory still failed
exact selected-module collection after a successful fault build/install; results
R03 and R05.

```text
command             : REPRO_IDS=T1-006,T2-006R2 bash <(awk '/^### Commands$/{s=1;next}/^#### Complete canonical dispatch matrix$/{exit}s&&/^```sh$/{c=1;next}s&&c&&/^```$/{c=0;print "";next}s&&c{print}' .agent/audits/distribution-catalog-and-python-shell.md)
campaign baseline B : d9c95021451c10614ce2b0c6348480ca96742b9c
evidence SHA E      : d9c95021451c10614ce2b0c6348480ca96742b9c
test selection       : installed test/python/test_init.py, staged build/install/collect
expected observation : installed-artifact fault remains detectable without mechanism claim
observed failures    : collection FileNotFoundError; 20 errors, 0 failures
accused assertion    : T1 omission passed; exact mechanism was not the oracle
same-class oracles   : installed package collection and artifact tests
coverage before      : 174 statements/34 covered; 50 branches/7 covered
coverage after       : identical in T1-006
probe-owned edit     : restored
post-command status  : clean, detached at E
```

**T3 comparison.** Not run.

**Adjudication.** Installed-package testing is anchored, but one uv backend
isolation switch is an over-specified implementation mechanism.

**Remedy.** Change the supported nox path to one normal isolated backend
build/install followed by the same installed pytest selection. Keep distinct
scikit-build output and reject source-tree imports.

**Unlock.** Test orchestration may use the backend's normal isolation path;
wheel contents and installed-resource behavior do not change.

**Risk and release impact.** Medium. Before applying, run the supported and
minimum sessions to prove build dependencies are complete and the installed
wheel, not the source tree, is selected.

**Maintainer decision.** `Rejected`

**Decision reason.** The persistent build directory and explicit uv isolation
are an intentional incremental-rebuild performance design. Retain this
mechanism.

**Resolution state.** `Not applicable`

**Closure evidence.** The intentional mechanism remains unchanged. The rejected
decision is recorded in `e934b6249c49f63cbf0bb04d352c24c5c8fd03c3`; closure
remains conditional on the non-squash merge described in Reconciliation.

### V5. Optional-module gates pin local reason text - Over-specified

**Assertions.** `DIST-d9c95021451c-A0062` at
`test/python/test_pennylane.py:27-28` and A0064 at
`test/python/test_pennylane_qaoa.py:32-33`.

**Promise.** S24 requires the pre-import Python-3.11 integration floor and a
Python-3.10-compatible base package. It does not promise exact skip text or two
independently maintained diagnostic strings.

**Provenance.** Commit `8c2d17eaba1ee4dc87a8da7a751527c451da0086` co-introduced
both gates, their modules, and the optional dependency. Co-introduction is not
the verdict.

**Experiment.** `T1-017R1` and `T1-019R1` changed only reason text and retained
the exact named-module tuples: collected 0, skipped 1, errors/failures 0.
`T2-017` and `T2-019` lowered package metadata while the local gates still
skipped unchanged. Results R21 and R07.

```text
command             : REPRO_IDS=T1-017R1,T1-019R1,T2-017,T2-019 bash <(awk '/^### Commands$/{s=1;next}/^#### Complete canonical dispatch matrix$/{exit}s&&/^```sh$/{c=1;next}s&&c&&/^```$/{c=0;print "";next}s&&c{print}' .agent/audits/distribution-catalog-and-python-shell.md)
campaign baseline B : d9c95021451c10614ce2b0c6348480ca96742b9c
evidence SHA E      : d9c95021451c10614ce2b0c6348480ca96742b9c
test selection       : nox tests-3.10 -- --collect-only each exact module
expected observation : wording changes do not alter the pre-import skip tuple
observed failures    : none; each module reported one named skip
accused assertion    : gate stayed effective; exact reason was immaterial
same-class oracles   : S24 metadata and both module-level version gates
coverage before      : collection only; no test function executed
coverage after       : collection only; no test function executed
probe-owned edit     : restored
post-command status  : clean, detached at E
```

**T3 comparison.** Not run.

**Adjudication.** Only the exact local reason text is over-specified. The
pre-import version gates are anchored and must remain.

**Remedy.** Make the skip reason reason-agnostic or derive the text from a
single constant while retaining both pre-import `sys.version_info` checks. Do
not centralize or move the gates after imports until a Python 3.10 base-only and
Python 3.11 extra-installed implementation is executed.

**Unlock.** Diagnostic wording may change. This verdict does not lower the
integration floor or authorize an untested shared-gate refactor.

**Risk and release impact.** Medium. A gate moved after optional imports can
break supported Python-3.10 base installs; that change remains blocked.

**Maintainer decision.** `Accepted`

**Decision reason.** Skip reason text is not stable. Keep both pre-import
Python-floor gates and narrow only the wording assertions.

**Resolution state.** `Applied`

**Closure evidence.** Commit `e08d323f40eb38dd6ba629dcabcdb42feff075b5` added
one shared semantic floor and reason owner. Commit
`454c203191dc3517e838079a0f90ac6d2656ce28` moved both pre-import module gates to
that owner without weakening the Python floor. The exact-R Python matrix passed.
Closure remains conditional on the non-squash merge described in Reconciliation.

### V6. Lazy import is tested through a private dictionary key - Over-specified

**Assertions.** `DIST-d9c95021451c-A0047` at `test/python/test_init.py:136-137`.

**Promise.** S24 requires a base package that does not eagerly require
PennyLane; S25 owns installed lazy targets. Neither promises absence of a name
from a private module dictionary.

**Provenance.** Commit `8c2d17eaba1ee4dc87a8da7a751527c451da0086` created the
shim and this proxy assertion together. That normal co-introduction is not
adjudicative.

**Experiment.** `T1-015` omitted A0047 with zero coverage delta. `T2-015R1`
added harmless `pennylane = None`; build/install/collection passed and only
`test_pennylane_entry_point_is_lazy_on_the_base_install` failed. Results R01 and
R06.

```text
command             : REPRO_IDS=T1-015,T2-015R1 bash <(awk '/^### Commands$/{s=1;next}/^#### Complete canonical dispatch matrix$/{exit}s&&/^```sh$/{c=1;next}s&&c&&/^```$/{c=0;print "";next}s&&c{print}' .agent/audits/distribution-catalog-and-python-shell.md)
campaign baseline B : d9c95021451c10614ce2b0c6348480ca96742b9c
evidence SHA E      : d9c95021451c10614ce2b0c6348480ca96742b9c
test selection       : uvx nox -s tests-3.14 -- test/python/test_init.py
expected observation : harmless namespace binding fails only the private proxy
observed failures    : test_pennylane_entry_point_is_lazy_on_the_base_install
accused assertion    : failed
same-class oracles   : none; replacement is required
coverage before      : 174 statements/34 covered; 50 branches/7 covered
coverage after       : identical in T1-015
probe-owned edit     : restored
post-command status  : clean, detached at E
```

**T3 comparison.** Not run.

**Adjudication.** The proxy is tighter than the promised import boundary. The
experiment does not prove that deletion without replacement is safe.

**Remedy.** First add an isolated base-wheel subprocess on Python 3.10 and 3.11+
that blocks `pennylane` and `mqt.core`, imports the root and shim, enumerates
entry points, and kills an eager-import mutant. Only then replace the dictionary
assertion.

**Unlock.** Private shim names may change after a behavioral replacement exists.
Eager loading remains forbidden.

**Risk and release impact.** High. Removing the proxy first could make the base
wheel or CLI require the optional extra. Remediation is blocked on the
replacement experiment.

**Maintainer decision.** `Accepted`

**Decision reason.** Private module-dictionary shape is not the lazy-loading
contract. Add and prove the base-wheel behavioral import blocker before
replacing the proxy.

**Resolution state.** `Applied`

**Closure evidence.** Commit `e08d323f40eb38dd6ba629dcabcdb42feff075b5` first
added an isolated base-wheel import blocker covering all ten entry points.
Commit `8b1c42aa0aab02045be45c7c6c1c9dbdcc82e232` then replaced the private
dictionary assertion with that behavioral boundary. The exact-R test passed, and
a fresh eager-import mutant was killed by the replacement oracle. Closure
remains conditional on the non-squash merge described in Reconciliation.

### V7. Generic catalogue member absence is too strict - Over-specified

**Assertions.** `DIST-d9c95021451c-A0023` at `test/python/test_init.py:70`.

**Promise.** S11 promises no fixed ARN or Region defaults. S14 promises runtime
configuration for the generic record. Neither requires the `session` member to
be absent as a representation rule.

**Provenance.** Commit `6fcf75728af1850445b346d8822c390ec28e0512` co-introduced
the generic record, install/discovery path, and no-member assertion. Its body
contained AI text, which remains only a provenance signal.

**Experiment.** `T1-009` omitted A0023 without coverage delta. In paired
`T3-002R1`, both exact environments coupled an empty `session: {}` fault. The
current side failed exactly `test_installed_catalogue`; the side narrowed to
fixed-key absence passed. Results R03 and R15.

```text
command             : REPRO_IDS=T1-009,T3-002R1 bash <(awk '/^### Commands$/{s=1;next}/^#### Complete canonical dispatch matrix$/{exit}s&&/^```sh$/{c=1;next}s&&c&&/^```$/{c=0;print "";next}s&&c{print}' .agent/audits/distribution-catalog-and-python-shell.md)
campaign baseline B : d9c95021451c10614ce2b0c6348480ca96742b9c
evidence SHA E      : d9c95021451c10614ce2b0c6348480ca96742b9c
test selection       : uvx nox -s tests-3.14 -- test/python/test_init.py
expected observation : empty session is distinguished only by member absence
observed failures    : current test_installed_catalogue; narrowed none
accused assertion    : sole current oracle for empty representation
same-class oracles   : narrowed absence of base-url and custom2
coverage before      : 174 statements/34 covered; 50 branches/7 covered
coverage after       : identical in T1-009
probe-owned edit     : restored
post-command status  : clean, detached at E; both worktrees removed
```

**T3 comparison.** Complete failing sets were unequal exactly as predicted:
current `{test_installed_catalogue}`, narrowed `{}`. Baselines passed and the
installed fault artifact was coupled.

**Adjudication.** Whole-member absence over-constrains the S11 promise. The
paired result proves empty-object representation freedom only; it does not prove
every other session key harmless.

**Remedy.** Replace the no-member predicate with explicit absence of `base-url`
and `custom2`. Before applying that narrowing to production data, decide every
other supported session-key class and add an exact pinned-MQT parser/bridge
mutant for a harmful extra key such as `auth-file` or `custom1`.

**Unlock.** An empty session object is permitted by the test contract. No
non-empty generic default is authorized by this verdict.

**Risk and release impact.** High. An accepted but harmful credential,
reservation, or configuration default could change generic-device behavior. Keep
the current assertion until the replacement and harmful-key oracle land.

**Maintainer decision.** `Accepted`

**Decision reason.** The device-shipped default catalogue may use an empty
session object, but repository-owned defaults must reject dangerous values. This
decision does not constrain externally managed HPC-centre catalogues, which may
carry site-chosen sensitive values.

**Resolution state.** `Applied`

**Closure evidence.** Commit `e08d323f40eb38dd6ba629dcabcdb42feff075b5` added
catalogue-relative library validation. Commit
`454c203191dc3517e838079a0f90ac6d2656ce28` then replaced member absence with
`get("session", {}) == {}`. The device-shipped default therefore accepts an
absent or empty session and rejects every non-empty value. This repository
assertion does not constrain externally managed HPC-centre catalogues, which may
carry site-chosen sensitive values. The exact-R Python matrix passed. Closure
remains conditional on the non-squash merge described in Reconciliation.

### V8. Artifact path substrings are not consumer contracts - Over-specified

**Assertions.** `DIST-d9c95021451c-A0028` at `test/python/test_init.py:81`,
A0033 at line 95, and A0036 at line 102.

**Promise.** S3, S5, S6, S16, and S20 promise usable installed artifacts. They
deliberately permit configurable GNU destinations and platform filenames; they
do not promise the substrings `include`, `cmake`, or the project name.

**Provenance.** Commit `33d8487b12fad74ed674db08bc312d0823b5ab7b` added all
three assertions after the discovery spellings existed. No relevant human
request fixed these substrings.

**Experiment.** The authoritative coherent splits are `T2-012I1`, `T2-012C1`,
and `T2-012O4`; the paired results are `T3-003R2` for A0028 and `T3-004` for
A0036. Results R16 and R17. All intended installed artifacts were coupled;
exported-header compilation or installed consumer configure/build passed; only
the lexical test failed on current sides.

```text
command             : REPRO_IDS=T2-012I1,T2-012C1,T2-012O4,T3-003R2,T3-004 bash <(awk '/^### Commands$/{s=1;next}/^#### Complete canonical dispatch matrix$/{exit}s&&/^```sh$/{c=1;next}s&&c&&/^```$/{c=0;print "";next}s&&c{print}' .agent/audits/distribution-catalog-and-python-shell.md)
campaign baseline B : d9c95021451c10614ce2b0c6348480ca96742b9c
evidence SHA E      : d9c95021451c10614ce2b0c6348480ca96742b9c
test selection       : installed test_init.py plus header/CMake consumers
expected observation : coherent spelling changes fail only lexical predicates
observed failures    : include_dir_exists; cmake_dir_exists; library_path_exists
accused assertion    : sole selected Python oracle in each current split
same-class oracles   : semantic header compile and installed CMake consumer
coverage before      : 174 statements/34 covered; 50 branches/7 covered
coverage after       : identical in T1-012
probe-owned edit     : restored
post-command status  : clean, detached at E; temporary worktrees removed
```

**T3 comparison.** T3-003R2 and T3-004 used identical coupled faults in fresh
pairs. Current sides failed exactly the named lexical test; narrowed sides had
no failing tests. Header compilation and the load-free installed CMake consumer
passed on both sides. A0033 has the isolated T2 consumer result, not a T3 pair.

**Adjudication.** The three assertions defend real artifacts through incidental
spelling. They are Over-specified as lexical tests. The experiments do not grant
production directory or filename freedom.

**Remedy.** Replace A0028 with an exported-header compile, A0033 with an exact
documented `find_package` use of the Python locator, and A0036 with pinned MQT
catalogue parse, runtime-copy relocation, and actual native load. Until the last
replacement exists across release platform classes, remove no production name or
layout and retain A0036 if necessary as a temporary weak guard.

**Unlock.** Only the lexical test constraints are lifted. No production
relocation, OUTPUT_NAME change, wheel layout change, or locator redesign is
authorized by this audit.

**Risk and release impact.** High. Current evidence is Linux and the CMake
consumer is load-free. MQT Core runtime-copy, dynamic loading, repaired wheels,
Windows DLL behavior, macOS install names, and PennyLane construction remain
release blockers for any production layout change.

**Maintainer decision.** `Accepted`

**Decision reason.** Path spelling is not the contract. Replace lexical checks
with semantic installed consumers. Do not change production paths, filenames,
wheel layout, or locator design.

**Resolution state.** `Applied`

**Closure evidence.** Commit `e08d323f40eb38dd6ba629dcabcdb42feff075b5` added
semantic installed C-header and package-file oracles. Commit
`99d7c510689365e024f8912a72560288e96d0e7e` added a Python-locator `find_package`
consumer, direct `ctypes` symbol resolution, `mqt_copy_qdmi_runtime`, and
relocated catalogue parsing. Commit `30269f5ea8aaf6c524c583685038b2fc8129bf4d`
made the relocated native load a fresh `python -I` process with no package
import. Commit `454c203191dc3517e838079a0f90ac6d2656ce28` removed only the three
lexical assertions; production layout and names did not change. The local Linux
aarch64 exact-R consumer matrix passed. Cross-platform repaired-wheel evidence
remains CI work if required. Closure remains conditional on the non-squash merge
described in Reconciliation.

### V9. License enforcement should not freeze one author - Over-specified

**Assertions.** `DIST-d9c95021451c-A0003` at `.pre-commit-config.yaml:89-95` and
`.license-tools-config.json:2-40`.

**Promise.** S28 promises `Apache-2.0 WITH LLVM-exception` applicability and
package license metadata. It does not promise one author string on every
included core source file.

**Provenance.** The exact author, license, enforcement, and SPANK exclusions
were assembled across different commits. The license changed before the current
enforcement configuration. No rung-3 request requires forced author text.

**Experiment.** `T1-003R1` set `force_author=false` while retaining license
enforcement; all 22 hooks passed. `T2-003` injected an author-only fault, which
failed only `Check license headers`; results R01 and R02.

```text
command             : REPRO_IDS=T1-003R1,T2-003 bash <(awk '/^### Commands$/{s=1;next}/^#### Complete canonical dispatch matrix$/{exit}s&&/^```sh$/{c=1;next}s&&c&&/^```$/{c=0;print "";next}s&&c{print}' .agent/audits/distribution-catalog-and-python-shell.md)
campaign baseline B : d9c95021451c10614ce2b0c6348480ca96742b9c
evidence SHA E      : d9c95021451c10614ce2b0c6348480ca96742b9c
test selection       : uvx nox -s lint; all 22 configured hooks
expected observation : author-only variation is separable from license policy
observed failures    : T2-003 failed only Check license headers
accused assertion    : author subconstraint was sole oracle for the author fault
same-class oracles   : retained license rule, not yet fault-validated
coverage before      : static lint; not measured
coverage after       : static lint; not measured
probe-owned edit     : restored
post-command status  : clean, detached at E
```

**T3 comparison.** Not run.

**Adjudication.** The combined checker protects a real legal promise but adds an
unpromised universal-author constraint. That subconstraint is Over-specified.
The current evidence does not yet validate the retained legal oracle under the
proposed configuration.

**Remedy.** Set `force_author=false` while retaining the Apache-2.0 WITH
LLVM-exception rule, but only after an exact SPDX/license-only fault is rejected
by the narrowed hook and built wheel/sdist license metadata is inspected.

**Unlock.** Legitimate per-file authorship may vary. License applicability,
SPANK's GPL boundary, and package license metadata do not change.

**Risk and release impact.** Medium-high legal risk. The narrowing is blocked
until the retained-license fault passes; classification alone authorizes no
configuration edit.

**Maintainer decision.** `Rejected`

**Decision reason.** This package intentionally has and enforces one author.
Non-SPANK content is Apache-2.0 WITH LLVM-exception. The separately GPL-licensed
SPANK content is excluded from wheels and the Python package. Retain the current
enforcement.

**Resolution state.** `Not applicable`

**Closure evidence.** The single-author and split-license policy remains
unchanged. The rejected decision is recorded in
`e934b6249c49f63cbf0bb04d352c24c5c8fd03c3`; closure remains conditional on the
non-squash merge described in Reconciliation.

### V10. Python-floor test pins exception class and diagnostic wording - Over-specified

**Assertions.** `DIST-d9c95021451c-A0072` and A0073 at
`test/python/test_init.py:138-141`.

**Promise.** S24 requires a Python-3.10-compatible base package and a
Python-3.11 floor for the optional PennyLane integration. S25 requires all ten
installed entry-point name-to-lazy-attribute mappings. Neither promises the
`ImportError` exception domain, including subclasses, or the lexical text
`Python 3.11`.

**Provenance.** The version gate, exception, message, and mixed lazy-entry-point
test were introduced together in PR #168. The fresh provenance review found no
human request for the exception domain or message. Co-introduction does not
settle the contract.

**Experiment.** `T2-042R1` changed only version-gate `ImportError` to
`RuntimeError`; `T2-043R1` changed only `Python 3.11 or newer` to the
semantically equivalent `Python >= 3.11`. Each used a distinct installed
Python-3.10 environment and the complete offline `test/python` scope. Each
baseline had the same 19-node set with 17 passed and two skipped. Each fault
retained all 19 nodes and changed exactly
`test_pennylane_entry_point_is_lazy_on_the_base_install` from passed to failed:
16 passed, two skipped, one failed, zero errors/xfails/xpasses. Installed
digests and mutually exclusive structural markers proved coupling; result R23.

```text
command             : REPRO_IDS=T2-042R1,T2-043R1 bash <(awk '/^### Commands$/{s=1;next}/^#### Complete canonical dispatch matrix$/{exit}s&&/^```sh$/{c=1;next}s&&c&&/^```$/{c=0;print "";next}s&&c{print}' .agent/audits/distribution-catalog-and-python-shell.md)
campaign baseline B : d9c95021451c10614ce2b0c6348480ca96742b9c
evidence SHA E      : d9c95021451c10614ce2b0c6348480ca96742b9c
test selection       : supported tests-3.10 installed session; complete test/python collection and execution
expected observation : only the lazy entry-point node changes from passed to failed
observed failures    : exactly test/python/test_init.py::test_pennylane_entry_point_is_lazy_on_the_base_install
accused assertions   : sole complete-scope oracles for their exact class/text faults
same-class oracles   : none for class/text; S24 floor and S25 mapping must survive separately
coverage before      : outcome sets only; line/branch coverage not measured
coverage after       : identical 19-node set; exact one-node outcome delta
probe-owned edit     : restored
post-command status  : clean, detached at E; environments and worktrees removed
```

**T3 comparison.** Not run. The complete-scope T2 faults settle only the extra
exception-domain and lexical constraints. They do not validate a replacement
semantic gate.

**Adjudication.** The mixed test defends real S24/S25 promises but pins two
details beyond them. Both assertions are Over-specified. This does not mean any
exception is acceptable: the later missing-extra branch also raises
`ImportError`, so simply deleting `match=` or accepting arbitrary exceptions
could let a removed version gate pass accidentally.

**Remedy.** Before narrowing either assertion, add (1) an exact S25 oracle for
all ten names and complete object references and (2) sentinel implementations
for all ten targets in Python 3.10, then run a paired version-gate-bypass fault
through the complete offline Python scope. The sentinel oracle must fail because
an unsupported target becomes loadable, not because PennyLane is absent. Only
then replace class/text matching with the semantic floor oracle. If the loading
owner declares `ImportError` public, retain A0072 as that explicit anchor.

**Unlock.** No gate, exception-type, or diagnostic edit is cleared yet. After
the replacement oracles, equivalent wording and an explicitly chosen failure
domain need not be frozen by this mixed test.

**Risk and release impact.** High. `EntryPoint.load` is a real plugin boundary;
weakening the test without exact mapping and sentinel evidence can admit an
unsupported Python 3.10 adapter load or mask it with the missing-extra branch.

**Maintainer decisions.** A0072: `Rejected`. A0073: `Accepted`.

**Decision reasons.** A0072 is rejected because `ImportError` is the stable
entry-point loading failure domain at the Python floor and must remain asserted.
A0073 is accepted because diagnostic wording is not stable. Retain the
`ImportError` assertion and add the exact S25 mapping plus sentinel floor oracle
before relaxing only message matching.

**Resolution states.** A0072: `Not applicable`. A0073: `Applied`.

**Closure evidence.** A0072 remains asserted as the stable `ImportError` failure
domain; its rejected decision is recorded in
`e934b6249c49f63cbf0bb04d352c24c5c8fd03c3`. Commit
`e08d323f40eb38dd6ba629dcabcdb42feff075b5` added exact S25 mapping and a
ten-target Python-3.10 sentinel oracle. Commit
`8b1c42aa0aab02045be45c7c6c1c9dbdcc82e232` then removed only message matching.
At exact R the retained type oracle and sentinel test passed, and a fresh
version-gate-bypass mutant was killed by the sentinel resolver. Both
assertion-level dispositions remain conditionally closed on the non-squash merge
described in Reconciliation.

### V11. Live-test no-omission policy pins fatal refusal - Over-specified

**Assertion.** `DIST-d9c95021451c-A0074` at `CMakeLists.txt:63-65`.

**Promise.** S29 requires that opting into live tests makes the named target and
CTest registrations available. It does not require one fatal diagnostic or
define precedence when a parent explicitly sets ordinary tests OFF.

**Provenance.** The live option, fatal guard, target, and registrations arrived
together with PR #171. The fresh provenance review found no human request for
fatal refusal specifically. The guard nevertheless prevents a real silent-
omission failure at exact E.

**Experiment.** `T2-044` ran three fresh compile/configure-only arms. Unchanged
E rejected live ON/tests OFF. Removing only the guard configured successfully
but left the named live target and live registration markers absent. Replacing
the guard with the frozen parent-promotion candidate configured tests ON/live
ON, exposed and built the live target, compiled `test_live.cpp`, reached link,
and emitted target and `amazon-braket-live` registration markers. All discovery,
CTest, test-binary, live-environment, and AWS-action counts were zero; result
R24.

```text
command             : REPRO_IDS=T2-044 bash <(awk '/^### Commands$/{s=1;next}/^#### Complete canonical dispatch matrix$/{exit}s&&/^```sh$/{c=1;next}s&&c&&/^```$/{c=0;print "";next}s&&c{print}' .agent/audits/distribution-catalog-and-python-shell.md)
campaign baseline B : d9c95021451c10614ce2b0c6348480ca96742b9c
evidence SHA E      : d9c95021451c10614ce2b0c6348480ca96742b9c
test selection       : three configure arms; PRE_TEST; named live target build only
expected observation : reject contradiction or provide the requested target and registrations
observed failures    : silent-omission arm configured but exposed neither target nor registration
accused assertion    : fatal mechanism is stricter than the S29 invariant
same-class oracles   : target inventory, TU/link reachability, generated registration markers
coverage before      : configure/compile structure only; not measured
coverage after       : configure/compile structure only; no test execution
probe-owned edit     : restored
post-command status  : clean, detached at E; worktrees removed
```

**T3 comparison.** Not run. T2-044 separates the invariant from one mechanism;
it does not establish superproject equivalence for the promotion candidate.

**Adjudication.** The S29 no-silent-omission behavior is mandatory, while fatal
refusal is an Over-specified enforcement mechanism. The successful standalone
promotion arm is feasibility evidence, not a release-policy decision.

**Remedy.** Keep the current guard. First publish the contradictory-option
policy and run an offline top-level plus `add_subdirectory` four-tuple matrix
covering cache values, default-build inventory, ordinary/live target presence,
and generated registrations. A later mechanism may reject explicitly or expose
and link the live target without silent omission. It must not use `CACHE FORCE`
to override an explicit parent OFF choice, and it must run no test or AWS
behavior.

**Unlock.** No production edit is cleared. A consumer-safe, documented policy
may later replace fatal refusal while preserving target and registration
availability.

**Risk and release impact.** High. `CACHE FORCE` can expand a superproject's
dependencies, targets, and CTest state despite an explicit parent policy.

**Maintainer decision.** `Accepted`

**Decision reason.** Treat this as a documentation gap. Retain and document the
fatal LIVE=ON/TESTS=OFF policy and no-silent-omission behavior. `CACHE FORCE` is
forbidden.

**Resolution state.** `Applied`

**Closure evidence.** Commit `4e1beface7e11ed22663ac6949eaa0ec0b631130`
documented and retained the fatal LIVE=ON/TESTS=OFF policy; it did not use
`CACHE FORCE` or alter CMake behavior. At exact R the contradictory
configuration still failed with the documented diagnostic. Native resolver
evidence also configured the parent with tests and live tests enabled under
`PRE_TEST`, reached and linked `test_live.cpp`, and ran no discovery, CTest,
test binary, live behavior, network, or AWS action. A later Python test drift
did not affect that native policy evidence. Closure remains conditional on the
non-squash merge described in Reconciliation.

## Anchors confirmed

### H01. A0001, A0002, A0004 - metadata, lock, and typing owners

- **Promise:** S2, S17, and S24.
- **Fault:** invalid project metadata (`T2-001`), a stale lock (`T2-002`), and
  an internal annotated-list type mutation (`T2-004`).
- **Evidence:** R01 and R02. The metadata fault failed the validator and a
  repository-review hook; the stale lock failed the exact offline lock check;
  the type mutant survived the full lint suite and therefore proved no narrowing
  or redundancy.
- **Why it stays:** the first two are current machine-checked owners for
  standardized metadata and resolver state. The selected type mutant did not
  demonstrate that the configured typed-package check is over-broad. Any later
  hook deduplication needs a paired same-fault result and a named workflow
  owner.

### H02. A0005-A0009 and A0013 - installed CMake/export contract

- **Promise:** S1 and S4-S9.
- **Fault:** incompatible package version (`T2-023`), three split exported
  property mutations (`T2-024`, `T2-024P1`, `T2-024M1`), and a public-header
  compile fault (`T2-005`).
- **Evidence:** R01, R02, R06, and R09. The version fault failed installed
  consumer configure; each property guard named its fault; the header fault
  failed target builds with verification enabled and disabled.
- **Why it stays:** installed package selection, target linkage, public headers,
  and the three exported literals are published CMake/MQT contracts. T2-005 did
  not isolate global interface verification as redundant, so A0006 stays.

### H03. A0011, A0012, A0014 - supported package test boundary

- **Promise:** S2, S16-S18, S20, and S24.
- **Fault:** Python 3.14 excluded by `requires-python` (`T2-025R1`), a false
  pytest minimum (`T2-007`), and deletion of a root export (`T2-026R1`).
- **Evidence:** R02 and R06. The version range was rejected before collection;
  only minimums detected the false lower bound; deleted export failed exact
  `test_init.py` and `test_main.py` collection.
- **Why it stays:** the supported interpreter matrix, lowest-direct resolution,
  and public imports are release/consumer contracts. Healthy omission or
  ordinary-session survival does not make their unique negative oracles
  redundant.

### H04. A0016-A0019 - version type/non-emptiness and stable identities

- **Promise:** S9 and S18.
- **Fault:** list-valued version (`T2-027`), empty version (`T2-008`, `T3-001`),
  wrong Python device ID (`T2-028`), and wrong prefix (`T2-029`).
- **Evidence:** R02, R06, and R14. Named version and metadata tests killed each
  fault. A0017 remains the canonical non-empty oracle after V1.
- **Why it stays:** type, non-emptiness, device ID, and prefix are independent
  public propositions. V1 removes only the duplicate truthiness spelling.

### H05. A0020-A0022, A0024-A0025, A0045-A0046 - catalogue and plugin identity

- **Promise:** S9-S13, S20, and S25.
- **Fault:** malformed JSON, wrong root key, wrong ID, wrong ARN form, wrong
  Python prefix, and wrong installed entry-point target.
- **Evidence:** only distinct-cache coupled `T2-030R2`, `T2-031R1`, `T2-032R1`,
  and `T2-033R1` in R10, plus `T2-029` and exact-env metadata-coupled `T2-034`
  in R06/R11.
- **Why it stays:** these are the installed catalogue/plugin roster and schema
  oracles. The earlier shared-cache T2-030-T2-033 outcomes are excluded and do
  not weaken the corrected results.

### H06. A0026-A0027, A0029-A0032, A0034-A0035 - artifact existence and shape

- **Promise:** S3, S5-S7, S16, S20, and S21.
- **Fault:** coherent missing paths (`T2-010`) and a file used as the include
  root (`T2-011`).
- **Evidence:** R01-R04. Missing artifacts failed four exact installed-artifact
  tests; the wrong include shape failed `test_include_dir_exists`.
- **Why it stays:** `exists()` may be logically weaker than `is_dir()` or
  `is_file()`, but no paired same-mutant omission established each surviving
  oracle after assertion ordering changed. The current record supports no
  deletion. Consumer-oriented consolidation remains a future experiment.

### H07. A0037-A0044 - public Path type and cwd-independent identity

- **Promise:** S2, S20, and S21.
- **Fault:** four exports converted to strings (`T2-013R1`) and four exports
  converted to cwd-relative Paths (`T2-014R1`).
- **Evidence:** R07. String exports caused eight named `TypeError` failures.
  Relative exports failed the absolute-path test and an installed CMake consumer
  after cwd change.
- **Why it stays:** real Python and pinned MQT consumers use Path operations and
  durable locators. Repetition may later be consolidated, but changing the
  public representation or accepting cwd sensitivity is not unlocked.

### H08. A0048-A0049 and A0051-A0061 - CLI import, status, and values

- **Promise:** S18, S20, S22, and S23.
- **Fault:** deleted root export, nonzero parser exits, hidden option, wrong
  version, and four split wrong-value branches.
- **Evidence:** R06, R11, and R12. `T2-035` independently killed help/version
  status, `T2-036`/`T2-037` killed hidden-help/wrong-version faults, and
  amendment-10 status/value splits named each path test separately.
- **Why it stays:** each status and semantic output is a separate public CLI
  promise. The original combined T2-038 ordering limitation is preserved; only
  T2-038V1-T2-041V1 support the value rows.

### H09. A0063, A0065 - true missing-extra collection gates

- **Promise:** S24.
- **Fault:** remove the declared PennyLane extra for each exact module.
- **Evidence:** `T2-018R1` and `T2-020` in R07; paired `T3-005R3` in R18.
  Current A0063 reported the named skip, while the unconditional-import side
  reported `ModuleNotFoundError`; both were collect-only with zero execution.
- **Why it stays:** a true optional-absence path is real. The current broad
  `ImportError` may hide internal defects, but no top-level-versus-internal
  split was executed. Keep both gates pending that narrower experiment.

### H10. A0066, A0068, A0070 - required `constants.hpp` consumers

- **Promise:** S6 and S29.
- **Fault:** a public-header `#error` (`T2-022`), omission from the source unit
  (`T1-022R1`), and the paired live-target comparison (`T3-006R2`).
- **Evidence:** R08, R13, and R19. Omission failed while compiling
  `test_device_unit.cpp`. The header fault killed producer/source/live builds.
  In T3-006R2 both faults failed before `test_live.cpp` and link reachability.
- **Why it stays:** A0066/A0068 are real installed/source consumers. A0070 is
  retained under the red-team limit: equal early failure tuples do not exercise
  the live TU, and replacing provider aliases with raw values creates a second
  semantic authority. No discovery, CTest, binary, live environment, AWS action,
  or task occurred.

## Deliberately not touched

### Routed functional assertions

Native ABI/session behavior, device metadata, AWS configuration, QuantumTask,
PennyLane adapter runtime, and SPANK assertions retain their other scope owners.
This audit neither reclassifies nor remediates them.

### Production layout and library naming

V8 classifies only lexical test predicates. CMake destinations, Python locator
logic, catalogue library values, target `OUTPUT_NAME`, and wheel layout remained
unchanged. Resolution added real CMake, MQT runtime-copy, relocated parser, and
native-load consumers on Linux aarch64; it did not authorize or apply a layout
or filename change.

### Live Amazon Braket behavior

The opt-in target was configured and compiled only. No discovery, CTest, test
binary, credential lookup, AWS API, task, S3 object, or live device action was
run or authorized.

### Release configuration and external workflows

S15, S19, and S26 have no direct census assertion. The exact external reusable
workflow, release artifacts, wheel tags, sdist inclusion, v1.1.0 coherence,
released MQT Core minimum, regenerated lock, artifact attestation, and
final-release-candidate SHA remain release-review work, not assertion verdicts.

### Historical non-adjudicable attempts

The following 30 IDs are preserved only as history and support no verdict:

- T1: `T1-001`-`T1-005`, `T1-017`, `T1-019`, `T1-021`, `T1-022`;
- T2: `T2-006`, `T2-006R1`, `T2-012`, `T2-012R1`, `T2-012R2`, `T2-013`,
  `T2-014`, `T2-015`, `T2-018`, `T2-025`, `T2-026`, `T2-030R1`, `T2-012O1`,
  `T2-012O2`, `T2-012O3`; and
- T3: interrupted `T3-002`, `T3-005`, `T3-005R1`, `T3-005R2`, `T3-006`,
  `T3-006R1`.

The framework-affected shared-cache `T2-030`-`T2-033` outcomes are separately
excluded. Only distinct-cache `T2-030R2`, `T2-031R1`, `T2-032R1`, and `T2-033R1`
are admissible.

## Evidence record

### Environment

- **Campaign baseline B:** `d9c95021451c10614ce2b0c6348480ca96742b9c`
- **Evidence SHA E:** `d9c95021451c10614ce2b0c6348480ca96742b9c`
- **Evidence worktree:** detached at E; no branch or commits.
- **Authoring worktree:** `codex/audit-distribution-catalog-and-python-shell`;
  this audit file only.
- **Platform:** Linux aarch64; CMake 4.4.2; Ninja 1.13; Python 3.12.3; uv
  0.12.5. Individual nox sessions selected their recorded interpreters.
- **Build mode:** supported nox installed-package sessions, isolated CMake/Ninja
  consumers, and compile-only PRE_TEST live targets.
- **AWS mode:** offline; credential/profile/endpoint/live variables removed and
  EC2 metadata disabled.

### Commands

The result records R01-R22 were produced by frozen campaign harnesses. Those
harnesses were outside the repository and are not a reproduction dependency. The
recipes below are a canonical reconstruction from their frozen mutations,
selections, isolation rules, and oracles. They were syntax- and matrix-checked
during authoring, but were not substituted for the commands that produced the
recorded observations.

Run each fenced block from a clean checkout containing E. The helper creates a
fresh detached worktree for each repo-native probe, gives it unique cache,
scikit-build, nox, temporary, and coverage state, clears every credential/live
selector recognized by the probe, and proves cleanup. Dependency acquisition for
repository-pinned FetchContent sources is allowed before an offline compile arm
under amendment 01. Compile recipes may build the opt-in live-test target, but
no recipe invokes discovery, CTest, a test binary, AWS behavior, or live
behavior.

Concatenate the `sh` fences in this Commands section in order to form one
canonical driver. Explicitly set `REPRO_IDS` to `all` or to a comma-separated
list of admissible IDs from the dispatch matrix. Unset, empty, empty-component,
duplicate, and unknown selectors fail before any worktree is created. `all`
dispatches every supported ID exactly once.

```sh
set -euo pipefail
E=d9c95021451c10614ce2b0c6348480ca96742b9c
SUPPORTED_IDS='T1-001R1 T1-002R1 T1-003R1 T1-004R1 T1-005R1 T1-006 T1-007 T1-008 T1-009 T1-010 T1-011 T1-012 T1-013 T1-014 T1-015 T1-016 T1-017R1 T1-018 T1-019R1 T1-020 T1-021R1 T1-022R1 T1-023 T2-001 T2-002 T2-003 T2-004 T2-005 T2-006R2 T2-007 T2-008 T2-009 T2-010 T2-011 T2-012I1 T2-012C1 T2-012O4 T2-013R1 T2-014R1 T2-015R1 T2-016 T2-017 T2-018R1 T2-019 T2-020 T2-021 T2-022 T2-023 T2-024 T2-024P1 T2-024M1 T2-025R1 T2-026R1 T2-027 T2-028 T2-029 T2-030R2 T2-031R1 T2-032R1 T2-033R1 T2-034 T2-035 T2-036 T2-037 T2-038 T2-038V1 T2-039 T2-039V1 T2-040 T2-040V1 T2-041 T2-041V1 T2-042R1 T2-043R1 T2-044 T3-001 T3-002R1 T3-003R2 T3-004 T3-005R3 T3-006R2 T3-007'
declare -A SUPPORTED_SET=()
declare -A REQUESTED_SET=()
declare -A DISPATCH_COUNT=()
declare -a REQUESTED_ORDER=()
validate_selector() {
  if [[ ${REPRO_IDS+x} != x || -z $REPRO_IDS ]]; then
    printf 'REPRO_IDS must be set to all or a nonempty ID list\n' >&2
    return 64
  fi
  local id
  for id in $SUPPORTED_IDS; do
    if [[ -n ${SUPPORTED_SET[$id]+x} ]]; then
      printf 'duplicate supported ID: %s\n' "$id" >&2
      return 70
    fi
    SUPPORTED_SET[$id]=1
  done
  local -a requested=()
  if [[ $REPRO_IDS == all ]]; then
    read -r -a requested <<<"$SUPPORTED_IDS"
  else
    case "$REPRO_IDS" in
      ,*|*,|*,,*)
        printf 'REPRO_IDS contains an empty selector\n' >&2
        return 64 ;;
    esac
    IFS=, read -r -a requested <<<"$REPRO_IDS"
  fi
  for id in "${requested[@]}"; do
    if [[ -z ${SUPPORTED_SET[$id]+x} ]]; then
      printf 'unknown REPRO_IDS selector: %s\n' "$id" >&2
      return 64
    fi
    if [[ -n ${REQUESTED_SET[$id]+x} ]]; then
      printf 'duplicate REPRO_IDS selector: %s\n' "$id" >&2
      return 64
    fi
    REQUESTED_SET[$id]=1
    REQUESTED_ORDER+=("$id")
  done
}
selected() {
  local id=$1
  [[ -n ${REQUESTED_SET[$id]+x} ]] || return 1
  DISPATCH_COUNT[$id]=$(( ${DISPATCH_COUNT[$id]:-0} + 1 ))
  if (( DISPATCH_COUNT[$id] != 1 )); then
    printf 'duplicate canonical dispatch: %s\n' "$id" >&2
    return 70
  fi
  return 0
}
verify_dispatch_complete() {
  local id
  for id in "${REQUESTED_ORDER[@]}"; do
    if (( ${DISPATCH_COUNT[$id]:-0} != 1 )); then
      printf 'canonical recipe was not dispatched exactly once: %s\n' "$id" >&2
      return 70
    fi
  done
}
validate_selector
SOURCE=$(git rev-parse --show-toplevel)
test -z "$(git -C "$SOURCE" status --porcelain=v1 --untracked-files=all)"
git -C "$SOURCE" cat-file -e "$E^{commit}"
REPRO_ROOT=$(mktemp -d)
cleanup_repro() {
  original_status=$1
  cleanup_status=0
  trap - EXIT HUP INT TERM
  case "$REPRO_ROOT" in
    /tmp/*) ;;
    *) printf 'unsafe reproduction root: cleanup refused\n' >&2; exit 70 ;;
  esac
  if ! registry_before=$(git -C "$SOURCE" worktree list --porcelain); then
    cleanup_status=70
    registry_before=
  fi
  while IFS= read -r wt; do
    case "$wt" in
      "$REPRO_ROOT"/*)
        if ! git -C "$SOURCE" worktree remove --force "$wt"; then
          cleanup_status=70
        fi ;;
    esac
  done < <(sed -n 's/^worktree //p' <<<"$registry_before")
  if ! rm -rf -- "$REPRO_ROOT"; then
    cleanup_status=70
  fi
  if ! git -C "$SOURCE" worktree prune; then
    cleanup_status=70
  fi
  if ! registry_after=$(git -C "$SOURCE" worktree list --porcelain); then
    cleanup_status=70
    registry_after=
  fi
  while IFS= read -r wt; do
    case "$wt" in "$REPRO_ROOT"/*) cleanup_status=70 ;; esac
  done < <(sed -n 's/^worktree //p' <<<"$registry_after")
  if (( cleanup_status != 0 )); then
    printf 'owned reproduction cleanup failed\n' >&2
    if (( original_status == 0 )); then
      exit "$cleanup_status"
    fi
  fi
  exit "$original_status"
}
trap 'cleanup_repro $?' EXIT
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM

unset AWS_ACCESS_KEY_ID AWS_SECRET_ACCESS_KEY AWS_SESSION_TOKEN
unset AWS_SECURITY_TOKEN AWS_PROFILE AWS_DEFAULT_PROFILE AWS_CONFIG_FILE
unset AWS_SHARED_CREDENTIALS_FILE AWS_WEB_IDENTITY_TOKEN_FILE AWS_ROLE_ARN
unset AWS_ROLE_SESSION_NAME AWS_CONTAINER_CREDENTIALS_FULL_URI
unset AWS_CONTAINER_CREDENTIALS_RELATIVE_URI AWS_CONTAINER_AUTHORIZATION_TOKEN
unset AWS_CONTAINER_AUTHORIZATION_TOKEN_FILE AWS_REGION AWS_DEFAULT_REGION
unset AWS_ENDPOINT_URL AWS_ENDPOINT_URL_BRAKET AWS_ENDPOINT_URL_S3
unset AWS_ENDPOINT_URL_STS AWS_S3_BUCKET AMZN_BRAKET_TASK_RESULTS_S3_URI
unset AMAZON_BRAKET_DEVICE_ARN AMAZON_BRAKET_RESERVATION_ARN
unset AMAZON_BRAKET_QDMI_RUN_LIVE_CATALOG
unset AMAZON_BRAKET_QDMI_TEST_ALLOW_BUCKET_CREATION
unset AMAZON_BRAKET_PENNYLANE_LIVE IQM_DEVICE_ARN
touch "$REPRO_ROOT/aws-config" "$REPRO_ROOT/aws-credentials"
export AWS_CONFIG_FILE="$REPRO_ROOT/aws-config"
export AWS_SHARED_CREDENTIALS_FILE="$REPRO_ROOT/aws-credentials"
export AWS_EC2_METADATA_DISABLED=true NO_COLOR=1

probe_expected_nodes() {
  case "$1" in
    T2-008|T2-027)
      printf '%s\n' 'test/python/test_init.py::test_version_exists' ;;
    T2-009) : ;;
    T2-011)
      printf '%s\n' 'test/python/test_init.py::test_include_dir_exists' ;;
    T2-016|T2-036)
      printf '%s\n' 'test/python/test_main.py::test_cli_help[parameters-redacted]' ;;
    T2-028|T2-029)
      printf '%s\n' \
        'test/python/test_init.py::test_installed_catalogue' \
        'test/python/test_init.py::test_qdmi_device_metadata' ;;
    T2-035)
      printf '%s\n' \
        'test/python/test_main.py::test_cli_help[parameters-redacted]' \
        'test/python/test_main.py::test_cli_version[parameters-redacted]' ;;
    T2-037)
      printf '%s\n' 'test/python/test_main.py::test_cli_version[parameters-redacted]' ;;
    T2-038|T2-038V1)
      printf '%s\n' 'test/python/test_main.py::test_cli_include_dir[parameters-redacted]' ;;
    T2-039|T2-039V1)
      printf '%s\n' 'test/python/test_main.py::test_cli_cmake_dir[parameters-redacted]' ;;
    T2-040|T2-040V1)
      printf '%s\n' 'test/python/test_main.py::test_cli_lib_path[parameters-redacted]' ;;
    T2-041|T2-041V1)
      printf '%s\n' 'test/python/test_main.py::test_cli_catalog_path[parameters-redacted]' ;;
    *) return 64 ;;
  esac
}

validate_probe_output() {
  local id=$1 status=$2 log=$3
  test "$status" -eq 0
  grep -Fqx "baseline commit : $E" "$log"
  grep -Fqx 'execution       : offline' "$log"
  case "$id" in
    T1-005R1)
      grep -Fqx 'baseline suite  : pass' "$log"
      grep -Fqx 'modified suite  : pass' "$log"
      grep -Fqx 'with lines      : total=1394 covered=1132 missing=262' "$log"
      grep -Fqx 'without lines   : total=1394 covered=1132 missing=262' "$log"
      grep -Fqx 'line delta      : total=0 covered=0 missing=0' "$log"
      grep -Fqx 'with branches   : total=2308 covered=1202 missing=1106' "$log"
      grep -Fqx 'without branches: total=2308 covered=1202 missing=1106' "$log"
      grep -Fqx 'branch delta    : total=0 covered=0 missing=0' "$log" ;;
    T1-016)
      grep -Fqx 'baseline suite  : pass' "$log"
      grep -Fqx 'modified suite  : pass' "$log"
      grep -Fqx 'with statements : total=174 covered=54 missing=120' "$log"
      grep -Fqx 'without stmts   : total=174 covered=54 missing=120' "$log"
      grep -Fqx 'statement delta : total=0 covered=0 missing=0' "$log"
      grep -Fqx 'with branches   : total=50 covered=16 missing=34' "$log"
      grep -Fqx 'without branches: total=50 covered=16 missing=34' "$log"
      grep -Fqx 'branch delta    : total=0 covered=0 missing=0' "$log" ;;
    T1-006|T1-008|T1-009|T1-011|T1-013|T1-014|T1-015)
      grep -Fqx 'baseline suite  : pass' "$log"
      grep -Fqx 'modified suite  : pass' "$log"
      grep -Fqx 'with statements : total=174 covered=34 missing=140' "$log"
      grep -Fqx 'without stmts   : total=174 covered=34 missing=140' "$log"
      grep -Fqx 'statement delta : total=0 covered=0 missing=0' "$log"
      grep -Fqx 'with branches   : total=50 covered=7 missing=43' "$log"
      grep -Fqx 'without branches: total=50 covered=7 missing=43' "$log"
      grep -Fqx 'branch delta    : total=0 covered=0 missing=0' "$log" ;;
    T2-*)
      grep -Fqx 'baseline build  : pass' "$log"
      grep -Fqx 'baseline suite  : pass' "$log"
      grep -Fqx 'fault build     : pass' "$log"
      local actual="$REPRO_ROOT/$id-probe-actual.nodes"
      local expected="$REPRO_ROOT/$id-probe-expected.nodes"
      sed -n 's/^  - //p' "$log" |
        sed -E 's/\[[^]]*\]/[parameters-redacted]/g' |
        LC_ALL=C sort >"$actual"
      probe_expected_nodes "$id" | LC_ALL=C sort >"$expected"
      test "$(wc -l <"$actual")" -eq "$(LC_ALL=C sort -u "$actual" | wc -l)"
      cmp -s "$expected" "$actual"
      local failures
      failures=$(wc -l <"$expected")
      grep -Fqx "failing tests   : $failures" "$log"
      if (( failures == 0 )); then
        grep -Fqx 'fault suite     : pass' "$log"
      else
        grep -Fqx 'fault suite     : fail' "$log"
      fi ;;
    *) return 64 ;;
  esac
}

print_probe_evidence() {
  local id=$1 log=$2
  printf 'probe_result|id=%s|validated=yes\n' "$id"
  sed -n -E \
    '/^(with|without|statement delta|line delta|branch delta|fault build|fault suite|failing tests)[ :]/p; s/^  - /failed_node=/p' \
    "$log" | sed -E 's/\[[^]]*\]/[parameters-redacted]/g'
}

probe() {
  id=$1
  shift
  state="$REPRO_ROOT/$id"
  wt="$state/worktree"
  mkdir -p "$state/cache" "$state/skbuild" "$state/tmp"
  git -C "$SOURCE" worktree add --detach "$wt" "$E" >/dev/null
  test -z "$(git -C "$wt" status --porcelain=v1 --untracked-files=all)"
  test -z "$(git -C "$wt" symbolic-ref --quiet HEAD)"
  test "$(git -C "$wt" rev-parse HEAD)" = "$E"
  set +e
  (
    cd "$wt"
    export UV_CACHE_DIR="$state/cache"
    export SKBUILD_BUILD_DIR="$state/skbuild"
    export TMPDIR="$state/tmp"
    export COVERAGE_FILE="$state/coverage"
    .agent/audit-probe.sh "$@" --expected-baseline "$E"
  ) >"$state/probe.log" 2>&1
  probe_status=$?
  set -e
  validate_probe_output "$id" "$probe_status" "$state/probe.log"
  print_probe_evidence "$id" "$state/probe.log"
  test -z "$(git -C "$wt" status --porcelain=v1 --untracked-files=all)"
  test -z "$(git -C "$wt" symbolic-ref --quiet HEAD)"
  test "$(git -C "$wt" rev-parse HEAD)" = "$E"
  git -C "$SOURCE" worktree remove "$wt" >/dev/null
  git -C "$SOURCE" worktree prune
  ! git -C "$SOURCE" worktree list --porcelain |
    grep -Fqx "worktree $wt"
}

probe_id() {
  case "$1" in
    T1-005R1)
      probe "$1" t1 --lang cpp --source src --target all \
        --omit CMakeLists.txt:47-49 ;;
    T1-006)
      probe "$1" t1 --lang python --source python/amazon/braket/qdmi \
        --tests test/python/test_init.py --omit noxfile.py:99-100 \
        --nox-session tests-3.14 ;;
    T1-008)
      probe "$1" t1 --lang python --source python/amazon/braket/qdmi \
        --tests test/python/test_init.py \
        --omit test/python/test_init.py:43 --nox-session tests-3.14 ;;
    T1-009)
      probe "$1" t1 --lang python --source python/amazon/braket/qdmi \
        --tests test/python/test_init.py \
        --omit test/python/test_init.py:70 --nox-session tests-3.14 ;;
    T1-011)
      probe "$1" t1 --lang python --source python/amazon/braket/qdmi \
        --tests test/python/test_init.py \
        --omit test/python/test_init.py:80 --nox-session tests-3.14 ;;
    T1-013)
      probe "$1" t1 --lang python --source python/amazon/braket/qdmi \
        --tests test/python/test_init.py \
        --omit test/python/test_init.py:107-110 --nox-session tests-3.14 ;;
    T1-014)
      probe "$1" t1 --lang python --source python/amazon/braket/qdmi \
        --tests test/python/test_init.py \
        --omit test/python/test_init.py:115-118 --nox-session tests-3.14 ;;
    T1-015)
      probe "$1" t1 --lang python --source python/amazon/braket/qdmi \
        --tests test/python/test_init.py \
        --omit test/python/test_init.py:137 --nox-session tests-3.14 ;;
    T1-016)
      probe "$1" t1 --lang python --source python/amazon/braket/qdmi \
        --tests test/python/test_main.py \
        --omit test/python/test_main.py:40 --nox-session tests-3.14 ;;
    T2-008)
      probe "$1" t2 --lang python --tests test/python/test_init.py \
        --inject pyproject.toml:143 --with 'version = ""' \
        --nox-session tests-3.14 ;;
    T2-009)
      probe "$1" t2 --lang python --tests test/python/test_init.py \
        --inject cmake/amazon-braket-qdmi-device.qdmi.json.in:9 \
        --with $'        "enabled": true,\n        "session": {}' \
        --nox-session tests-3.14 ;;
    T2-011)
      probe "$1" t2 --lang python --tests test/python/test_init.py \
        --inject python/amazon/braket/qdmi/__init__.py:108 \
        --with $'    raise FileNotFoundError(msg)\nAMAZON_BRAKET_QDMI_INCLUDE_DIR = AMAZON_BRAKET_QDMI_LIBRARY_PATH' \
        --nox-session tests-3.14 ;;
    T2-016)
      probe "$1" t2 --lang python --tests test/python/test_main.py \
        --inject python/amazon/braket/qdmi/__main__.py:53 \
        --with '        description="Amazon Braket QDMI command-line interface.",' \
        --nox-session tests-3.14 ;;
    T2-027)
      probe "$1" t2 --lang python --tests test/python/test_init.py \
        --inject python/amazon/braket/qdmi/__init__.py:26 \
        --with '__version__ = ["1.0.1"]' --nox-session tests-3.14 ;;
    T2-028)
      probe "$1" t2 --lang python --tests test/python/test_init.py \
        --inject python/amazon/braket/qdmi/__init__.py:38 \
        --with 'AMAZON_BRAKET_QDMI_DEVICE_ID = "amazon.braket.broken"' \
        --nox-session tests-3.14 ;;
    T2-029)
      probe "$1" t2 --lang python --tests test/python/test_init.py \
        --inject python/amazon/braket/qdmi/__init__.py:39 \
        --with 'AMAZON_BRAKET_QDMI_PREFIX = "AMAZON_BRAKET_MUTANT"' \
        --nox-session tests-3.14 ;;
    T2-035)
      probe "$1" t2 --lang python --tests test/python/test_main.py \
        --inject python/amazon/braket/qdmi/__main__.py:58 \
        --with $'    parser = make_parser()\n    original_exit = parser.exit\n    parser.exit = lambda status=0, message=None: original_exit(1 if status == 0 else status, message)' \
        --nox-session tests-3.14 ;;
    T2-036)
      probe "$1" t2 --lang python --tests test/python/test_main.py \
        --inject python/amazon/braket/qdmi/__main__.py:68 \
        --with '        help=argparse.SUPPRESS,' --nox-session tests-3.14 ;;
    T2-037)
      probe "$1" t2 --lang python --tests test/python/test_main.py \
        --inject python/amazon/braket/qdmi/__main__.py:63 \
        --with '        version="0.0.0",' --nox-session tests-3.14 ;;
    T2-038)
      probe "$1" t2 --lang python --tests test/python/test_main.py \
        --inject python/amazon/braket/qdmi/__main__.py:88-89 \
        --with $'    if args.include_dir:\n        print(AMAZON_BRAKET_QDMI_CMAKE_DIR)\n        raise SystemExit(1)' \
        --nox-session tests-3.14 ;;
    T2-039)
      probe "$1" t2 --lang python --tests test/python/test_main.py \
        --inject python/amazon/braket/qdmi/__main__.py:90-91 \
        --with $'    elif args.cmake_dir:\n        print(AMAZON_BRAKET_QDMI_INCLUDE_DIR)\n        raise SystemExit(1)' \
        --nox-session tests-3.14 ;;
    T2-040)
      probe "$1" t2 --lang python --tests test/python/test_main.py \
        --inject python/amazon/braket/qdmi/__main__.py:92-93 \
        --with $'    elif args.lib_path:\n        print(AMAZON_BRAKET_QDMI_CATALOG_PATH)\n        raise SystemExit(1)' \
        --nox-session tests-3.14 ;;
    T2-041)
      probe "$1" t2 --lang python --tests test/python/test_main.py \
        --inject python/amazon/braket/qdmi/__main__.py:94-95 \
        --with $'    elif args.catalog_path:\n        print(AMAZON_BRAKET_QDMI_LIBRARY_PATH)\n        raise SystemExit(1)' \
        --nox-session tests-3.14 ;;
    T2-038V1)
      probe "$1" t2 --lang python --tests test/python/test_main.py \
        --inject python/amazon/braket/qdmi/__main__.py:88-89 \
        --with $'    if args.include_dir:\n        print(AMAZON_BRAKET_QDMI_CMAKE_DIR)' \
        --nox-session tests-3.14 ;;
    T2-039V1)
      probe "$1" t2 --lang python --tests test/python/test_main.py \
        --inject python/amazon/braket/qdmi/__main__.py:90-91 \
        --with $'    elif args.cmake_dir:\n        print(AMAZON_BRAKET_QDMI_INCLUDE_DIR)' \
        --nox-session tests-3.14 ;;
    T2-040V1)
      probe "$1" t2 --lang python --tests test/python/test_main.py \
        --inject python/amazon/braket/qdmi/__main__.py:92-93 \
        --with $'    elif args.lib_path:\n        print(AMAZON_BRAKET_QDMI_CATALOG_PATH)' \
        --nox-session tests-3.14 ;;
    T2-041V1)
      probe "$1" t2 --lang python --tests test/python/test_main.py \
        --inject python/amazon/braket/qdmi/__main__.py:94-95 \
        --with $'    elif args.catalog_path:\n        print(AMAZON_BRAKET_QDMI_LIBRARY_PATH)' \
        --nox-session tests-3.14 ;;
    *) return 64 ;;
  esac
}

# Dispatch every supported repo-native probe selected by REPRO_IDS.
for id in T1-005R1 T1-006 T1-008 T1-009 T1-011 T1-013 T1-014 \
  T1-015 T1-016 T2-008 T2-009 T2-011 T2-016 T2-027 T2-028 \
  T2-029 T2-035 T2-036 T2-037 T2-038 T2-039 T2-040 T2-041 \
  T2-038V1 T2-039V1 T2-040V1 T2-041V1; do
  selected "$id" && probe_id "$id"
done
```

`probe_id` covers the standard probe experiments cited by V1, V2, V4, V6, V7,
H04, H06, and H08. The manual and paired recipes below define the staged,
coupled, installed-consumer, minimum-dependency, collection-only, and
compile-only cases that the supported probe cannot express. The repository
probe's sanitized contract exposes build/suite state and exact failing node IDs,
but not skip/error sets. The wrapper validates every exposed field and rejects
the probe's runner/infrastructure outcomes; this audit does not claim unreported
skip/error telemetry for those standard-probe rows.

Append the following definitions to the same shell program. `manual_pair`
creates separate baseline and changed worktrees. A failed changed arm is
reported but does not prevent cleanup. `python_mode` follows `_run_tests`
directly, so the distribution is built and installed into the exact isolated
environment before the selected pytest command. `collect_mode` always supplies
`--collect-only` and records the process status; status 1 is admissible only
when the named-module skip/count tuple required by amendment 06/19 is present.

```sh
begin_arm() {
  label=$1
  ARM="$REPRO_ROOT/arm-$label"
  WT="$ARM/worktree"
  mkdir -p "$ARM/cache" "$ARM/skbuild" "$ARM/tmp" "$ARM/nox"
  git -C "$SOURCE" worktree add --detach "$WT" "$E" >/dev/null
  test -z "$(git -C "$WT" status --porcelain=v1 --untracked-files=all)"
  test -z "$(git -C "$WT" symbolic-ref --quiet HEAD)"
  test "$(git -C "$WT" rev-parse HEAD)" = "$E"
  export UV_CACHE_DIR="$ARM/cache"
  export SKBUILD_BUILD_DIR="$ARM/skbuild"
  export TMPDIR="$ARM/tmp"
  export COVERAGE_FILE="$ARM/coverage"
  cd "$WT"
}

end_arm() {
  git diff --name-only -z |
    xargs -0 -r git restore --source="$E" --worktree --staged --
  test -z "$(git status --porcelain=v1 --untracked-files=all)"
  test -z "$(git symbolic-ref --quiet HEAD)"
  test "$(git rev-parse HEAD)" = "$E"
  cd "$SOURCE"
  git -C "$SOURCE" worktree remove "$WT" >/dev/null
  git -C "$SOURCE" worktree prune
  ! git -C "$SOURCE" worktree list --porcelain |
    grep -Fqx "worktree $WT"
}

replace_exact() {
  python3 - "$1" "$2" "$3" <<'PY'
from pathlib import Path
import sys

path, old, new = Path(sys.argv[1]), sys.argv[2], sys.argv[3]
text = path.read_text(encoding="utf-8")
if text.count(old) != 1:
    raise SystemExit(f"expected exactly one mutation site in {path}")
path.write_text(text.replace(old, new), encoding="utf-8")
PY
}

replace_count() {
  python3 - "$1" "$2" "$3" "$4" <<'PY'
from pathlib import Path
import sys

path, old, new, expected = Path(sys.argv[1]), sys.argv[2], sys.argv[3], int(sys.argv[4])
text = path.read_text(encoding="utf-8")
if text.count(old) != expected:
    raise SystemExit(f"unexpected mutation-site count in {path}")
path.write_text(text.replace(old, new), encoding="utf-8")
PY
}

delete_lines() {
  python3 - "$1" "$2" "$3" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
first, last = int(sys.argv[2]), int(sys.argv[3])
lines = path.read_text(encoding="utf-8").splitlines(keepends=True)
if first < 1 or last < first or last > len(lines):
    raise SystemExit("invalid pinned-E line range")
path.write_text("".join(lines[: first - 1] + lines[last:]), encoding="utf-8")
PY
}

write_pytest_telemetry() {
  python3 - "$ARM/audit_pytest_telemetry.py" <<'PY'
from __future__ import annotations

import json
import os
from pathlib import Path

collected: list[str] = []
outcomes: dict[str, list[str]] = {
    "passed": [], "skipped": [], "failed": [], "error": [],
    "xfail": [], "xpass": [],
}
conflicts: list[str] = []

def _record(category: str, node: str) -> None:
    if not node:
        conflicts.append("empty-node")
        return
    existing = [name for name, nodes in outcomes.items() if node in nodes]
    if existing and category not in existing:
        conflicts.append(node)
        return
    if node not in outcomes[category]:
        outcomes[category].append(node)

def pytest_collection_modifyitems(items) -> None:  # type: ignore[no-untyped-def]
    collected.extend(item.nodeid for item in items)

def pytest_collectreport(report) -> None:  # type: ignore[no-untyped-def]
    if report.skipped:
        collected.append(report.nodeid)
        _record("skipped", report.nodeid)
    elif report.failed:
        collected.append(report.nodeid)
        _record("error", report.nodeid)

def pytest_runtest_logreport(report) -> None:  # type: ignore[no-untyped-def]
    if report.when == "setup":
        if report.skipped:
            _record("xfail" if hasattr(report, "wasxfail") else "skipped", report.nodeid)
        elif report.failed:
            _record("error", report.nodeid)
    elif report.when == "call":
        if report.skipped:
            _record("xfail" if hasattr(report, "wasxfail") else "skipped", report.nodeid)
        elif report.failed:
            _record("failed", report.nodeid)
        elif hasattr(report, "wasxfail"):
            _record("xpass", report.nodeid)
        else:
            _record("passed", report.nodeid)
    elif report.when == "teardown" and report.failed:
        _record("error", report.nodeid)

def pytest_sessionfinish(exitstatus: int) -> None:
    unique_collected = sorted(set(collected))
    classified = set().union(*(set(nodes) for nodes in outcomes.values()))
    result = {
        "schema": 1,
        "status": int(exitstatus),
        "selected_scope": os.environ["AUDIT_SELECTED_SCOPE"],
        "collect_only": os.environ["AUDIT_COLLECT_ONLY"] == "yes",
        "collected": unique_collected,
        "duplicates": len(collected) - len(unique_collected),
        "conflicts": sorted(set(conflicts)),
        "unknown": sorted(set(unique_collected) - classified),
        **{name: sorted(nodes) for name, nodes in outcomes.items()},
    }
    Path(os.environ["AUDIT_TELEMETRY"]).write_text(
        json.dumps(result, sort_keys=True), encoding="utf-8"
    )
PY
}

run_status() {
  LAST_LOG="$ARM/command.log"
  if "$@" >"$LAST_LOG" 2>&1; then
    LAST_STATUS=0
  else
    LAST_STATUS=$?
  fi
  return "$LAST_STATUS"
}

nox_mode() {
  session=$1
  shift
  LAST_KIND=nox
  NOX_RUN_INDEX=$(( ${NOX_RUN_INDEX:-0} + 1 ))
  LAST_TELEMETRY="$ARM/nox-$NOX_RUN_INDEX-telemetry.json"
  write_pytest_telemetry
  export AUDIT_TELEMETRY="$LAST_TELEMETRY"
  export AUDIT_SELECTED_SCOPE="$*"
  export AUDIT_COLLECT_ONLY=no
  export PYTHONPATH="$ARM"
  export PYTHONNOUSERSITE=1
  export PYTEST_ADDOPTS='-p audit_pytest_telemetry -n 0'
  run_status uvx --from 'nox==2026.8.10' nox \
    --envdir "$ARM/nox" -s "$session" -- "$@"
}

python_mode() {
  version=$1
  shift
  LAST_KIND=pytest
  LAST_LOG="$ARM/command.log"
  LAST_TELEMETRY="$ARM/pytest-telemetry.json"
  : >"$LAST_LOG"
  write_pytest_telemetry
  export PYTHONPATH="$ARM"
  export PYTHONNOUSERSITE=1
  uv venv --python "$version" "$ARM/env" || return
  py="$ARM/env/bin/python"
  test -x "$(readlink -f "$py")" || return
  export UV_PROJECT_ENVIRONMENT="$ARM/env"
  uv sync --inexact --only-group build --only-group test || return
  uv sync --inexact --no-dev \
    --no-build-isolation-package amazon-braket-qdmi || return
  if test -n "${VERIFY_KIND:-}"; then
    verify_installed "$py" "$VERIFY_KIND" "$VERIFY_PHASE" "$VERIFY_REF" || return
  fi
  selected_scope=${AUDIT_SELECTED_SCOPE_OVERRIDE:-$*}
  collect_only=no
  case " $* " in *' --collect-only '*) collect_only=yes ;; esac
  export AUDIT_TELEMETRY="$LAST_TELEMETRY"
  export AUDIT_SELECTED_SCOPE="$selected_scope"
  export AUDIT_COLLECT_ONLY="$collect_only"
  export PYTEST_ADDOPTS=-n0
  coverage_args=(--cov-config=pyproject.toml)
  if [[ ${MEASURE_PYTHON_COVERAGE:-no} == yes ]]; then
    LAST_COVERAGE="$ARM/coverage.json"
    coverage_args+=(--cov=python/amazon/braket/qdmi --cov-branch \
      "--cov-report=json:$LAST_COVERAGE")
  fi
  run_status uv run --no-sync pytest -p audit_pytest_telemetry -n 0 "$@" \
    "${coverage_args[@]}"
}

collect_mode() {
  version=$1
  module=$2
  python_mode "$version" --collect-only "$module"
}

lint_mode() {
  LAST_KIND=lint
  run_status uvx --from 'nox==2026.8.10' nox \
    --envdir "$ARM/nox" -s lint
}

assert_pytest_report() {
  report=$1 status=$2 collected_count=$3 passed_count=$4 skipped_count=$5
  failed_count=$6 error_count=$7 xfail_count=$8 xpass_count=$9
  shift 9
  expected_passed=$1 expected_skipped=$2 expected_failed=$3 expected_error=$4
  expected_unknown=$5
  python3 - "$report" "$status" "$collected_count" "$passed_count" \
    "$skipped_count" "$failed_count" "$error_count" "$xfail_count" \
    "$xpass_count" "$expected_passed" "$expected_skipped" \
    "$expected_failed" "$expected_error" "$expected_unknown" <<'PY'
import json
import os
import sys
from pathlib import Path

(path, status, collected_count, passed_count, skipped_count, failed_count,
 error_count, xfail_count, xpass_count, *expected) = sys.argv[1:]
data = json.loads(Path(path).read_text(encoding="utf-8"))
assert data["schema"] == 1
assert data["status"] == int(status)
assert data["selected_scope"] == os.environ["AUDIT_SELECTED_SCOPE"]
assert data["collect_only"] == (os.environ["AUDIT_COLLECT_ONLY"] == "yes")
assert data["duplicates"] == 0
assert data["conflicts"] == []
categories = ("passed", "skipped", "failed", "error", "xfail", "xpass", "unknown")
for category in categories:
    assert len(data[category]) == len(set(data[category]))
all_nodes = [node for category in categories for node in data[category]]
assert len(all_nodes) == len(set(all_nodes))
assert set(all_nodes) == set(data["collected"])
counts = (collected_count, passed_count, skipped_count, failed_count,
          error_count, xfail_count, xpass_count)
values = (len(data["collected"]), len(data["passed"]), len(data["skipped"]),
          len(data["failed"]), len(data["error"]), len(data["xfail"]),
          len(data["xpass"]))
assert values == tuple(map(int, counts))
for category, encoded in zip(("passed", "skipped", "failed", "error", "unknown"), expected):
    wanted = sorted(item for item in encoded.split("\n") if item)
    assert data[category] == wanted
PY
}

assert_python_coverage() {
  python3 - "$LAST_COVERAGE" <<'PY'
import json
import sys
from pathlib import Path

totals = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))["totals"]
actual = (
    totals["num_statements"], totals["covered_lines"], totals["missing_lines"],
    totals["num_branches"], totals["covered_branches"], totals["missing_branches"],
)
assert actual == (174, 34, 140, 50, 7, 43)
PY
}

validate_nox_pass() {
  id=$1 phase=$2 status=$3
  test "$status" -eq 0
  assert_pytest_report "$LAST_TELEMETRY" 0 10 10 0 0 0 0 0 \
    "$(init_nodes)" '' '' '' ''
  printf 'nox_result|id=%s|phase=%s|status=0|passed=10|nodes=exact\n' \
    "$id" "$phase"
}

init_nodes() {
  printf '%s\n' \
    'test/python/test_init.py::test_cmake_dir_exists' \
    'test/python/test_init.py::test_include_dir_exists' \
    'test/python/test_init.py::test_include_dir_has_amazon_braket_qdmi_headers' \
    'test/python/test_init.py::test_installed_catalogue' \
    'test/python/test_init.py::test_library_path_exists' \
    'test/python/test_init.py::test_paths_are_absolute' \
    'test/python/test_init.py::test_paths_are_pathlib_objects' \
    'test/python/test_init.py::test_pennylane_entry_point_is_lazy_on_the_base_install' \
    'test/python/test_init.py::test_qdmi_device_metadata' \
    'test/python/test_init.py::test_version_exists'
}

pennylane_nodes() {
  printf '%s\n' \
    'test/python/test_pennylane.py::test_configures_explicit_qdmi_parameters' \
    'test/python/test_pennylane.py::test_uses_native_configuration_fallbacks' \
    'test/python/test_pennylane.py::test_rejects_invalid_configuration[-destination0-device_arn]' \
    'test/python/test_pennylane.py::test_rejects_invalid_configuration[arn:device-destination1-bucket]' \
    'test/python/test_pennylane.py::test_rejects_invalid_configuration[arn:device-destination2-bucket]' \
    'test/python/test_pennylane.py::test_rejects_invalid_configuration[arn:device-destination3-bucket]' \
    'test/python/test_pennylane.py::test_rejects_invalid_configuration[arn:device-destination4-bucket]' \
    'test/python/test_pennylane.py::test_rejects_invalid_configuration[arn:device-destination5-bucket]' \
    'test/python/test_pennylane.py::test_rejects_invalid_configuration[arn:device-destination6-prefix]' \
    'test/python/test_pennylane.py::test_rejects_invalid_configuration[arn:device-destination7-prefix]' \
    'test/python/test_pennylane.py::test_stable_pennylane_entry_point' \
    'test/python/test_pennylane.py::test_catalogue_pennylane_entry_points[amazon.braket.aqt.ibex-q1-AmazonBraketAqtIbexQ1Device]' \
    'test/python/test_pennylane.py::test_catalogue_pennylane_entry_points[amazon.braket.ionq.forte-1-AmazonBraketIonQForte1Device]' \
    'test/python/test_pennylane.py::test_catalogue_pennylane_entry_points[amazon.braket.ionq.forte-enterprise-1-AmazonBraketIonQForteEnterprise1Device]' \
    'test/python/test_pennylane.py::test_catalogue_pennylane_entry_points[amazon.braket.iqm.garnet-AmazonBraketIQMGarnetDevice]' \
    'test/python/test_pennylane.py::test_catalogue_pennylane_entry_points[amazon.braket.iqm.emerald-AmazonBraketIQMEmeraldDevice]' \
    'test/python/test_pennylane.py::test_catalogue_pennylane_entry_points[amazon.braket.rigetti.ankaa-3-AmazonBraketRigettiAnkaa3Device]' \
    'test/python/test_pennylane.py::test_catalogue_pennylane_entry_points[amazon.braket.rigetti.cepheus-1-108q-AmazonBraketRigettiCepheus1108QDevice]' \
    'test/python/test_pennylane.py::test_catalogue_pennylane_entry_points[amazon.braket.sv1-AmazonBraketSV1Device]' \
    'test/python/test_pennylane.py::test_catalogue_pennylane_entry_points[amazon.braket.dm1-AmazonBraketDM1Device]' \
    'test/python/test_pennylane.py::test_overrides_catalogue_configuration' \
    'test/python/test_pennylane.py::test_registers_packaged_qdmi_catalogue' \
    'test/python/test_pennylane.py::test_reads_catalogue_session_defaults'
}

qaoa_nodes() {
  printf '%s\n' \
    'test/python/test_pennylane_qaoa.py::test_qaoa_end_to_end_on_ddsim' \
    'test/python/test_pennylane_qaoa.py::test_qaoa_cost_capped_on_sv1'
}

main_nodes() {
  printf '%s\n' \
    'test/python/test_main.py::test_cli_catalog_path[inprocess]' \
    'test/python/test_main.py::test_cli_cmake_dir[inprocess]' \
    'test/python/test_main.py::test_cli_help[inprocess]' \
    'test/python/test_main.py::test_cli_include_dir[inprocess]' \
    'test/python/test_main.py::test_cli_lib_path[inprocess]' \
    'test/python/test_main.py::test_cli_version[inprocess]'
}

mqt_core_nodes() {
  printf '%s\n' 'test/python/test_mqt_core.py::test_register_device_if_absent'
}

python_nodes() {
  init_nodes
  main_nodes
  mqt_core_nodes
  pennylane_nodes
  qaoa_nodes
}

subtract_nodes() {
  excluded=$1
  while IFS= read -r node; do
    grep -Fqx "$node" <<<"$excluded" || printf '%s\n' "$node"
  done
}

t2_010_failures() {
  printf '%s\n' \
    'test/python/test_init.py::test_cmake_dir_exists' \
    'test/python/test_init.py::test_include_dir_exists' \
    'test/python/test_init.py::test_include_dir_has_amazon_braket_qdmi_headers' \
    'test/python/test_init.py::test_library_path_exists'
}
t2_013_failures() {
  printf '%s\n' \
    'test/python/test_init.py::test_cmake_dir_exists' \
    'test/python/test_init.py::test_include_dir_exists' \
    'test/python/test_init.py::test_include_dir_has_amazon_braket_qdmi_headers' \
    'test/python/test_init.py::test_installed_catalogue' \
    'test/python/test_init.py::test_library_path_exists' \
    'test/python/test_init.py::test_paths_are_absolute' \
    'test/python/test_init.py::test_paths_are_pathlib_objects' \
    'test/python/test_init.py::test_pennylane_entry_point_is_lazy_on_the_base_install'
}
t2_014_failures() {
  printf '%s\n' 'test/python/test_init.py::test_paths_are_absolute'
}
t2_015_failures() {
  printf '%s\n' \
    'test/python/test_init.py::test_pennylane_entry_point_is_lazy_on_the_base_install'
}
t2_010_passes() { init_nodes | subtract_nodes "$(t2_010_failures)"; }
t2_013_passes() { init_nodes | subtract_nodes "$(t2_013_failures)"; }
t2_014_passes() { init_nodes | subtract_nodes "$(t2_014_failures)"; }
t2_015_passes() { init_nodes | subtract_nodes "$(t2_015_failures)"; }
python_baseline_passes() {
  python_nodes | subtract_nodes \
    'test/python/test_pennylane_qaoa.py::test_qaoa_cost_capped_on_sv1'
}
t2_026_fault_unknown() {
  mqt_core_nodes
  pennylane_nodes
  qaoa_nodes
}

validate_lint_outcome() {
  id=$1 phase=$2 status=$3 log=$4
  parsed="$ARM/lint-outcomes"
  sed -n -E 's/^(.*[^.])\.{2,}(Passed|Failed)$/\2|\1/p' "$log" >"$parsed"
  case "$id:$phase" in
    T1-001R1:changed|T1-002R1:changed)
      test "$status" -eq 0
      test "$(grep -Fc 'Passed|' "$parsed")" -eq 21
      test "$(grep -Fc 'Failed|' "$parsed")" -eq 0 ;;
    T2-001:changed)
      test "$status" -eq 1
      test "$(grep -Fc 'Passed|' "$parsed")" -eq 20
      test "$(grep -Fc 'Failed|' "$parsed")" -eq 2
      grep -Fq 'Validate pyproject.toml' "$parsed"
      grep -Fq 'Scientific Python repo-review' "$parsed" || grep -Fq 'sp-repo-review' "$parsed" ;;
    T2-002:changed)
      test "$status" -eq 1
      test "$(grep -Fc 'Passed|' "$parsed")" -eq 21
      test "$(grep -Fc 'Failed|' "$parsed")" -eq 1
      grep -Fq 'uv-lock' "$log" ;;
    T2-003:changed)
      test "$status" -eq 1
      test "$(grep -Fc 'Passed|' "$parsed")" -eq 21
      test "$(grep -Fc 'Failed|' "$parsed")" -eq 1
      grep -Fq 'Check license headers' "$parsed" ;;
    *)
      test "$status" -eq 0
      test "$(grep -Fc 'Passed|' "$parsed")" -eq 22
      test "$(grep -Fc 'Failed|' "$parsed")" -eq 0 ;;
  esac
  printf 'lint_result|id=%s|phase=%s|status=%s|passed=%s|failed=%s\n' \
    "$id" "$phase" "$status" \
    "$(grep -Fc 'Passed|' "$parsed")" "$(grep -Fc 'Failed|' "$parsed")"
}

manual_pair() {
  id=$1
  mode=$2
  mutation=$3
  begin_arm "$id-baseline"
  set +e
  "$mode" baseline
  baseline_status=$?
  set -e
  validate_baseline "$id" "$baseline_status"
  end_arm
  begin_arm "$id-changed"
  eval "$mutation"
  set +e
  "$mode" changed
  changed_status=$?
  set -e
  validate_changed "$id" "$changed_status" "$LAST_LOG"
  end_arm
}

validate_baseline() {
  id=$1 status=$2
  case "${LAST_KIND:-custom}" in
    lint) validate_lint_outcome "$id" baseline "$status" "$LAST_LOG" ;;
    pytest)
      case "$id" in
        T1-017R1|T2-017)
          assert_pytest_report "$LAST_TELEMETRY" 5 1 0 1 0 0 0 0 \
            '' 'test/python/test_pennylane.py' '' '' '' ;;
        T1-019R1|T2-019)
          assert_pytest_report "$LAST_TELEMETRY" 5 1 0 1 0 0 0 0 \
            '' 'test/python/test_pennylane_qaoa.py' '' '' '' ;;
        T1-018|T2-018R1)
          assert_pytest_report "$LAST_TELEMETRY" 0 23 0 0 0 0 0 0 \
            '' '' '' '' "$(pennylane_nodes)" ;;
        T1-020|T2-020)
          assert_pytest_report "$LAST_TELEMETRY" 0 2 0 0 0 0 0 0 \
            '' '' '' '' "$(qaoa_nodes)" ;;
        T2-026R1)
          assert_pytest_report "$LAST_TELEMETRY" 0 42 41 1 0 0 0 0 \
            "$(python_baseline_passes)" \
            'test/python/test_pennylane_qaoa.py::test_qaoa_cost_capped_on_sv1' \
            '' '' '' ;;
        *)
          assert_pytest_report "$LAST_TELEMETRY" 0 10 10 0 0 0 0 0 \
            "$(init_nodes)" '' '' '' '' ;;
      esac
      case "$id" in T1-010|T1-012) assert_python_coverage ;; esac
      test "$status" -eq "$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1], encoding="utf-8"))["status"])' "$LAST_TELEMETRY")"
      printf 'pytest_result|id=%s|phase=baseline|validated=yes\n' "$id" ;;
    nox)
      validate_nox_pass "$id" baseline "$status" ;;
    t2_006)
      test "$status" -eq 0
      grep -Eq '(^|[^0-9])10 passed(,| in)' "$LAST_LOG"
      ! grep -Eq '(^|[^0-9])[1-9][0-9]* (failed|skipped|errors?)(,| in)' \
        "$LAST_LOG" ;;
    custom) test "$status" -eq 0 ;;
    *) return 64 ;;
  esac
}

validate_changed() {
  id=$1 status=$2 log=$3
  if [[ ${LAST_KIND:-custom} == lint ]]; then
    validate_lint_outcome "$id" changed "$status" "$log"
    return
  fi
  if [[ ${LAST_KIND:-custom} == pytest ]]; then
    case "$id" in
      T1-017R1|T2-017)
        assert_pytest_report "$LAST_TELEMETRY" 5 1 0 1 0 0 0 0 \
          '' 'test/python/test_pennylane.py' '' '' '' ;;
      T1-019R1|T2-019)
        assert_pytest_report "$LAST_TELEMETRY" 5 1 0 1 0 0 0 0 \
          '' 'test/python/test_pennylane_qaoa.py' '' '' '' ;;
      T1-018)
        assert_pytest_report "$LAST_TELEMETRY" 0 23 0 0 0 0 0 0 \
          '' '' '' '' "$(pennylane_nodes)" ;;
      T1-020)
        assert_pytest_report "$LAST_TELEMETRY" 0 2 0 0 0 0 0 0 \
          '' '' '' '' "$(qaoa_nodes)" ;;
      T2-018R1)
        assert_pytest_report "$LAST_TELEMETRY" 5 1 0 1 0 0 0 0 \
          '' 'test/python/test_pennylane.py' '' '' '' ;;
      T2-020)
        assert_pytest_report "$LAST_TELEMETRY" 5 1 0 1 0 0 0 0 \
          '' 'test/python/test_pennylane_qaoa.py' '' '' '' ;;
      T2-010)
        assert_pytest_report "$LAST_TELEMETRY" 1 10 6 0 4 0 0 0 \
          "$(t2_010_passes)" '' "$(t2_010_failures)" \
          '' '' ;;
      T2-013R1)
        assert_pytest_report "$LAST_TELEMETRY" 1 10 2 0 8 0 0 0 \
          "$(t2_013_passes)" '' "$(t2_013_failures)" \
          '' '' ;;
      T2-014R1)
        assert_pytest_report "$LAST_TELEMETRY" 1 10 9 0 1 0 0 0 \
          "$(t2_014_passes)" '' "$(t2_014_failures)" '' '' ;;
      T2-015R1)
        assert_pytest_report "$LAST_TELEMETRY" 1 10 9 0 1 0 0 0 \
          "$(t2_015_passes)" '' "$(t2_015_failures)" '' '' ;;
      T2-026R1)
        assert_pytest_report "$LAST_TELEMETRY" 2 28 0 0 0 2 0 0 '' '' '' \
          $'test/python/test_init.py\ntest/python/test_main.py' \
          "$(t2_026_fault_unknown)"
        grep -Fq 'ImportError' "$log" ;;
      T1-*)
        assert_pytest_report "$LAST_TELEMETRY" 0 10 10 0 0 0 0 0 \
          "$(init_nodes)" '' '' '' ''
        case "$id" in T1-010|T1-012) assert_python_coverage ;; esac ;;
      *) return 64 ;;
    esac
    test "$status" -eq "$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1], encoding="utf-8"))["status"])' "$LAST_TELEMETRY")"
    printf 'pytest_result|id=%s|phase=changed|validated=yes\n' "$id"
    return
  fi
  if [[ ${LAST_KIND:-custom} == nox ]]; then
    case "$id" in
      T1-007) validate_nox_pass "$id" changed "$status" ;;
      *) return 64 ;;
    esac
    return
  fi
  if [[ ${LAST_KIND:-custom} == t2_006 ]]; then
    test "$id" = T2-006R2
    test "$status" -eq 1
    grep -Fq 'FileNotFoundError' "$log"
    grep -Eq '(^|[^0-9])20 errors?([^0-9]|$)' "$log"
    ! grep -Eq '(^|[^0-9])[1-9][0-9]* (failed|skipped)(,| in)' "$log"
    actual_error_nodes=$(sed -n -E 's/^ERROR ([^ ]+).*/\1/p' "$log" |
      LC_ALL=C sort -u)
    test "$actual_error_nodes" = 'test/python/test_init.py'
    printf 'pytest_result|id=T2-006R2|phase=changed|status=1|errors=20|error_nodes=exact\n'
    return
  fi
  case "$id" in
    T1-*|T2-004)
      test "$status" -eq 0 ;;
    T2-025R1)
      test "$status" -eq 1
      grep -Eqi 'requires-python|Python.*incompatib' "$log" ;;
    T2-005|T2-021|T2-022)
      test "$status" -eq 0 ;;
    *)
      printf 'no changed-arm oracle for %s\n' "$id" >&2
      return 64 ;;
  esac
}
```

The exact reusable installed-artifact verifier follows. It prints only boolean
or count fields and digests equality, never paths, filenames, catalogue
contents, environment values, or resource identifiers. For `catalogue-*`,
`entrypoint`, and relocation cases it runs after installation and before the
selected pytest module. Baseline references are held outside the worktree.

```sh
verify_installed() {
  "$1" - "$2" "$3" "$4" <<'PY'
from __future__ import annotations

import hashlib
import json
import os
import sys
from importlib.metadata import distribution
from pathlib import Path

kind, phase, reference_arg = sys.argv[1:]
reference = Path(reference_arg)
dist = distribution("amazon-braket-qdmi")
environment = Path(sys.prefix).resolve()
environment_match = Path(str(dist._path)).resolve().is_relative_to(environment)
from amazon.braket.qdmi import (  # noqa: E402
    AMAZON_BRAKET_QDMI_CATALOG_PATH as catalogue,
    AMAZON_BRAKET_QDMI_CMAKE_DIR as cmake_dir,
    AMAZON_BRAKET_QDMI_INCLUDE_DIR as include_dir,
    AMAZON_BRAKET_QDMI_LIBRARY_PATH as library,
    __version__,
)

catalogue = catalogue.resolve()
include_dir = include_dir.resolve()
cmake_dir = cmake_dir.resolve()
library = library.resolve()
data = include_dir.parent
digest = hashlib.sha256()
for artifact in (catalogue, library):
    digest.update(artifact.read_bytes())
token = "baseline"
valid = all(path.exists() for path in (catalogue, include_dir, cmake_dir, library))

if kind == "version-empty":
    token = "empty" if __version__ == "" else "nonempty"
elif kind.startswith("catalogue-"):
    raw = catalogue.read_bytes()
    try:
        parsed = json.loads(raw)
        json_valid = True
    except json.JSONDecodeError:
        parsed = {}
        json_valid = False
    if kind == "catalogue-malformed":
        token = "valid" if json_valid else "invalid"
    elif kind == "catalogue-root":
        token = "mutant" if "qdmi-mutant" in parsed else "original"
    elif kind == "catalogue-id":
        token = "mutant" if b"ibex-q1-mutant" in raw else "original"
    elif kind == "catalogue-url":
        token = "mutant" if b"example.invalid" in raw else "original"
    elif kind == "catalogue-session":
        generic = parsed["qdmi"]["devices"][0]
        token = "empty-session" if generic.get("session") == {} else "absent"
elif kind == "entrypoint":
    values = [ep.value for ep in dist.entry_points if ep.group == "pennylane.plugins"]
    token = "mutant" if any("_wrong_entrypoint" in value for value in values) else "original"
elif kind == "include":
    token = include_dir.name
    valid = valid and (include_dir / "amazon_braket_qdmi").is_dir()
elif kind == "cmake":
    token = cmake_dir.name
    valid = valid and any(cmake_dir.rglob("*-config.cmake"))
elif kind == "output":
    parsed = json.loads(catalogue.read_text(encoding="utf-8"))
    values = []
    stack = [parsed]
    while stack:
        value = stack.pop()
        if isinstance(value, dict):
            values.extend(v for k, v in value.items() if k == "library" and isinstance(v, str))
            stack.extend(value.values())
        elif isinstance(value, list):
            stack.extend(value)
    candidates = [catalogue.parent / value for value in values]
    adjacent = bool(candidates) and all(p.parent.resolve() == catalogue.parent for p in candidates)
    existing = adjacent and all(p.is_file() for p in candidates)
    same = existing and all(
        os.path.samefile(p, library)
        or hashlib.sha256(p.read_bytes()).digest()
        == hashlib.sha256(library.read_bytes()).digest()
        for p in candidates
    )
    token = "mutant" if "braket-qdmi-provider" in library.name else "original"
    valid = valid and adjacent and existing and same and library.is_relative_to(data)

record = {"digest": digest.hexdigest(), "token": token, "valid": valid}
if phase == "baseline":
    reference.write_text(json.dumps(record), encoding="utf-8")
    difference = True
else:
    baseline = json.loads(reference.read_text(encoding="utf-8"))
    difference = token != baseline["token"]
    if kind == "include":
        difference = difference and digest.hexdigest() == baseline["digest"]
    elif kind not in {"cmake", "version-empty", "entrypoint"}:
        difference = difference and digest.hexdigest() != baseline["digest"]

coupled = environment_match and valid and difference
print(
    "coupling"
    f"|kind={kind}|phase={phase}"
    f"|environment_match={'yes' if environment_match else 'no'}"
    f"|artifact_valid={'yes' if valid else 'no'}"
    f"|planned_difference={'yes' if difference else 'no'}"
    f"|coupled={'yes' if coupled else 'no'}"
)
raise SystemExit(0 if coupled else 3)
PY
}
```

These mode functions and calls are the complete canonical recipes for the
remaining non-coupled Python/lint evidence cited by V1-V9 and H01-H10. Each
`manual_pair` invocation runs the unmodified baseline first and then the exact
pinned-E mutation. The T1 reason-text replacements retain the gates. The T2
missing-extra replacements change only the declared test dependency.

```sh
mode_lint() { lint_mode; }
mode_init() { python_mode 3.14 test/python/test_init.py; }
mode_t2_006() {
  LAST_KIND=t2_006
  run_status env -u AUDIT_TELEMETRY -u AUDIT_SELECTED_SCOPE \
    -u AUDIT_COLLECT_ONLY -u PYTEST_ADDOPTS -u PYTHONPATH \
    uvx --from 'nox==2026.8.10' nox --envdir "$ARM/nox" \
    -s tests-3.14 -- test/python/test_init.py
}
mode_init_coverage() {
  MEASURE_PYTHON_COVERAGE=yes python_mode 3.14 test/python/test_init.py
}
mode_python() { python_mode 3.14 test/python; }
mode_minimums() { nox_mode minimums-3.14 test/python/test_init.py; }
mode_pl310() { collect_mode 3.10 test/python/test_pennylane.py; }
mode_qaoa310() { collect_mode 3.10 test/python/test_pennylane_qaoa.py; }
mode_pl311() { collect_mode 3.11 test/python/test_pennylane.py; }
mode_qaoa311() { collect_mode 3.11 test/python/test_pennylane_qaoa.py; }

selected T1-003R1 && manual_pair T1-003R1 mode_lint \
  'replace_exact .license-tools-config.json '"'"'"force_author": true'"'"' '"'"'"force_author": false'"'"''
selected T2-001 && manual_pair T2-001 mode_lint \
  'replace_exact pyproject.toml '"'"'minversion = "9.0"'"'"' '"'"'minversion = { bad = true }'"'"''
selected T2-003 && manual_pair T2-003 mode_lint \
  'replace_exact python/amazon/braket/qdmi/__init__.py '"'"'# Copyright (c) 2025 - 2026 Munich Quantum Software Company GmbH'"'"' '"'"'# Copyright (c) 2025 - 2026 Example Corporation'"'"''
selected T2-004 && manual_pair T2-004 mode_lint \
  'replace_exact noxfile.py '"'"'PYTHON_ALL_VERSIONS = ["3.10", "3.11", "3.12", "3.13", "3.14"]'"'"' '"'"'PYTHON_ALL_VERSIONS: list[int] = ["3.10", "3.11", "3.12", "3.13", "3.14"]'"'"''
selected T2-006R2 && manual_pair T2-006R2 mode_t2_006 \
  'replace_exact pyproject.toml '"'"'wheel.install-dir = "amazon/braket/qdmi/data"'"'"' '"'"'wheel.install-dir = "amazon/braket/qdmi/broken-data"'"'"''
selected T2-015R1 && manual_pair T2-015R1 mode_init \
  'replace_exact python/amazon/braket/qdmi/_pennylane_entrypoint.py '"'"'from typing import TYPE_CHECKING'"'"' $'"'"'from typing import TYPE_CHECKING\n\npennylane = None'"'"''
selected T1-017R1 && manual_pair T1-017R1 mode_pl310 \
  'replace_exact test/python/test_pennylane.py '"'"'PennyLane requires Python 3.11 or newer.'"'"' '"'"'unsupported optional integration'"'"''
selected T1-019R1 && manual_pair T1-019R1 mode_qaoa310 \
  'replace_exact test/python/test_pennylane_qaoa.py '"'"'PennyLane requires Python 3.11 or newer.'"'"' '"'"'unsupported optional integration'"'"''
selected T2-017 && manual_pair T2-017 mode_pl310 \
  'replace_exact pyproject.toml '"'"'python_version >= '\''3.11'\'''"'"' '"'"'python_version >= '\''3.10'\'''"'"''
selected T2-019 && manual_pair T2-019 mode_qaoa310 \
  'replace_exact pyproject.toml '"'"'python_version >= '\''3.11'\'''"'"' '"'"'python_version >= '\''3.10'\'''"'"''
selected T2-018R1 && manual_pair T2-018R1 mode_pl311 \
  'replace_exact pyproject.toml '"'"'"mqt-core[pennylane] @ git+https://github.com/munich-quantum-toolkit/core.git@0fe651210c52dbcf67e49e567ef67e1c9a33d809",'"'"' '"'"'"mqt-core @ git+https://github.com/munich-quantum-toolkit/core.git@0fe651210c52dbcf67e49e567ef67e1c9a33d809",'"'"''
selected T2-020 && manual_pair T2-020 mode_qaoa311 \
  'replace_exact pyproject.toml '"'"'"mqt-core[pennylane] @ git+https://github.com/munich-quantum-toolkit/core.git@0fe651210c52dbcf67e49e567ef67e1c9a33d809",'"'"' '"'"'"mqt-core @ git+https://github.com/munich-quantum-toolkit/core.git@0fe651210c52dbcf67e49e567ef67e1c9a33d809",'"'"''
selected T2-026R1 && manual_pair T2-026R1 mode_python \
  'replace_exact python/amazon/braket/qdmi/__init__.py '"'"'del dist, located_include_dir, resolved_include_dir'"'"' $'"'"'del dist, located_include_dir, resolved_include_dir\ndel AMAZON_BRAKET_QDMI_CMAKE_DIR'"'"''

fault_missing_paths() {
  replace_exact python/amazon/braket/qdmi/__init__.py \
    'AMAZON_BRAKET_QDMI_LIBRARY_PATH = min(library_files, key=lambda p: len(p.name))' \
    'AMAZON_BRAKET_QDMI_LIBRARY_PATH = _AMAZON_BRAKET_QDMI_LIBRARY_DIR / "missing-amazon-braket-qdmi-device"'
  replace_exact python/amazon/braket/qdmi/__init__.py \
    $'AMAZON_BRAKET_QDMI_INCLUDE_DIR = _AMAZON_BRAKET_QDMI_DATA / "include"\nif not AMAZON_BRAKET_QDMI_INCLUDE_DIR.exists():\n    msg = f"AMAZON_BRAKET_QDMI_INCLUDE_DIR does not exist: {AMAZON_BRAKET_QDMI_INCLUDE_DIR}"\n    raise FileNotFoundError(msg)' \
    'AMAZON_BRAKET_QDMI_INCLUDE_DIR = _AMAZON_BRAKET_QDMI_DATA / "missing-include"'
  replace_exact python/amazon/braket/qdmi/__init__.py \
    $'AMAZON_BRAKET_QDMI_CMAKE_DIR = _AMAZON_BRAKET_QDMI_DATA / "share" / "cmake"\nif not AMAZON_BRAKET_QDMI_CMAKE_DIR.exists():\n    msg = f"AMAZON_BRAKET_QDMI_CMAKE_DIR does not exist: {AMAZON_BRAKET_QDMI_CMAKE_DIR}"\n    raise FileNotFoundError(msg)' \
    'AMAZON_BRAKET_QDMI_CMAKE_DIR = _AMAZON_BRAKET_QDMI_DATA / "share" / "missing-cmake"'
}
selected T2-010 && manual_pair T2-010 mode_init fault_missing_paths

fault_string_paths() {
  replace_exact python/amazon/braket/qdmi/__init__.py \
    'del dist, located_include_dir, resolved_include_dir' \
    $'AMAZON_BRAKET_QDMI_INCLUDE_DIR = str(AMAZON_BRAKET_QDMI_INCLUDE_DIR)\nAMAZON_BRAKET_QDMI_CMAKE_DIR = str(AMAZON_BRAKET_QDMI_CMAKE_DIR)\nAMAZON_BRAKET_QDMI_LIBRARY_PATH = str(AMAZON_BRAKET_QDMI_LIBRARY_PATH)\nAMAZON_BRAKET_QDMI_CATALOG_PATH = str(AMAZON_BRAKET_QDMI_CATALOG_PATH)\n\ndel dist, located_include_dir, resolved_include_dir'
}
selected T2-013R1 && manual_pair T2-013R1 mode_init fault_string_paths

if selected T2-025R1; then
  begin_arm T2-025R1-baseline
  python_mode 3.14 test/python/test_init.py
  assert_pytest_report "$LAST_TELEMETRY" 0 10 10 0 0 0 0 0 \
    "$(init_nodes)" '' '' '' ''
  end_arm
  begin_arm T2-025R1-fault
  replace_exact pyproject.toml 'requires-python = ">=3.10"' \
    'requires-python = ">=3.10,<3.14"'
  uv build --wheel --out-dir "$ARM/dist"
  set +e
  nox_mode tests-3.14 test/python/test_init.py
  status=$?
  set -e
  test "$status" -eq 1
  test ! -e "$LAST_TELEMETRY"
  grep -Eqi 'requires-python|Python.*incompatib' "$LAST_LOG"
  end_arm
fi

fault_relative_paths() {
  replace_exact python/amazon/braket/qdmi/__init__.py 'import sys' $'import os\nimport sys'
  replace_exact python/amazon/braket/qdmi/__init__.py \
    'del dist, located_include_dir, resolved_include_dir' \
    $'AMAZON_BRAKET_QDMI_INCLUDE_DIR = Path(os.path.relpath(AMAZON_BRAKET_QDMI_INCLUDE_DIR, start=Path.cwd()))\nAMAZON_BRAKET_QDMI_CMAKE_DIR = Path(os.path.relpath(AMAZON_BRAKET_QDMI_CMAKE_DIR, start=Path.cwd()))\nAMAZON_BRAKET_QDMI_LIBRARY_PATH = Path(os.path.relpath(AMAZON_BRAKET_QDMI_LIBRARY_PATH, start=Path.cwd()))\nAMAZON_BRAKET_QDMI_CATALOG_PATH = Path(os.path.relpath(AMAZON_BRAKET_QDMI_CATALOG_PATH, start=Path.cwd()))\n\ndel dist, located_include_dir, resolved_include_dir'
}

cwd_smoke() {
  phase=$1
  mkdir -p "$ARM/cwd" "$ARM/cwd-consumer-src"
  set +e
  "$py" - "$ARM/cwd" "$ARM/cwd-consumer-src" \
    "$ARM/cwd-consumer-build" >"$ARM/cwd-smoke.log" <<'PY'
import json
import os
import subprocess
import sys
from pathlib import Path

from amazon.braket.qdmi import (
    AMAZON_BRAKET_QDMI_CATALOG_PATH,
    AMAZON_BRAKET_QDMI_CMAKE_DIR,
    AMAZON_BRAKET_QDMI_INCLUDE_DIR,
    AMAZON_BRAKET_QDMI_LIBRARY_PATH,
)

new_cwd, source, build = map(Path, sys.argv[1:])
source.joinpath("CMakeLists.txt").write_text(
    "cmake_minimum_required(VERSION 3.24...4.4)\n"
    "project(specaudit_cwd_consumer LANGUAGES C CXX)\n"
    "find_package(amazon-braket-qdmi-device 1.0.1 REQUIRED CONFIG)\n",
    encoding="utf-8",
)
new_cwd.mkdir(parents=True, exist_ok=True)
os.chdir(new_cwd)
include_ok = (AMAZON_BRAKET_QDMI_INCLUDE_DIR / "amazon_braket_qdmi").is_dir()
cmake_ok = AMAZON_BRAKET_QDMI_CMAKE_DIR.is_dir()
library_ok = AMAZON_BRAKET_QDMI_LIBRARY_PATH.is_file()
try:
    json.loads(AMAZON_BRAKET_QDMI_CATALOG_PATH.read_text(encoding="utf-8"))
    catalogue_ok = True
except (OSError, json.JSONDecodeError):
    catalogue_ok = False
configure = subprocess.run(
    [
        "cmake", "-S", str(source), "-B", str(build), "-G", "Ninja",
        "-DCMAKE_BUILD_TYPE=Release",
        "-Damazon-braket-qdmi-device_DIR="
        f"{AMAZON_BRAKET_QDMI_CMAKE_DIR / 'amazon-braket-qdmi-device'}",
    ],
    check=False,
    stdout=subprocess.DEVNULL,
    stderr=subprocess.DEVNULL,
)
values = (include_ok, catalogue_ok, library_ok, cmake_ok, configure.returncode == 0)
print("cwd_oracle=" + ":".join("pass" if value else "fail" for value in values))
raise SystemExit(0 if all(values) else 1)
PY
  status=$?
  set -e
  case "$phase" in
    baseline)
      test "$status" -eq 0
      grep -Fqx 'cwd_oracle=pass:pass:pass:pass:pass' "$ARM/cwd-smoke.log" ;;
    fault)
      test "$status" -eq 1
      grep -Fqx 'cwd_oracle=pass:pass:pass:pass:fail' "$ARM/cwd-smoke.log" ;;
    *) return 64 ;;
  esac
  sed "s/^/cwd_result|phase=$phase|/" "$ARM/cwd-smoke.log"
}

if selected T2-014R1; then
  begin_arm T2-014R1-baseline
  python_mode 3.14 test/python/test_init.py
  assert_pytest_report "$LAST_TELEMETRY" 0 10 10 0 0 0 0 0 \
    "$(init_nodes)" '' '' '' ''
  cwd_smoke baseline
  end_arm
  begin_arm T2-014R1-fault
  fault_relative_paths
  set +e
  python_mode 3.14 test/python/test_init.py
  pytest_status=$?
  set -e
  assert_pytest_report "$LAST_TELEMETRY" 1 10 9 0 1 0 0 0 \
    "$(t2_014_passes)" '' "$(t2_014_failures)" '' ''
  test "$pytest_status" -eq 1
  cwd_smoke fault
  end_arm
fi
```

For `T2-002`, use the same two-arm pattern but run both
`uv lock --check --offline` and `lint_mode`; the fault is the exact
`version = "1.0.1"` to `version = "1.0.2"` replacement, with `uv.lock`
unchanged. For `T2-007`, run both `tests-3.14` and `minimums-3.14` with
`test/python/test_init.py`; replace only the test-group requirement
`pytest>=9.0.2` with `pytest>=8.0.0`. These two recipes intentionally permit the
changed arm to fail and restore the lock through `end_arm`.

`T2-010` uses `mode_init` and one coherent changed arm: replace the selected
library with
`_AMAZON_BRAKET_QDMI_LIBRARY_DIR / "missing-amazon-braket-qdmi-device"`; replace
the include export with `_AMAZON_BRAKET_QDMI_DATA / "missing-include"`; replace
the CMake export with `_AMAZON_BRAKET_QDMI_DATA / "share" / "missing-cmake"`;
and remove the three corresponding import-time `exists()` guards. `T2-013R1`
appends four `str(...)` reassignments immediately before the final `del` in
`__init__.py` and uses `mode_init`. `T2-014R1` adds `import os`, reassigns the
same four exports to `Path(os.path.relpath(value, start=Path.cwd()))`, runs
`mode_init`, then changes cwd in a subprocess and repeats
include/catalogue/library reads plus the installed consumer below. These are
exact coherent multi-edit faults; no resource value is printed.

The remaining T1 narrowing calls are executable with the same helper. Multiple
line deletions are issued from highest to lowest pinned-E line so each target
remains exact.

```sh
selected T1-001R1 && manual_pair T1-001R1 mode_lint \
  'delete_lines .pre-commit-config.yaml 59 63'
selected T1-002R1 && manual_pair T1-002R1 mode_lint \
  'delete_lines .pre-commit-config.yaml 65 70'
selected T1-004R1 && manual_pair T1-004R1 mode_lint \
  'replace_exact .pre-commit-config.yaml '"'"'        args: [--only-group=typecheck]'"'"' $'"'"'        args: [--only-group=typecheck]\n        files: ^python/amazon/braket/qdmi/'"'"''
selected T1-007 && manual_pair T1-007 mode_minimums \
  'replace_exact noxfile.py '"'"'            install_args=["--resolution=lowest-direct"],'"'"' '"'"'            install_args=[],'"'"''
selected T1-010 && manual_pair T1-010 mode_init_coverage \
  'delete_lines test/python/test_init.py 100 100; delete_lines test/python/test_init.py 93 93; delete_lines test/python/test_init.py 87 87; delete_lines test/python/test_init.py 79 79'
selected T1-012 && manual_pair T1-012 mode_init_coverage \
  'delete_lines test/python/test_init.py 102 102; delete_lines test/python/test_init.py 95 95; delete_lines test/python/test_init.py 81 81'
selected T1-018 && manual_pair T1-018 mode_pl311 \
  'replace_exact test/python/test_pennylane.py $'"'"'try:\n    import pennylane as qp\nexcept ImportError:\n    pytest.skip("Install the PennyLane extra to run these tests.", allow_module_level=True)'"'"' '"'"'import pennylane as qp'"'"''
selected T1-020 && manual_pair T1-020 mode_qaoa311 \
  'replace_exact test/python/test_pennylane_qaoa.py $'"'"'try:\n    import pennylane as qp\nexcept ImportError:\n    pytest.skip("Install the PennyLane extra to run these tests.", allow_module_level=True)'"'"' '"'"'import pennylane as qp'"'"''
```

`T2-002` and `T2-007` are reproduced without an implicit lock rewrite:

```sh
if selected T2-002; then
  begin_arm T2-002-baseline
  uv lock --check --offline
  lint_mode
  validate_lint_outcome T2-002 baseline "$LAST_STATUS" "$LAST_LOG"
  end_arm
  begin_arm T2-002-fault
  replace_exact pyproject.toml 'version = "1.0.1"' 'version = "1.0.2"'
  set +e
  uv lock --check --offline >"$ARM/lock.log" 2>&1
  lock_status=$?
  lint_mode
  lint_status=$?
  set -e
  test "$lock_status" -eq 1
  test "$lint_status" -eq 1
  validate_lint_outcome T2-002 changed "$lint_status" "$LAST_LOG"
  end_arm
fi

if selected T2-007; then
  begin_arm T2-007-baseline
  nox_mode tests-3.14 test/python/test_init.py
  validate_nox_pass T2-007 baseline-ordinary "$LAST_STATUS"
  nox_mode minimums-3.14 test/python/test_init.py
  validate_nox_pass T2-007 baseline-minimums "$LAST_STATUS"
  end_arm
  begin_arm T2-007-fault
  replace_exact pyproject.toml '"pytest>=9.0.2",' '"pytest>=8.0.0",'
  nox_mode tests-3.14 test/python/test_init.py
  ordinary_status=$?
  validate_nox_pass T2-007 fault-ordinary "$ordinary_status"
  set +e
  nox_mode minimums-3.14 test/python/test_init.py
  minimum_status=$?
  set -e
  test "$minimum_status" -eq 1
  test ! -e "$LAST_TELEMETRY"
  grep -Eq "minversion.*pytest-9\\.0.*pytest-8\\.0\\.0|pytest-8\\.0\\.0.*minversion.*pytest-9\\.0" \
    "$LAST_LOG"
  end_arm
fi
```

The coupled package cases use the following exact mutations and consumers.
`coupled_t2` runs baseline/fault; `coupled_t3` runs current baseline/fault and
narrowed baseline/fault, with a separate reference and all five isolation
dimensions for every arm. The verifier above runs before pytest. The header and
CMake consumer compile after pytest only to preserve the original required-
consumer reachability field; neither consumer is executed.

```sh
header_check() {
  inc=$("$py" -c 'from amazon.braket.qdmi import AMAZON_BRAKET_QDMI_INCLUDE_DIR as p; print(p)')
  printf '%s\n' '#include "amazon-braket-qdmi-device/constants.hpp"' \
    'int main() { return 0; }' >"$ARM/header.cpp"
  c++ -std=c++20 -I"$inc" -c "$ARM/header.cpp" -o "$ARM/header.o"
}

consumer_check() {
  cmake_dir=$("$py" -c 'from amazon.braket.qdmi import AMAZON_BRAKET_QDMI_CMAKE_DIR as p; print(p)')
  data_dir=$("$py" -c 'from amazon.braket.qdmi import AMAZON_BRAKET_QDMI_INCLUDE_DIR as p; print(p.parent)')
  mkdir -p "$ARM/consumer-src"
  printf '%s\n' 'cmake_minimum_required(VERSION 3.24...4.4)' \
    'project(specaudit_consumer LANGUAGES C CXX)' \
    'find_package(amazon-braket-qdmi-device 1.0.1 REQUIRED CONFIG)' \
    'add_executable(specaudit_consumer main.cpp)' \
    'target_link_libraries(specaudit_consumer PRIVATE amazon-braket-qdmi-device)' \
    >"$ARM/consumer-src/CMakeLists.txt"
  printf '%s\n' '#include "amazon-braket-qdmi-device/constants.hpp"' \
    'int main() { return 0; }' >"$ARM/consumer-src/main.cpp"
  cmake -S "$ARM/consumer-src" -B "$ARM/consumer-build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$data_dir" \
    "-Damazon-braket-qdmi-device_DIR=$cmake_dir/amazon-braket-qdmi-device"
  cmake --build "$ARM/consumer-build" --target specaudit_consumer --parallel 4
}

fault_include() {
  replace_exact CMakeLists.txt 'include(cmake/ExternalDependencies.cmake)' \
    $'include(cmake/ExternalDependencies.cmake)\nset(CMAKE_INSTALL_INCLUDEDIR "headers")'
  replace_exact python/amazon/braket/qdmi/__init__.py \
    'data/include/amazon_braket_qdmi' 'data/headers/amazon_braket_qdmi'
  replace_exact python/amazon/braket/qdmi/__init__.py \
    '_AMAZON_BRAKET_QDMI_DATA / "include"' \
    '_AMAZON_BRAKET_QDMI_DATA / "headers"'
}

fault_cmake() {
  replace_exact src/CMakeLists.txt \
    '"${CMAKE_INSTALL_DATADIR}/cmake/${QDMI_TARGET_NAME}"' \
    '"${CMAKE_INSTALL_DATADIR}/CMake/${QDMI_TARGET_NAME}"'
  replace_exact python/amazon/braket/qdmi/__init__.py \
    '_AMAZON_BRAKET_QDMI_DATA / "share" / "cmake"' \
    '_AMAZON_BRAKET_QDMI_DATA / "share" / "CMake"'
}

fault_output() {
  replace_exact src/CMakeLists.txt \
    '             SOVERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}' \
    $'             SOVERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}\n             OUTPUT_NAME braket-qdmi-provider'
  replace_exact python/amazon/braket/qdmi/__init__.py \
    'glob("*amazon-braket-qdmi-device*")' 'glob("*braket-qdmi-provider*")'
}

fault_version() {
  replace_exact pyproject.toml 'version = "${version}"' 'version = ""'
}
narrow_version() { delete_lines test/python/test_init.py 43 43; }
narrow_include() { delete_lines test/python/test_init.py 81 81; }
narrow_output() { delete_lines test/python/test_init.py 102 102; }

fault_session() {
  replace_exact cmake/amazon-braket-qdmi-device.qdmi.json.in \
    $'        "prefix": "AMAZON_BRAKET",\n        "enabled": true\n      },' \
    $'        "prefix": "AMAZON_BRAKET",\n        "enabled": true,\n        "session": {}\n      },'
}
narrow_session() {
  replace_exact test/python/test_init.py \
    '    assert "session" not in devices[AMAZON_BRAKET_QDMI_DEVICE_ID]' \
    $'    generic = devices[AMAZON_BRAKET_QDMI_DEVICE_ID]\n    assert "base-url" not in generic.get("session", {})\n    assert "custom2" not in generic.get("session", {})'
}

fault_catalogue_malformed() {
  replace_exact cmake/amazon-braket-qdmi-device.qdmi.json.in \
    $'{\n  "schema-version": 1,' $'[\n  "schema-version": 1,'
}
fault_catalogue_root() {
  replace_exact cmake/amazon-braket-qdmi-device.qdmi.json.in \
    '  "qdmi": {' '  "qdmi-mutant": {'
}
fault_catalogue_id() {
  replace_exact cmake/amazon-braket-qdmi-device.qdmi.json.in \
    '"amazon.braket.aqt.ibex-q1"' '"amazon.braket.aqt.ibex-q1-mutant"'
}
fault_catalogue_url() {
  replace_exact cmake/amazon-braket-qdmi-device.qdmi.json.in \
    '"arn:aws:braket:eu-north-1::device/qpu/aqt/Ibex-Q1"' \
    '"https://example.invalid/device"'
}
fault_entrypoint() {
  replace_exact pyproject.toml \
    'amazon.braket.qdmi._pennylane_entrypoint:AmazonBraketDevice' \
    'amazon.braket.qdmi._wrong_entrypoint:AmazonBraketDevice'
}

coupled_arm() {
  id=$1
  side=$2
  phase=$3
  kind=$4
  narrow=$5
  fault=$6
  extra=$7
  begin_arm "$id-$side-$phase"
  test "$narrow" = : || "$narrow"
  test "$phase" = baseline || "$fault"
  VERIFY_KIND=$kind
  VERIFY_PHASE=$phase
  VERIFY_REF="$REPRO_ROOT/$id-$side-reference.json"
  export VERIFY_KIND VERIFY_PHASE VERIFY_REF
  set +e
  python_mode 3.14 test/python/test_init.py
  pytest_status=$?
  set -e
  nodes="$REPRO_ROOT/$id-$side-$phase.nodes"
  python3 - "$LAST_TELEMETRY" "$nodes" <<'PY'
import json
import sys
from pathlib import Path

data = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
text = "\n".join(data["failed"])
Path(sys.argv[2]).write_text(text + ("\n" if text else ""), encoding="utf-8")
PY
  failures=$(wc -l <"$nodes")
  printf 'pytest_result|id=%s|side=%s|phase=%s|status=%s|failures=%s\n' \
    "$id" "$side" "$phase" "$pytest_status" "$failures"
  sed 's/^/failed_node=/' "$nodes"
  validate_coupled_arm "$id" "$side" "$phase" "$pytest_status" "$nodes"
  test "$extra" = : || "$extra"
  unset VERIFY_KIND VERIFY_PHASE VERIFY_REF
  end_arm
}

validate_coupled_arm() {
  id=$1 side=$2 phase=$3 status=$4
  if test "$phase" = baseline; then
    assert_pytest_report "$LAST_TELEMETRY" 0 10 10 0 0 0 0 0 \
      "$(init_nodes)" '' '' '' ''
    test "$status" -eq 0
    return
  fi
  case "$id:$side" in
    T2-030R2:current|T2-031R1:current|T2-032R1:current)
      expected_failed=$'test/python/test_init.py::test_installed_catalogue\ntest/python/test_init.py::test_pennylane_entry_point_is_lazy_on_the_base_install' ;;
    T2-033R1:current)
      expected_failed='test/python/test_init.py::test_installed_catalogue' ;;
    T2-034:current)
      expected_failed='test/python/test_init.py::test_pennylane_entry_point_is_lazy_on_the_base_install' ;;
    T2-012I1:current|T3-003R2:current)
      expected_failed='test/python/test_init.py::test_include_dir_exists' ;;
    T2-012C1:current)
      expected_failed='test/python/test_init.py::test_cmake_dir_exists' ;;
    T2-012O4:current|T3-004:current)
      expected_failed='test/python/test_init.py::test_library_path_exists' ;;
    T3-001:current|T3-001:narrowed)
      expected_failed='test/python/test_init.py::test_version_exists' ;;
    T3-002R1:current)
      expected_failed='test/python/test_init.py::test_installed_catalogue' ;;
    T3-002R1:narrowed|T3-003R2:narrowed|T3-004:narrowed)
      assert_pytest_report "$LAST_TELEMETRY" 0 10 10 0 0 0 0 0 \
        "$(init_nodes)" '' '' '' ''
      test "$status" -eq 0
      return ;;
    *)
      printf 'no coupled oracle for %s:%s\n' "$id" "$side" >&2
      return 64 ;;
  esac
  expected_passed=$(init_nodes)
  while IFS= read -r failed; do
    expected_passed=$(printf '%s\n' "$expected_passed" | grep -Fvx "$failed")
  done <<<"$expected_failed"
  failure_count=$(printf '%s\n' "$expected_failed" | wc -l)
  assert_pytest_report "$LAST_TELEMETRY" 1 10 $((10 - failure_count)) \
    0 "$failure_count" 0 0 0 "$expected_passed" '' "$expected_failed" '' ''
  test "$status" -eq 1
}

coupled_t2() {
  id=$1 kind=$2 fault=$3 extra=$4
  coupled_arm "$id" current baseline "$kind" : "$fault" "$extra"
  coupled_arm "$id" current fault "$kind" : "$fault" "$extra"
}

coupled_t3() {
  id=$1 kind=$2 narrow=$3 fault=$4 extra=$5
  coupled_arm "$id" current baseline "$kind" : "$fault" "$extra"
  coupled_arm "$id" current fault "$kind" : "$fault" "$extra"
  coupled_arm "$id" narrowed baseline "$kind" "$narrow" "$fault" "$extra"
  coupled_arm "$id" narrowed fault "$kind" "$narrow" "$fault" "$extra"
  current="$REPRO_ROOT/$id-current-fault.nodes"
  narrowed="$REPRO_ROOT/$id-narrowed-fault.nodes"
  case "$id" in
    T3-001) cmp -s "$current" "$narrowed" ;;
    T3-002R1|T3-003R2|T3-004)
      ! cmp -s "$current" "$narrowed"
      test -s "$current"
      test ! -s "$narrowed" ;;
    *) return 64 ;;
  esac
  printf 'killed_set_comparison|id=%s|equal=%s\n' "$id" \
    "$(cmp -s "$current" "$narrowed" && printf yes || printf no)"
}

selected T2-030R2 && coupled_t2 T2-030R2 catalogue-malformed fault_catalogue_malformed :
selected T2-031R1 && coupled_t2 T2-031R1 catalogue-root fault_catalogue_root :
selected T2-032R1 && coupled_t2 T2-032R1 catalogue-id fault_catalogue_id :
selected T2-033R1 && coupled_t2 T2-033R1 catalogue-url fault_catalogue_url :
selected T2-034 && coupled_t2 T2-034 entrypoint fault_entrypoint :
selected T2-012I1 && coupled_t2 T2-012I1 include fault_include header_check
selected T2-012C1 && coupled_t2 T2-012C1 cmake fault_cmake consumer_check
selected T2-012O4 && coupled_t2 T2-012O4 output fault_output consumer_check
selected T3-001 && coupled_t3 T3-001 version-empty narrow_version fault_version :
selected T3-002R1 && coupled_t3 T3-002R1 catalogue-session narrow_session fault_session :
selected T3-003R2 && coupled_t3 T3-003R2 include narrow_include fault_include header_check
selected T3-004 && coupled_t3 T3-004 output narrow_output fault_output consumer_check
```

The native helpers below use only repository CMake declarations and named
targets. They permit repository-pinned dependency acquisition during configure,
set GoogleTest discovery to `PRE_TEST`, and never run CTest or any executable.
The marker guard fails if configure/build output shows discovery, CTest, binary
execution, a live selector, or an AWS action. `compile_matrix_arm` preserves the
production-build reachability distinction: a producer failure does not become a
test oracle, and each later source/live context is still attempted and reported
separately.

```sh
marker_guard() {
  log=$1
  ! grep -Eq '(gtest_list_tests|GoogleTestAddTests|--gtest_list_tests|(^|[ /])ctest([ :]|$)|Start [0-9]+:|Test #[0-9]+:|Running.*amazon-braket-qdmi-device.*-test|AMAZON_BRAKET_QDMI_RUN_LIVE_CATALOG=1|CreateQuantumTask|CancelQuantumTask|PutObject|CreateBucket)' "$log"
}

configure_native() {
  build=$1 tests=$2 live=$3
  shift 3
  cmake -S . -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_AMAZON_BRAKET_TESTS="$tests" \
    -DBUILD_AMAZON_BRAKET_LIVE_TESTS="$live" \
    -DCMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE=PRE_TEST "$@"
}

target_build() {
  build=$1 target=$2 log=$3
  if cmake --build "$build" --target "$target" --parallel 4 >"$log" 2>&1; then
    status=0
  else
    status=$?
  fi
  marker_guard "$log" || return 90
  tus=$(grep -Eo '[^ /]+\.cpp\.o' "$log" | sed 's/\.o$//' |
    sort -u | paste -sd, - || true)
  test -n "$tus" || tus=none
  link=no
  grep -Eq 'Linking CXX executable|Linking CXX shared library' "$log" && link=yes
  printf 'build|target=%s|status=%s|compiled_tus=%s|link=%s\n' \
    "$target" "$status" "$tus" "$link"
  return "$status"
}

native_consumer() {
  prefix=$1
  mkdir -p "$ARM/native-consumer-src"
  printf '%s\n' 'cmake_minimum_required(VERSION 3.24...4.4)' \
    'project(specaudit_native_consumer LANGUAGES C CXX)' \
    'find_package(amazon-braket-qdmi-device 1.0.1 REQUIRED CONFIG)' \
    'add_executable(specaudit_native_consumer main.cpp)' \
    'target_link_libraries(specaudit_native_consumer PRIVATE amazon-braket-qdmi-device)' \
    >"$ARM/native-consumer-src/CMakeLists.txt"
  printf '%s\n' '#include "amazon-braket-qdmi-device/constants.hpp"' \
    'int main() { return 0; }' >"$ARM/native-consumer-src/main.cpp"
  cmake -S "$ARM/native-consumer-src" -B "$ARM/native-consumer" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$prefix"
  cmake --build "$ARM/native-consumer" \
    --target specaudit_native_consumer --parallel 4
}

compile_matrix_arm() {
  producer="$ARM/producer"
  prefix="$ARM/prefix"
  configure_native "$producer" OFF OFF -DCMAKE_INSTALL_PREFIX="$prefix" || return
  set +e
  target_build "$producer" amazon-braket-qdmi-device "$ARM/producer.log"
  producer_status=$?
  set -e
  if test "$producer_status" -eq 0; then
    cmake --install "$producer" \
      --component amazon-braket-qdmi-device_Runtime || return
    cmake --install "$producer" \
      --component amazon-braket-qdmi-device_Development || return
    native_consumer "$prefix" || return
    consumer_reached=yes
  else
    printf 'installed_consumer=not-reached\n'
    consumer_reached=no
  fi
  configure_native "$ARM/source" ON OFF || return
  set +e
  target_build "$ARM/source" \
    amazon-braket-qdmi-device-test "$ARM/source.log"
  source_status=$?
  set -e
  configure_native "$ARM/live" ON ON || return
  set +e
  target_build "$ARM/live" \
    amazon-braket-qdmi-device-live-test "$ARM/live.log"
  live_status=$?
  set -e
  COMPILE_MATRIX_TUPLE="$producer_status:$consumer_reached:$source_status:$live_status"
  printf 'compile_matrix_tuple=%s\n' "$COMPILE_MATRIX_TUPLE"
}

live_compile_arm() {
  configure_native "$ARM/live" ON ON
  set +e
  target_build "$ARM/live" amazon-braket-qdmi-device-live-test "$ARM/live.log"
  status=$?
  set -e
  test_live=no
  grep -Eq 'test_live\.cpp(\.o|[^[:alnum:]_])' "$ARM/live.log" && test_live=yes
  printf 'live_tuple|status=%s|test_live_tu=%s\n' "$status" "$test_live"
  LIVE_TUPLE="$status:$test_live"
}

interface_arm() {
  statuses=
  for setting in ON OFF; do
    build="$ARM/interface-$setting"
    configure_native "$build" OFF OFF \
      -DCMAKE_VERIFY_INTERFACE_HEADER_SETS="$setting" || return
    set +e
    target_build "$build" amazon-braket-qdmi-device \
      "$ARM/interface-$setting.log"
    one_status=$?
    set -e
    statuses="${statuses:+$statuses:}$one_status"
  done
  INTERFACE_TUPLE=$statuses
  printf 'interface_tuple=%s\n' "$INTERFACE_TUPLE"
}

configure_guard_arm() {
  if configure_native "$ARM/guard" ON OFF >"$ARM/configure.log" 2>&1; then
    status=0
  else
    status=$?
  fi
  printf 'configure_status=%s\n' "$status"
  return "$status"
}
```

The final collection and live-target pairs are fully defined here. Collection
validation uses the semantic no-tests-collected category rather than one pytest
numeric spelling: current fault must be one named module skip and zero errors;
narrowed fault must be one `ModuleNotFoundError` collection error and no skip.
Both have zero collected node IDs, and the command is always `--collect-only`.
Compile validation requires both healthy baselines to reach `test_live.cpp` and
link, both fault arms to fail before that TU/link, equal fault tuples, and zero
for all marker categories guarded above.

```sh
fault_missing_extra() {
  replace_exact pyproject.toml \
    '"mqt-core[pennylane] @ git+https://github.com/munich-quantum-toolkit/core.git@0fe651210c52dbcf67e49e567ef67e1c9a33d809",' \
    '"mqt-core @ git+https://github.com/munich-quantum-toolkit/core.git@0fe651210c52dbcf67e49e567ef67e1c9a33d809",'
}
narrow_unconditional_import() {
  replace_exact test/python/test_pennylane.py \
    $'try:\n    import pennylane as qp\nexcept ImportError:\n    pytest.skip("Install the PennyLane extra to run these tests.", allow_module_level=True)' \
    'import pennylane as qp'
}

collection_arm() {
  id=$1 side=$2 phase=$3
  begin_arm "$id-$side-$phase"
  test "$side" = current || narrow_unconditional_import
  test "$phase" = baseline || fault_missing_extra
  set +e
  collect_mode 3.11 test/python/test_pennylane.py
  status=$?
  set -e
  if test "$phase" = baseline; then
    assert_pytest_report "$LAST_TELEMETRY" 0 23 0 0 0 0 0 0 \
      '' '' '' '' "$(pennylane_nodes)"
    expected_status=0
    skipped=0
    errors=0
  elif test "$side" = current; then
    assert_pytest_report "$LAST_TELEMETRY" 5 1 0 1 0 0 0 0 \
      '' 'test/python/test_pennylane.py' '' '' ''
    expected_status=5
    skipped=1
    errors=0
  else
    assert_pytest_report "$LAST_TELEMETRY" 2 1 0 0 0 1 0 0 \
      '' '' '' 'test/python/test_pennylane.py' ''
    expected_status=2
    skipped=0
    errors=1
    grep -Fq 'ModuleNotFoundError' "$LAST_LOG"
  fi
  test "$status" -eq "$expected_status"
  printf 'collection|side=%s|phase=%s|status=%s|skipped=%s|errors=%s|nodes=exact\n' \
    "$side" "$phase" "$status" "$skipped" "$errors"
  end_arm
}

if selected T3-005R3; then
  collection_arm T3-005R3 current baseline
  collection_arm T3-005R3 current fault
  collection_arm T3-005R3 narrowed baseline
  collection_arm T3-005R3 narrowed fault
fi

fault_constants() {
  replace_exact include/amazon-braket-qdmi-device/constants.hpp \
    '#pragma once' '#error "SpecAudit public-header compile mutant"'
}
fault_prefix() {
  replace_exact src/CMakeLists.txt \
    'generate_prefixed_qdmi_headers(${QDMI_PREFIX})' \
    'generate_prefixed_qdmi_headers(AMAZON_BRAKET_MUTANT)'
}
narrow_live_constants() {
  replace_exact test/test_live.cpp \
    $'#include "amazon-braket-qdmi-device/constants.hpp"\n#include "amazon_braket_qdmi/device.h"' \
    '#include "amazon_braket_qdmi/device.h"'
  replace_exact test/test_live.cpp \
    'AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_REGION' \
    'QDMI_DEVICE_SESSION_PARAMETER_CUSTOM2'
  replace_count test/test_live.cpp \
    'AMAZON_BRAKET_QDMI_DEVICE_SESSION_PARAMETER_DEVICEARN' \
    'QDMI_DEVICE_SESSION_PARAMETER_BASEURL' 2
  replace_exact test/test_live.cpp \
    'std::getenv(AMAZON_BRAKET_QDMI_DEVICE_ENV_TASK_RESULTS_S3_URI)' \
    'std::getenv("AMZN_BRAKET_TASK_RESULTS_S3_URI")'
}
narrow_live_device() {
  replace_exact test/test_live.cpp \
    $'#include "amazon-braket-qdmi-device/constants.hpp"\n#include "amazon_braket_qdmi/device.h"' \
    '#include "amazon-braket-qdmi-device/constants.hpp"'
}

live_pair() {
  id=$1 narrow=$2 fault=$3
  for side in current narrowed; do
    for phase in baseline fault; do
      begin_arm "$id-$side-$phase"
      test "$side" = current || "$narrow"
      test "$phase" = baseline || "$fault"
      live_compile_arm
      tuple=$LIVE_TUPLE
      printf '%s\n' "$tuple" >"$REPRO_ROOT/$id-$side-$phase.tuple"
      if test "$phase" = baseline; then
        test "$tuple" = '0:yes'
      else
        test "$tuple" = '1:no'
      fi
      end_arm
    done
  done
  cmp -s "$REPRO_ROOT/$id-current-fault.tuple" \
    "$REPRO_ROOT/$id-narrowed-fault.tuple"
  printf 'compile_tuple_comparison|id=%s|equal=yes\n' "$id"
}

selected T3-006R2 && live_pair T3-006R2 narrow_live_constants fault_constants
selected T3-007 && live_pair T3-007 narrow_live_device fault_prefix
```

For the compile-matrix evidence, run the following exact two-arm calls. The
changed-arm oracle is: `T1-021R1` all installed/source/live contexts pass;
`T2-021` and `T2-022` have healthy baselines and fault-side producer,
source-target, and live-target build failures, with installed consumer not
reached when producer build fails. `T1-022R1` removes only the `constants.hpp`
include from `test_device_unit.cpp` and must fail its named source target.
`T1-023` applies `narrow_live_constants` and must produce `LIVE_TUPLE=0:yes`.

```sh
remove_direct_device_headers() {
  for file in test/test_device.cpp test/test_device_unit.cpp test/test_live.cpp; do
    replace_exact "$file" $'#include "amazon-braket-qdmi-device/constants.hpp"\n#include "amazon_braket_qdmi/device.h"' \
      '#include "amazon-braket-qdmi-device/constants.hpp"'
  done
}
mode_compile_matrix() {
  phase=$1
  LAST_KIND=custom
  compile_matrix_arm || return
  case "$phase:$id" in
    baseline:*) test "$COMPILE_MATRIX_TUPLE" = '0:yes:0:0' ;;
    changed:T1-021R1) test "$COMPILE_MATRIX_TUPLE" = '0:yes:0:0' ;;
    changed:T2-021|changed:T2-022)
      test "$COMPILE_MATRIX_TUPLE" = '1:no:1:1' ;;
    *) return 64 ;;
  esac
}
selected T1-021R1 && manual_pair T1-021R1 mode_compile_matrix remove_direct_device_headers
selected T2-021 && manual_pair T2-021 mode_compile_matrix fault_prefix
selected T2-022 && manual_pair T2-022 mode_compile_matrix fault_constants

if selected T1-022R1; then
  begin_arm T1-022R1
  replace_exact test/test_device_unit.cpp \
    $'#include "amazon-braket-qdmi-device/constants.hpp"\n#include "amazon_braket_qdmi/device.h"' \
    '#include "amazon_braket_qdmi/device.h"'
  configure_native "$ARM/source" ON OFF
  set +e
  run_status target_build "$ARM/source" amazon-braket-qdmi-device-test \
    "$ARM/source.log"
  status=$?
  set -e
  test "$status" -eq 1
  grep -Fq 'test_device_unit.cpp' "$ARM/source.log"
  end_arm
fi

if selected T1-023; then
  begin_arm T1-023
  narrow_live_constants
  live_compile_arm
  test "$LIVE_TUPLE" = '0:yes'
  end_arm
fi
```

The interface, package-version, and configure-guard anchors use these exact
recipes. They validate the named consumer or guard rather than treating an
earlier producer/configure failure as a test oracle.

```sh
fault_interface_header() {
  replace_exact include/amazon-braket-qdmi-device/constants.hpp \
    '#include "amazon_braket_qdmi/device.h"' \
    'static_assert(sizeof(AMAZON_BRAKET_QDMI_Device) > 0);'
}
mode_interface() {
  phase=$1
  LAST_KIND=custom
  interface_arm || return
  case "$phase" in
    baseline) test "$INTERFACE_TUPLE" = '0:0' ;;
    changed) test "$INTERFACE_TUPLE" = '1:1' ;;
    *) return 64 ;;
  esac
}
selected T2-005 && manual_pair T2-005 mode_interface fault_interface_header

installed_version_producer() {
  if configure_native "$ARM/producer" OFF OFF \
    -DCMAKE_INSTALL_PREFIX="$ARM/prefix" >"$ARM/producer-configure.log" 2>&1; then
    producer_configure_status=0
  else
    producer_configure_status=$?
  fi
  test "$producer_configure_status" -eq 0
  if target_build "$ARM/producer" amazon-braket-qdmi-device \
    "$ARM/producer-build.log"; then
    producer_build_status=0
  else
    producer_build_status=$?
  fi
  test "$producer_build_status" -eq 0
  if cmake --install "$ARM/producer" \
    --component amazon-braket-qdmi-device_Runtime \
    >"$ARM/runtime-install.log" 2>&1; then
    runtime_install_status=0
  else
    runtime_install_status=$?
  fi
  test "$runtime_install_status" -eq 0
  if cmake --install "$ARM/producer" \
    --component amazon-braket-qdmi-device_Development \
    >"$ARM/development-install.log" 2>&1; then
    development_install_status=0
  else
    development_install_status=$?
  fi
  test "$development_install_status" -eq 0
  printf 'version_producer|configure=0|build=0|runtime_install=0|development_install=0\n'
}

write_version_consumer() {
  mkdir -p "$ARM/version-consumer-src"
  printf '%s\n' 'cmake_minimum_required(VERSION 3.24...4.4)' \
    'project(specaudit_version_consumer LANGUAGES C CXX)' \
    'find_package(amazon-braket-qdmi-device 1.0.1 REQUIRED CONFIG)' \
    'add_executable(specaudit_version_consumer main.cpp)' \
    'target_link_libraries(specaudit_version_consumer PRIVATE amazon-braket-qdmi-device)' \
    >"$ARM/version-consumer-src/CMakeLists.txt"
  printf '%s\n' '#include "amazon-braket-qdmi-device/constants.hpp"' \
    'int main() { return 0; }' >"$ARM/version-consumer-src/main.cpp"
}

configure_version_consumer() {
  write_version_consumer
  if cmake -S "$ARM/version-consumer-src" -B "$ARM/version-consumer-build" \
    -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$ARM/prefix" >"$ARM/consumer-configure.log" 2>&1; then
    consumer_configure_status=0
  else
    consumer_configure_status=$?
  fi
  printf 'version_consumer|configure=%s\n' "$consumer_configure_status"
  return "$consumer_configure_status"
}

build_version_consumer() {
  target_build "$ARM/version-consumer-build" specaudit_version_consumer \
    "$ARM/consumer-build.log"
}

if selected T2-023; then
  begin_arm T2-023-baseline
  installed_version_producer
  configure_version_consumer
  baseline_consumer_configure_status=$?
  test "$baseline_consumer_configure_status" -eq 0
  build_version_consumer
  baseline_consumer_build_status=$?
  test "$baseline_consumer_build_status" -eq 0
  printf 'version_result|phase=baseline|producer=0:0:0:0|consumer=0:0\n'
  end_arm
  begin_arm T2-023-fault
  replace_exact src/CMakeLists.txt '  VERSION ${PROJECT_VERSION}' \
    '  VERSION 2.0.0'
  installed_version_producer
  set +e
  configure_version_consumer
  fault_consumer_configure_status=$?
  set -e
  test "$fault_consumer_configure_status" -eq 1
  test ! -e "$ARM/consumer-build.log"
  printf 'version_result|phase=fault|producer=0:0:0:0|consumer=1:not-reached\n'
  end_arm
fi

fault_guard_combined() {
  replace_exact src/CMakeLists.txt \
    $'  PROPERTIES QDMI_DEVICE_ID amazon.braket.default\n             QDMI_DEVICE_PREFIX ${QDMI_PREFIX}\n             QDMI_MANIFEST_NAME ${AMAZON_BRAKET_QDMI_CATALOG_NAME})' \
    $'  PROPERTIES QDMI_DEVICE_ID amazon.braket.broken\n             QDMI_DEVICE_PREFIX BROKEN_PREFIX\n             QDMI_MANIFEST_NAME amazon-braket-mutant.qdmi.json)'
}
fault_guard_prefix() {
  replace_exact src/CMakeLists.txt \
    '             QDMI_DEVICE_PREFIX ${QDMI_PREFIX}' \
    '             QDMI_DEVICE_PREFIX AMAZON_BRAKET_MUTANT'
}
fault_guard_manifest() {
  replace_exact src/CMakeLists.txt \
    '             QDMI_MANIFEST_NAME ${AMAZON_BRAKET_QDMI_CATALOG_NAME})' \
    '             QDMI_MANIFEST_NAME amazon-braket-mutant.qdmi.json)'
}

guard_pair() {
  id=$1 fault=$2 expected=$3
  begin_arm "$id-baseline"
  configure_guard_arm || {
    end_arm
    return 20
  }
  end_arm
  begin_arm "$id-fault"
  "$fault"
  set +e
  configure_guard_arm
  status=$?
  set -e
  test "$status" -eq 1
  grep -Fq "$expected" "$ARM/configure.log"
  end_arm
}
selected T2-024 && guard_pair T2-024 fault_guard_combined 'stable device ID'
selected T2-024P1 && guard_pair T2-024P1 fault_guard_prefix 'symbol prefix'
selected T2-024M1 && guard_pair T2-024M1 fault_guard_manifest 'device catalog'

floor_nodes() {
  init_nodes
  printf '%s\n' \
    'test/python/test_main.py::test_cli_catalog_path[inprocess]' \
    'test/python/test_main.py::test_cli_cmake_dir[inprocess]' \
    'test/python/test_main.py::test_cli_help[inprocess]' \
    'test/python/test_main.py::test_cli_include_dir[inprocess]' \
    'test/python/test_main.py::test_cli_lib_path[inprocess]' \
    'test/python/test_main.py::test_cli_version[inprocess]' \
    'test/python/test_mqt_core.py::test_register_device_if_absent' \
    'test/python/test_pennylane.py' \
    'test/python/test_pennylane_qaoa.py'
}
floor_skips() {
  printf '%s\n' 'test/python/test_pennylane.py' \
    'test/python/test_pennylane_qaoa.py'
}
floor_baseline_passes() {
  floor_nodes | grep -Fvx 'test/python/test_pennylane.py' |
    grep -Fvx 'test/python/test_pennylane_qaoa.py'
}
floor_fault_passes() {
  floor_baseline_passes | grep -Fvx \
    'test/python/test_init.py::test_pennylane_entry_point_is_lazy_on_the_base_install'
}

floor_structure() {
  kind=$1 phase=$2 reference=$3
  "$py" - "$kind" "$phase" "$reference" <<'PY'
from __future__ import annotations

import hashlib
import json
import sys
from importlib.metadata import distribution
from pathlib import Path

kind, phase, reference_arg = sys.argv[1:]
reference = Path(reference_arg)
dist = distribution("amazon-braket-qdmi")
environment = Path(sys.prefix).resolve()
matches = [
    Path(dist.locate_file(item)).resolve()
    for item in (dist.files or [])
    if str(item).endswith("amazon/braket/qdmi/_pennylane_entrypoint.py")
]
assert len(matches) == 1
module = matches[0]
text = module.read_text(encoding="utf-8")
digest = hashlib.sha256(module.read_bytes()).hexdigest()
record = {
    "digest": digest,
    "environment": module.is_relative_to(environment),
    "branch": text.count("if sys.version_info < (3, 11):") == 1,
    "original": text.count("Python 3.11 or newer") == 1,
    "reworded": text.count("Python >= 3.11") == 1,
    "runtime": text.count("raise RuntimeError(msg)") == 1,
    "import_count": text.count("raise ImportError(msg)"),
}
if phase == "baseline":
    assert record == {
        "digest": digest, "environment": True, "branch": True,
        "original": True, "reworded": False, "runtime": False,
        "import_count": 2,
    }
    reference.write_text(json.dumps(record), encoding="utf-8")
else:
    baseline = json.loads(reference.read_text(encoding="utf-8"))
    assert digest != baseline["digest"]
    if kind == "exception":
        assert record == {
            "digest": digest, "environment": True, "branch": True,
            "original": True, "reworded": False, "runtime": True,
            "import_count": 1,
        }
    else:
        assert record == {
            "digest": digest, "environment": True, "branch": True,
            "original": False, "reworded": True, "runtime": False,
            "import_count": 2,
        }
print(f"floor_coupling|kind={kind}|phase={phase}|coupled=yes")
PY
}

floor_pytest() {
  phase=$1 collect=$2
  LAST_TELEMETRY="$ARM/floor-$phase-$collect.json"
  write_pytest_telemetry
  export AUDIT_TELEMETRY="$LAST_TELEMETRY"
  export AUDIT_SELECTED_SCOPE=test/python
  export AUDIT_COLLECT_ONLY="$collect"
  export PYTHONPATH="$ARM"
  export PYTHONNOUSERSITE=1
  export PYTEST_ADDOPTS=-n0
  args=(test/python)
  [[ $collect == yes ]] && args=(--collect-only test/python)
  set +e
  run_status uv run --no-sync pytest -p audit_pytest_telemetry -n 0 \
    "${args[@]}" --cov-config=pyproject.toml
  result=$?
  set -e
  if [[ $collect == yes ]]; then
    assert_pytest_report "$LAST_TELEMETRY" 0 19 0 2 0 0 0 0 '' \
      "$(floor_skips)" '' '' "$(floor_baseline_passes)"
  elif [[ $phase == baseline ]]; then
    assert_pytest_report "$LAST_TELEMETRY" 0 19 17 2 0 0 0 0 \
      "$(floor_baseline_passes)" "$(floor_skips)" '' '' ''
  else
    assert_pytest_report "$LAST_TELEMETRY" 1 19 16 2 1 0 0 0 \
      "$(floor_fault_passes)" "$(floor_skips)" \
      'test/python/test_init.py::test_pennylane_entry_point_is_lazy_on_the_base_install' \
      '' ''
  fi
  test "$result" -eq "$( [[ $phase == fault && $collect == no ]] && printf 1 || printf 0 )"
}

floor_arm() {
  id=$1 phase=$2 kind=$3 fault=$4
  begin_arm "$id-$phase"
  [[ $phase == baseline ]] || "$fault"
  uv venv --python 3.10 "$ARM/env"
  py="$ARM/env/bin/python"
  test -x "$py"
  test -x "$(readlink -f "$py")"
  export UV_PROJECT_ENVIRONMENT="$ARM/env"
  uv sync --inexact --only-group build --only-group test
  uv sync --inexact --no-dev \
    --no-build-isolation-package amazon-braket-qdmi
  floor_structure "$kind" "$phase" "$REPRO_ROOT/$id-structure.json"
  floor_pytest "$phase" yes
  floor_pytest "$phase" no
  printf 'floor_result|id=%s|phase=%s|collected=19|passed=%s|skipped=2|failed=%s|errors=0|xfail=0|xpass=0\n' \
    "$id" "$phase" "$( [[ $phase == baseline ]] && printf 17 || printf 16 )" \
    "$( [[ $phase == baseline ]] && printf 0 || printf 1 )"
  end_arm
}

fault_floor_exception() {
  replace_exact python/amazon/braket/qdmi/_pennylane_entrypoint.py \
    '        raise ImportError(msg)' '        raise RuntimeError(msg)'
}
fault_floor_wording() {
  replace_exact python/amazon/braket/qdmi/_pennylane_entrypoint.py \
    'The Amazon Braket PennyLane integration requires Python 3.11 or newer.' \
    'The Amazon Braket PennyLane integration requires Python >= 3.11.'
}

if selected T2-042R1; then
  floor_arm T2-042R1 baseline exception fault_floor_exception
  floor_arm T2-042R1 fault exception fault_floor_exception
fi
if selected T2-043R1; then
  floor_arm T2-043R1 baseline wording fault_floor_wording
  floor_arm T2-043R1 fault wording fault_floor_wording
fi

registration_counts() {
  build=$1
  files="$ARM/registration-files"
  find "$build" -type f -name 'CTestTestfile.cmake' -print | LC_ALL=C sort >"$files"
  REGISTRATION_FILES=$(wc -l <"$files")
  if [[ -s $files ]]; then
    REGISTRATION_TARGETS=$(xargs grep -Fho \
      'amazon-braket-qdmi-device-live-test' <"$files" | wc -l || true)
    REGISTRATION_LABELS=$(xargs grep -Fho 'amazon-braket-live' <"$files" | wc -l || true)
  else
    REGISTRATION_TARGETS=0
    REGISTRATION_LABELS=0
  fi
}

configure_live_contradiction() {
  build=$1 log=$2
  if configure_native "$build" OFF ON >"$log" 2>&1; then
    result=0
  else
    result=$?
  fi
  return "$result"
}

count_live_policy_markers() {
  pattern=$1
  shift
  if matches=$(grep -Eih -- "$pattern" "$@"); then
    grep_status=0
  else
    grep_status=$?
  fi
  case "$grep_status" in
    0) printf '%s\n' "$matches" | wc -l ;;
    1) printf '0\n' ;;
    *)
      printf 'live-policy marker scan failed: status=%s\n' "$grep_status" >&2
      return "$grep_status" ;;
  esac
}

assert_live_policy_safety() {
  discovery=$(count_live_policy_markers \
    '(--gtest_list_tests|gtest_list_tests|GoogleTestAddTests)' "$@")
  ctest=$(count_live_policy_markers \
    '(^|[ /])ctest([ :]|$)' "$@")
  binary=$(count_live_policy_markers \
    '^[[:space:]]*([^[:space:]]*/)?amazon-braket-qdmi-device-(live-)?test([[:space:]]|$)' \
    "$@")
  live_env=$(count_live_policy_markers \
    '(AMAZON_BRAKET_QDMI_RUN_LIVE_CATALOG|AMAZON_BRAKET_QDMI_TEST_ALLOW_BUCKET_CREATION|AMAZON_BRAKET_PENNYLANE_LIVE)=1' \
    "$@")
  aws=$(count_live_policy_markers \
    '(CreateQuantumTask|CancelQuantumTask|PutObject|CreateBucket|task submitted|created remote task)' \
    "$@")
  LIVE_POLICY_SAFETY="$discovery:$ctest:$binary:$live_env:$aws"
  test "$LIVE_POLICY_SAFETY" = '0:0:0:0:0'
}

remove_live_guard() {
  replace_exact CMakeLists.txt \
    $'if(BUILD_AMAZON_BRAKET_LIVE_TESTS AND NOT BUILD_AMAZON_BRAKET_TESTS)\n  message(FATAL_ERROR "BUILD_AMAZON_BRAKET_LIVE_TESTS requires BUILD_AMAZON_BRAKET_TESTS")\nendif()' \
    ''
}
promote_live_parent() {
  replace_exact CMakeLists.txt \
    $'if(BUILD_AMAZON_BRAKET_LIVE_TESTS AND NOT BUILD_AMAZON_BRAKET_TESTS)\n  message(FATAL_ERROR "BUILD_AMAZON_BRAKET_LIVE_TESTS requires BUILD_AMAZON_BRAKET_TESTS")\nendif()' \
    $'if(BUILD_AMAZON_BRAKET_LIVE_TESTS AND NOT BUILD_AMAZON_BRAKET_TESTS)\n  set(BUILD_AMAZON_BRAKET_TESTS ON CACHE BOOL\n      "Build tests for the Amazon Braket QDMI device" FORCE)\nendif()'
}

if selected T2-044; then
  begin_arm T2-044-current-refusal
  set +e
  configure_live_contradiction "$ARM/build" "$ARM/configure.log"
  refusal_status=$?
  set -e
  test "$refusal_status" -eq 1
  grep -Fq 'BUILD_AMAZON_BRAKET_LIVE_TESTS requires BUILD_AMAZON_BRAKET_TESTS' \
    "$ARM/configure.log"
  assert_live_policy_safety "$ARM/configure.log"
  printf 'live_policy|arm=current-refusal|configure=1|category=cmake-configure-failure|safety=%s\n' \
    "$LIVE_POLICY_SAFETY"
  end_arm

  begin_arm T2-044-silent-omission
  remove_live_guard
  configure_live_contradiction "$ARM/build" "$ARM/configure.log"
  grep -Fqx 'BUILD_AMAZON_BRAKET_TESTS:BOOL=OFF' "$ARM/build/CMakeCache.txt"
  grep -Fqx 'BUILD_AMAZON_BRAKET_LIVE_TESTS:BOOL=ON' "$ARM/build/CMakeCache.txt"
  cmake --build "$ARM/build" --target help >"$ARM/help.log" 2>&1
  ! grep -Fq 'amazon-braket-qdmi-device-live-test' "$ARM/help.log"
  registration_counts "$ARM/build"
  test "$REGISTRATION_FILES:$REGISTRATION_TARGETS:$REGISTRATION_LABELS" = '1:0:0'
  assert_live_policy_safety "$ARM/configure.log" "$ARM/help.log"
  printf 'live_policy|arm=silent-omission|configure=0|cache=OFF:ON|target=no|registration=1:0:0|safety=%s\n' \
    "$LIVE_POLICY_SAFETY"
  end_arm

  begin_arm T2-044-parent-promotion
  promote_live_parent
  configure_live_contradiction "$ARM/build" "$ARM/configure.log"
  grep -Fqx 'BUILD_AMAZON_BRAKET_TESTS:BOOL=ON' "$ARM/build/CMakeCache.txt"
  grep -Fqx 'BUILD_AMAZON_BRAKET_LIVE_TESTS:BOOL=ON' "$ARM/build/CMakeCache.txt"
  cmake --build "$ARM/build" --target help >"$ARM/help.log" 2>&1
  grep -Fq 'amazon-braket-qdmi-device-live-test' "$ARM/help.log"
  if cmake --build "$ARM/build" --target \
    amazon-braket-qdmi-device-live-test --parallel 4 >"$ARM/build.log" 2>&1; then
    live_build_status=0
  else
    live_build_status=$?
  fi
  test "$live_build_status" -eq 0
  grep -Eq 'test_live\.cpp(\.o|[^[:alnum:]_])' "$ARM/build.log"
  grep -Eq 'Linking CXX executable' "$ARM/build.log"
  registration_counts "$ARM/build"
  test "$REGISTRATION_FILES:$REGISTRATION_TARGETS:$REGISTRATION_LABELS" = '11:8:2'
  assert_live_policy_safety \
    "$ARM/configure.log" "$ARM/help.log" "$ARM/build.log"
  printf 'live_policy|arm=parent-promotion|configure=0|cache=ON:ON|target=yes|test_live=yes|link=yes|registration=11:8:2|safety=%s\n' \
    "$LIVE_POLICY_SAFETY"
  end_arm
fi

verify_dispatch_complete
:
```

#### Complete canonical dispatch matrix

This is the mechanical 23/44/7 group map. `Probe` means the exact `probe_id`
case above; `Pair`, `Coupled`, `Collect`, `Compile`, `Guard`, and `Installed`
mean the fully defined helpers above. A `manual_pair` row states its complete
pinned-E edit and selection. Combined rows retain every authorized split ID.

| Group  | Admissible ID or IDs             | Canonical mutation, selection, and fail-closed oracle                                                                                                                                                                                                                         |
| :----- | :------------------------------- | :---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| T1-001 | `T1-001R1`                       | `manual_pair`; delete `.pre-commit-config.yaml:59-63`; lint must pass.                                                                                                                                                                                                        |
| T1-002 | `T1-002R1`                       | `manual_pair`; delete `.pre-commit-config.yaml:65-70`; lint must pass.                                                                                                                                                                                                        |
| T1-003 | `T1-003R1`                       | Defined call; `force_author: true` to `false`; all lint hooks pass.                                                                                                                                                                                                           |
| T1-004 | `T1-004R1`                       | Defined call; add `files: ^python/amazon/braket/qdmi/` only to `ty`; lint passes.                                                                                                                                                                                             |
| T1-005 | `T1-005R1`                       | Probe; omit `CMakeLists.txt:47-49`; C++ target/coverage analogue.                                                                                                                                                                                                             |
| T1-006 | `T1-006`                         | Probe; omit `noxfile.py:99-100`; `tests-3.14`, exact `test_init.py`.                                                                                                                                                                                                          |
| T1-007 | `T1-007`                         | Defined call; minimums `install_args=[]`; exact `minimums-3.14 test_init.py` passes.                                                                                                                                                                                          |
| T1-008 | `T1-008`                         | Probe; omit A0015 line 43; exact `test_init.py`.                                                                                                                                                                                                                              |
| T1-009 | `T1-009`                         | Probe; omit A0023 line 70; exact `test_init.py`.                                                                                                                                                                                                                              |
| T1-010 | `T1-010`                         | Defined call; omit only lines 79,87,93,100; exact `test_init.py` passes.                                                                                                                                                                                                      |
| T1-011 | `T1-011`                         | Probe; omit line 80; exact `test_init.py`.                                                                                                                                                                                                                                    |
| T1-012 | `T1-012`                         | Defined call; omit only lines 81,95,102; exact `test_init.py` passes.                                                                                                                                                                                                         |
| T1-013 | `T1-013`                         | Probe; omit lines 107-110; exact `test_init.py`.                                                                                                                                                                                                                              |
| T1-014 | `T1-014`                         | Probe; omit lines 115-118; exact `test_init.py`.                                                                                                                                                                                                                              |
| T1-015 | `T1-015`                         | Probe; omit line 137; exact `test_init.py`.                                                                                                                                                                                                                                   |
| T1-016 | `T1-016`                         | Probe; omit `test_main.py:40`; exact CLI module.                                                                                                                                                                                                                              |
| T1-017 | `T1-017R1`                       | Defined call; replace only PennyLane gate reason; Python 3.10 collect-only named skip.                                                                                                                                                                                        |
| T1-018 | `T1-018`                         | Defined call; unconditional PennyLane import; Python 3.11 collect-only passes.                                                                                                                                                                                                |
| T1-019 | `T1-019R1`                       | Defined call; replace only QAOA gate reason; Python 3.10 collect-only named skip.                                                                                                                                                                                             |
| T1-020 | `T1-020`                         | Defined call; unconditional QAOA import; Python 3.11 collect-only passes.                                                                                                                                                                                                     |
| T1-021 | `T1-021R1`                       | Compile matrix; remove three direct `device.h` includes; installed/source/live builds pass.                                                                                                                                                                                   |
| T1-022 | `T1-022R1`                       | Defined source compile; remove unit `constants.hpp`; named TU build fails.                                                                                                                                                                                                    |
| T1-023 | `T1-023`                         | Defined live compile; exact four-value A0070 narrowing; tuple `0:yes`.                                                                                                                                                                                                        |
| T2-001 | `T2-001`                         | Defined call; invalid TOML `minversion` table; validation hook must fail.                                                                                                                                                                                                     |
| T2-002 | `T2-002`                         | Defined lock/lint pair; version 1.0.2 with unchanged lock; both checks fail.                                                                                                                                                                                                  |
| T2-003 | `T2-003`                         | Defined call; author-only header fault; only license-header hook is required to fail.                                                                                                                                                                                         |
| T2-004 | `T2-004`                         | Defined call; `list[int]` annotation with string values; full lint survives.                                                                                                                                                                                                  |
| T2-005 | `T2-005`                         | Interface pair; replace generated-header include with incomplete-type `static_assert`; ON/OFF builds fail.                                                                                                                                                                    |
| T2-006 | `T2-006R2`                       | Defined staged package pair; broken wheel install dir; exact `test_init.py` collection node reports 20 `FileNotFoundError` errors and no failures/skips.                                                                                                                      |
| T2-007 | `T2-007`                         | Defined ordinary/minimum pair; pytest floor 8.0; ordinary passes, minimum fails.                                                                                                                                                                                              |
| T2-008 | `T2-008`                         | Probe; generated version empty; exact version node set.                                                                                                                                                                                                                       |
| T2-009 | `T2-009`                         | Probe; generic empty `session`; exact zero-failure outcome.                                                                                                                                                                                                                   |
| T2-010 | `T2-010`                         | Defined coherent missing-path call; four named artifact tests fail.                                                                                                                                                                                                           |
| T2-011 | `T2-011`                         | Probe; native file exported as include root; include-shape node fails.                                                                                                                                                                                                        |
| T2-012 | `T2-012I1`,`T2-012C1`,`T2-012O4` | Coupled splits: `headers`, `share/CMake`, and shared `OUTPUT_NAME`; exact lexical node plus header/consumer checks.                                                                                                                                                           |
| T2-013 | `T2-013R1`                       | Defined call; four exports converted to `str`; exact Path-type node fails.                                                                                                                                                                                                    |
| T2-014 | `T2-014R1`                       | Defined cwd pair; four exports converted to cwd-relative `Path`; absolute node and consumer distinction validated.                                                                                                                                                            |
| T2-015 | `T2-015R1`                       | Defined call; harmless `pennylane=None`; exact lazy-proxy node fails.                                                                                                                                                                                                         |
| T2-016 | `T2-016`                         | Probe; synonymous description; exact help node fails.                                                                                                                                                                                                                         |
| T2-017 | `T2-017`                         | Defined call; optional marker 3.11 to 3.10; Python 3.10 named skip unchanged.                                                                                                                                                                                                 |
| T2-018 | `T2-018R1`                       | Defined call; remove PennyLane test extra; Python 3.11 named missing-extra skip.                                                                                                                                                                                              |
| T2-019 | `T2-019`                         | Defined call; same marker fault; QAOA Python 3.10 named skip unchanged.                                                                                                                                                                                                       |
| T2-020 | `T2-020`                         | Defined call; same missing-extra fault; QAOA Python 3.11 named skip.                                                                                                                                                                                                          |
| T2-021 | `T2-021`                         | Compile matrix; generator prefix mutant; producer/source/live build-stage failures.                                                                                                                                                                                           |
| T2-022 | `T2-022`                         | Compile matrix; public `constants.hpp` `#error`; producer/source/live build-stage failures.                                                                                                                                                                                   |
| T2-023 | `T2-023`                         | Installed pair; version file 2.0.0; consumer requesting 1.0.1 fails configure.                                                                                                                                                                                                |
| T2-024 | `T2-024`,`T2-024P1`,`T2-024M1`   | Guard pairs; combined stable-ID-first plus isolated prefix and manifest faults name all three guards.                                                                                                                                                                         |
| T2-025 | `T2-025R1`                       | Defined staged pair; `requires-python <3.14`; wheel builds, Python 3.14 rejects metadata.                                                                                                                                                                                     |
| T2-026 | `T2-026R1`                       | Defined staged pair; delete CMake export; exact `test_init.py`/`test_main.py` collection errors.                                                                                                                                                                              |
| T2-027 | `T2-027`                         | Probe; list version; exact version-type node.                                                                                                                                                                                                                                 |
| T2-028 | `T2-028`                         | Probe; wrong Python device ID; metadata/catalogue nodes.                                                                                                                                                                                                                      |
| T2-029 | `T2-029`                         | Probe; wrong Python prefix; metadata/catalogue nodes.                                                                                                                                                                                                                         |
| T2-030 | `T2-030R2`                       | Coupled distinct-cache pair; malformed installed JSON; exact two-node set.                                                                                                                                                                                                    |
| T2-031 | `T2-031R1`                       | Coupled distinct-cache pair; installed root key mutant; exact two-node set.                                                                                                                                                                                                   |
| T2-032 | `T2-032R1`                       | Coupled distinct-cache pair; installed AQT ID mutant; exact two-node set.                                                                                                                                                                                                     |
| T2-033 | `T2-033R1`                       | Coupled distinct-cache pair; installed AQT URL mutant; catalogue node only.                                                                                                                                                                                                   |
| T2-034 | `T2-034`                         | Coupled exact-env entry-point metadata target mutant; lazy-entry-point node only.                                                                                                                                                                                             |
| T2-035 | `T2-035`                         | Probe; parser successful exits remapped to 1; help/version status nodes.                                                                                                                                                                                                      |
| T2-036 | `T2-036`                         | Probe; include option help suppressed; help node.                                                                                                                                                                                                                             |
| T2-037 | `T2-037`                         | Probe; CLI version `0.0.0`; version-value node.                                                                                                                                                                                                                               |
| T2-038 | `T2-038`,`T2-038V1`              | Probe status fault then value-only successful include branch; status and value nodes separately.                                                                                                                                                                              |
| T2-039 | `T2-039`,`T2-039V1`              | Probe status fault then value-only successful CMake branch; status and value nodes separately.                                                                                                                                                                                |
| T2-040 | `T2-040`,`T2-040V1`              | Probe status fault then value-only successful library branch; status and value nodes separately.                                                                                                                                                                              |
| T2-041 | `T2-041`,`T2-041V1`              | Probe status fault then value-only successful catalogue branch; status and value nodes separately.                                                                                                                                                                            |
| T2-042 | `T2-042R1`                       | Complete installed Python 3.10 `test/python` pair; exact 19-node baseline/fault outcome sets, one lazy-entry-point pass-to-fail delta, and installed-artifact digest coupling for the exception-domain fault.                                                                 |
| T2-043 | `T2-043R1`                       | Complete installed Python 3.10 `test/python` pair; exact 19-node baseline/fault outcome sets, the same one-node delta, and installed-artifact digest coupling for the wording fault.                                                                                          |
| T2-044 | `T2-044`                         | Three configure/target-only arms establish current refusal, silent omission after guard deletion, and standalone target feasibility under parent promotion; no arm runs discovery, CTest, a test binary, AWS, or live behavior, and feasibility is not claimed as the remedy. |
| T3-001 | `T3-001`                         | Coupled four-arm empty-version pair; exact killed sets equal.                                                                                                                                                                                                                 |
| T3-002 | `T3-002R1`                       | Coupled four-arm empty-session pair; current catalogue node versus empty narrowed set.                                                                                                                                                                                        |
| T3-003 | `T3-003R2`                       | Coupled four-arm include relocation; current include lexical node versus empty narrowed set; headers compile.                                                                                                                                                                 |
| T3-004 | `T3-004`                         | Coupled four-arm output rename; current library lexical node versus empty narrowed set; consumer builds.                                                                                                                                                                      |
| T3-005 | `T3-005R3`                       | Collect-only four-arm pair; current named skip versus narrowed `ModuleNotFoundError`; zero node execution.                                                                                                                                                                    |
| T3-006 | `T3-006R2`                       | PRE_TEST live-target-only constants fault; equal early compile tuples, selected TU/link unreached.                                                                                                                                                                            |
| T3-007 | `T3-007`                         | PRE_TEST live-target-only prefix fault; equal early compile tuples, selected TU/link unreached.                                                                                                                                                                               |

Mechanical dispatch accounting for this table is 23 T1 groups, 44 T2 groups, and
seven T3 groups. The T2 table expands to 52 admissible result IDs because
T2-012, T2-024, and T2-038-T2-041 retain authorized split experiments. The
complete-scope replacements `T2-042R1` and `T2-043R1` replace their focused
supporting runs; focused `T2-042`/`T2-043` and nonadjacent `T3-008` are not
dispatch IDs. The 30 historical non-adjudicable IDs and four shared-cache
outcomes are intentionally absent from this dispatch map.

The following authoring-time check was run after the driver and table were
complete. It requires every canonical group exactly once and every admissible
replacement/split ID to occur in the driver before the table.

```sh
python3 - <<'PY'
from collections import Counter
from pathlib import Path
import re

audit = Path(".agent/audits/distribution-catalog-and-python-shell.md").read_text()
driver = re.search(
    r"(?ms)^### Commands\n(.*?)^#### Complete canonical dispatch matrix\n", audit
).group(1)
table = re.search(
    r"(?ms)^#### Complete canonical dispatch matrix\n(.*?)^### Evidence summary\n",
    audit,
).group(1)
rows = re.findall(r"^\| (T[123]-\d{3}) \|", table, re.MULTILINE)
table_ids = []
for line in table.splitlines():
    if re.match(r"^\| T[123]-\d{3} \|", line):
        table_ids.extend(re.findall(r"`(T[123]-[^` ,]+)`", line.split("|")[2]))

supported = re.search(r"(?m)^SUPPORTED_IDS='([^']+)'$", driver).group(1).split()
direct_dispatch = re.findall(r"\bselected (T[123]-[0-9]{3}(?:[A-Z][0-9]+)?)\b", driver)
probe_loop = re.search(
    r"(?s)# Dispatch every supported repo-native probe selected by REPRO_IDS\.\n"
    r"for id in (.*?); do\n  selected \"\$id\" && probe_id \"\$id\"",
    driver,
).group(1)
dispatch_ids = direct_dispatch + re.findall(
    r"T[123]-[0-9]{3}(?:[A-Z][0-9]+)?", probe_loop
)

for name, values in (
    ("supported", supported), ("table", table_ids), ("dispatch", dispatch_ids)
):
    duplicates = sorted(item for item, total in Counter(values).items() if total != 1)
    assert not duplicates, f"{name} IDs not unique: {duplicates}"
assert set(supported) == set(table_ids) == set(dispatch_ids)
assert not {"T2-042", "T2-043", "T3-008"} & set(supported)

for tier, group_count, id_count in (
    ("T1", 23, 23), ("T2", 44, 52), ("T3", 7, 7)
):
    groups = [item for item in rows if item.startswith(f"{tier}-")]
    expected = [f"{tier}-{number:03}" for number in range(1, group_count + 1)]
    tier_ids = [item for item in table_ids if item.startswith(f"{tier}-")]
    missing_groups = sorted(set(expected) - set(groups))
    duplicate_groups = sorted({item for item in groups if groups.count(item) > 1})
    assert len(groups) == group_count
    assert len(tier_ids) == id_count
    assert not missing_groups and not duplicate_groups
    print(
        f"{tier}: groups={len(groups)}/{group_count}; IDs={len(tier_ids)}/{id_count}; "
        "missing_groups=none; duplicate_groups=none; "
        "selector/table/dispatch=exactly-once"
    )
PY
```

Sanitized output:

```text
T1: groups=23/23; IDs=23/23; missing_groups=none; duplicate_groups=none; selector/table/dispatch=exactly-once
T2: groups=44/44; IDs=52/52; missing_groups=none; duplicate_groups=none; selector/table/dispatch=exactly-once
T3: groups=7/7; IDs=7/7; missing_groups=none; duplicate_groups=none; selector/table/dispatch=exactly-once
```

### Evidence summary

- T1: 23/23 mapped groups have admissible evidence.
- T2: 44/44 mapped groups have admissible evidence.
- T3: 7/7 paired groups have admissible factual outcomes.
- Census: 74/74 assertions have executed mapped evidence; no row is Pending.
- Final adjudication: 57 Anchored, 16 Over-specified, one Redundant, zero
  Contract-free, and zero Coverage-driven.
- Historical non-adjudicable attempts: the original 30 are excluded and covered
  by authorized replacements or splits. `T3-008` is a separately preserved 31st
  non-adjudicable attempt; no verdict relies on it and no removal is cleared
  from it.
- Shared-cache catalogue outcomes: excluded; four distinct-cache coupled
  replacements used.
- Fresh complete-scope evidence: `T2-042R1` and `T2-043R1` each retained the
  same 19 collected nodes. Their Python 3.10 baselines passed 17 and skipped
  two; each fault passed 16, skipped two, and failed only the lazy-entry-point
  node, with zero collection errors, xfails, xpasses, or infrastructure
  failures. `T2-044` separately established the no-silent-omission invariant and
  the policy consequences of two alternative guard mechanisms.
- Authoring checks: 74 contiguous census rows and the 57/16/1 class sum; 29
  ledger rows; 11 verdict, ten anchor, and 24 closed result-key headings; exact
  23/44/7 group and 23/52/7 ID dispatch; 11 Commands shell fences extracted to
  2,252 lines; `bash -n`, ShellCheck warning, seven invalid- selector, fence,
  privacy/local-path, whitespace-diff, and only-file checks passed. Markdown was
  then formatted and the complete lint session passed before the decision
  record. The canonical reconstruction recipes were not run and are not
  represented as the commands that produced R01-R24.

### Live evidence

`Not run.`

No task submission, AWS call, credential use, live environment, or external
state change was part of this audit.

### Restoration

Every admissible probe-owned edit was restored. Each paired side used a fresh
detached worktree at E, ran serially, was restored, removed, and absent from the
worktree registry. Final reconciliation recorded zero temporary directories,
zero registry entries, zero active nox/pytest/CMake/Ninja/CTest processes, and
zero raw final logs.

After every admitted command, these checks passed:

```sh
test -z "$(git status --porcelain=v1 --untracked-files=all)"
test "$(git rev-parse HEAD)" = "${E}"
test -z "$(git symbolic-ref --quiet HEAD)"
```

The original 30 non-adjudicable attempts and four shared-cache catalogue
outcomes were not repaired in place or promoted. Their authorized replacements
remain separate evidence records. The later `T3-008` reaching attempt was also
preserved non-adjudicably after its current-fault build oracle mismatch;
narrowed arms did not run and its observation is excluded from every verdict.
The census-amendment executor then finished with no owned temporary directory,
worktree, registry entry, dependency seed, or relevant process, and with the
evidence checkout clean, detached, and exactly at E.

### Drift and revalidation

- **Exact current main SHA M:** `d9c95021451c10614ce2b0c6348480ca96742b9c`
- **Diff:** `git diff --name-status B..refs/remotes/origin/main --`
  `CMakeLists.txt src cmake pyproject.toml noxfile.py`
  `.pre-commit-config.yaml .license-tools-config.json python test` was empty
  because `B = E = M`.
- **Relevant drift:** none.
- **Revalidation:** not required. PRs #183 and #184 are represented in the
  pinned main/evidence SHA supplied for this audit.

### Resolution evidence at exact R

The original campaign baseline B, evidence SHA E, assertion IDs, experiments,
and decisions above remain unchanged. Resolution was derived at exact SHA
`R = 30269f5ea8aaf6c524c583685038b2fc8129bf4d` through this linear signed commit
chain:

| Role                    | Exact signed commit                        | Subject                                          |
| :---------------------- | :----------------------------------------- | :----------------------------------------------- |
| Audit A                 | `e8be21c3ce65bc0633454a34b2f0779c91223147` | Document distribution and Python shell SpecAudit |
| Audit formatting        | `3cec314c6ce62f1f68950cee31dee4b29b21f35a` | Apply audit formatting                           |
| Resolution policy       | `f11351db141f310590d507185ffd2a527d67aa97` | Allow audit resolutions in audit pull requests   |
| Maintainer decision     | `e934b6249c49f63cbf0bb04d352c24c5c8fd03c3` | Record distribution audit maintainer decisions   |
| Replacement oracles     | `e08d323f40eb38dd6ba629dcabcdb42feff075b5` | Add semantic distribution test oracles           |
| Header consumer         | `fa98c4d8b016fcce45495e7d9896cd6d36451b84` | Add a generated-header consumer                  |
| Lazy-import replacement | `8b1c42aa0aab02045be45c7c6c1c9dbdcc82e232` | Decouple lazy-import oracle from diagnostics     |
| Installed consumers     | `99d7c510689365e024f8912a72560288e96d0e7e` | Add installed distribution consumer tests        |
| Assertion narrowing     | `454c203191dc3517e838079a0f90ac6d2656ce28` | Narrow over-specified distribution assertions    |
| Include cleanup         | `2232c69e2dce7aae515e0864b7c49ada91552dbb` | Remove redundant generated-header includes       |
| Policy documentation    | `4e1beface7e11ed22663ac6949eaa0ec0b631130` | Document the live-test option policy             |
| Isolated relocation     | `30269f5ea8aaf6c524c583685038b2fc8129bf4d` | Test relocated runtime in an isolated process    |

`git verify-commit` passed for every commit in this table. The following offline
project entry points passed at R:

```sh
uvx nox -s lint
uvx nox -s tests-3.10
uvx nox -s tests-3.11
uvx nox -s tests-3.12
uvx nox -s tests-3.13
uvx nox -s tests-3.14
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_AMAZON_BRAKET_TESTS=ON \
  -DBUILD_AMAZON_BRAKET_LIVE_TESTS=OFF
cmake --build build --config Release
ctest -C Release --test-dir build --output-on-failure \
  -E '^(AmazonBraketQDMISpecificationTest|AmazonBraketQDMIJobSpecificationTest|AmazonBraketQDMIPerJobS3Test|DeviceParsingTestFixture)\.|^AmazonBraketQDMIWaitTimeoutTest\.JobWaitTimeout$'
uvx nox --non-interactive -s docs
```

Python 3.10 passed 26 tests with two modules skipped during collection. Each of
Python 3.11, 3.12, 3.13, and 3.14 passed 49 tests with two expected skips. The
non-live C++ selection passed 116/116 tests. Documentation and lint passed.

#### V3 generated-header reachability

The full exact-R record
`amazon-braket-specaudit-2026-08-19-v3-reachability-30269f5.md` is summarized
here so it is not the sole reproduction source. It used a fresh detached
worktree and external build/install directories. QDMI was pinned at
`e80020f7ace5c0a716142378c812f30f86263c4e`; AWS SDK was pinned at
`444d1f7ce155a9e7ca33f4f28c511a5934c9e4b3`. A clean checkout can first let the
repository configure acquire those pinned sources, then run the evidence
configure fully disconnected:

```sh
R=30269f5ea8aaf6c524c583685038b2fc8129bf4d
root=$(mktemp -d)
wt=$root/worktree
prep=$root/prep
build=$root/build
install=$root/install
git worktree add --detach "$wt" "$R"
test -z "$(git -C "$wt" status --porcelain=v1 --untracked-files=all)"
test "$(git -C "$wt" rev-parse HEAD)" = "$R"
test -z "$(git -C "$wt" symbolic-ref --quiet HEAD)"

cmake -S "$wt" -B "$prep" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_AMAZON_BRAKET_TESTS=ON \
  -DBUILD_AMAZON_BRAKET_LIVE_TESTS=ON \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

cmake -S "$wt" -B "$build" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_AMAZON_BRAKET_TESTS=ON \
  -DBUILD_AMAZON_BRAKET_LIVE_TESTS=ON \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON \
  -DFETCHCONTENT_SOURCE_DIR_QDMI="$prep/_deps/qdmi-src" \
  -DFETCHCONTENT_SOURCE_DIR_AWSSDK="$prep/_deps/awssdk-src" \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$prep/_deps/googletest-src"
```

The executed record used already cached copies of the same pinned sources for
the three `FETCHCONTENT_SOURCE_DIR_*` values. It selected each exact command
from the generated compilation database and ran the compiler directly, so Ninja
could not discover, link, or execute a test:

```sh
compdb=$build/compile_commands.json
for file in \
  test_device.cpp \
  test_device_unit.cpp \
  test_generated_device_header.c \
  test_live.cpp \
  test_measurement.cpp
do
  command=$(jq -er --arg suffix "/test/$file" \
    '.[] | select(.file | endswith($suffix)) | .command' "$compdb")
  (cd "$build" && bash -c "$command")
done
```

All five baseline compiler commands exited `0`. The exact build-tree mutation
inserted `#error V3_REACHABILITY_SENTINEL` into
`$build/src/include/amazon_braket_qdmi/device.h`:

```sh
sed -i '26i#error V3_REACHABILITY_SENTINEL' \
  "$build/src/include/amazon_braket_qdmi/device.h"
```

Repeating the identical five commands produced this complete tuple:

| Translation unit                 | Exit | Sentinel diagnostic |
| :------------------------------- | ---: | :------------------ |
| `test_device.cpp`                |    1 | yes                 |
| `test_device_unit.cpp`           |    1 | yes                 |
| `test_generated_device_header.c` |    1 | yes                 |
| `test_live.cpp`                  |    1 | yes                 |
| `test_measurement.cpp`           |    0 | no                  |

The exact restoration command was:

```sh
sed -i '/^#error V3_REACHABILITY_SENTINEL$/d' \
  "$build/src/include/amazon_braket_qdmi/device.h"
```

All five recovery compiles then exited `0`. The installed standalone arm used
only the production target and public C consumer:

```sh
cmake --build "$build" --target amazon-braket-qdmi-device
cmake --install "$build" --prefix "$install"
/usr/bin/cc -I"$install/include" \
  -c "$wt/test/test_generated_device_header.c" \
  -o "$install/standalone-generated-header-consumer.o"
```

The baseline compile exited `0`. Adding only
`#error V3_INSTALLED_REACHABILITY_SENTINEL` to installed
`include/amazon_braket_qdmi/device.h` made the identical compile exit `1` with
that sentinel. The mutation and restoration commands were:

```sh
sed -i '26i#error V3_INSTALLED_REACHABILITY_SENTINEL' \
  "$install/include/amazon_braket_qdmi/device.h"
sed -i '/^#error V3_INSTALLED_REACHABILITY_SENTINEL$/d' \
  "$install/include/amazon_braket_qdmi/device.h"
```

The recovery compile exited `0`. Before removal, the worktree was clean,
detached, and exactly at R. The build, install, and worktree directories were
removed, `git worktree prune` succeeded, and no registry entry remained.

```sh
test -z "$(git -C "$wt" status --porcelain=v1 --untracked-files=all)"
test "$(git -C "$wt" rev-parse HEAD)" = "$R"
test -z "$(git -C "$wt" symbolic-ref --quiet HEAD)"
git worktree remove "$wt"
git worktree prune
cmake -E remove_directory "$root"
```

#### V6 eager-import replacement

The resolver used two fresh detached worktrees at R and the supported installed
Python 3.14 nox path. Python was `3.14.7`. This path-independent setup was used
for the baseline and fault arms:

```sh
R=30269f5ea8aaf6c524c583685038b2fc8129bf4d
root=$(mktemp -d)
git worktree add --detach "$root/baseline" "$R"
git worktree add --detach "$root/fault" "$R"
for wt in "$root/baseline" "$root/fault"; do
  test -z "$(git -C "$wt" status --porcelain=v1 --untracked-files=all)"
  test "$(git -C "$wt" rev-parse HEAD)" = "$R"
  test -z "$(git -C "$wt" symbolic-ref --quiet HEAD)"
done
```

From the corresponding worktree, the exact command was:

```sh
uvx nox -s tests-3.14 -- \
  test/python/test_init.py::test_base_install_does_not_import_optional_pennylane
```

The baseline exited `0` with the one selected node passed. The fault added only
a blank line and `from . import pennylane` immediately after the final cleanup
line in `python/amazon/braket/qdmi/__init__.py`:

```diff
 del dist, located_include_dir, resolved_include_dir
+
+from . import pennylane
```

The identical nox command exited `1` and failed exactly
`test_base_install_does_not_import_optional_pennylane`. The uncaught injected
root-package import entered `pennylane.py:27` and ended with
`ImportError: blocked optional import: mqt.core`. There were no other selected
nodes. Removing exactly the blank line and import restored the original file;
`git diff --exit-code` passed, and the baseline command again exited `0`.

#### V10 Python-floor sentinel

The resolver used a separate fresh pair created and checked with the V6 setup,
but selected Python 3.10. This exact command selected only the committed
ten-target semantic oracle:

```sh
uvx nox -s tests-3.10 -- \
  test/python/test_init.py::test_python_floor_precedes_available_optional_plugin
```

The baseline exited `0` with that node passed. The fault removed only these
three guard lines from `python/amazon/braket/qdmi/_pennylane_entrypoint.py`:

```python
if sys.version_info < (3, 11):
    msg = "The Amazon Braket PennyLane integration requires Python 3.11 or newer."
    raise ImportError(msg)
```

The identical command exited `1` and failed exactly
`test_python_floor_precedes_available_optional_plugin` with
`DID NOT RAISE <class 'ImportError'>`; the supplied ten sentinel targets made
the unsupported load succeed, so missing PennyLane could not mask the fault.
There were no other selected nodes. Restoring the exact three lines made the
same command exit `0`, and `git diff --exit-code` passed.

For both V6 and V10, the final cleanup was:

```sh
for wt in "$root/baseline" "$root/fault"; do
  test -z "$(git -C "$wt" status --porcelain=v1 --untracked-files=all)"
  test "$(git -C "$wt" rev-parse HEAD)" = "$R"
  test -z "$(git -C "$wt" symbolic-ref --quiet HEAD)"
  git worktree remove "$wt"
done
git worktree prune
cmake -E remove_directory "$root"
```

#### V11 retained contradictory-option policy

No source mutation was applied. The native resolver ran at
`4e1beface7e11ed22663ac6949eaa0ec0b631130`, the policy-documentation parent; the
only later change through R is the Python-only relocated-consumer test. It used
this fresh detached setup:

```sh
R=4e1beface7e11ed22663ac6949eaa0ec0b631130
root=$(mktemp -d)
wt=$root/worktree
git worktree add --detach "$wt" "$R"
test -z "$(git -C "$wt" status --porcelain=v1 --untracked-files=all)"
test "$(git -C "$wt" rev-parse HEAD)" = "$R"
test -z "$(git -C "$wt" symbolic-ref --quiet HEAD)"
```

The exact contradictory configure was:

```sh
cmake -S "$wt" -B "$root/refusal" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_AMAZON_BRAKET_TESTS=OFF \
  -DBUILD_AMAZON_BRAKET_LIVE_TESTS=ON
```

It exited `1` during configure with
`BUILD_AMAZON_BRAKET_LIVE_TESTS requires BUILD_AMAZON_BRAKET_TESTS`; no later
command ran. The positive policy arm used a new build directory:

```sh
cmake -S "$wt" -B "$root/positive" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_AMAZON_BRAKET_TESTS=ON \
  -DBUILD_AMAZON_BRAKET_LIVE_TESTS=ON \
  -DCMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE=PRE_TEST
cmake --build "$root/positive" \
  --target amazon-braket-qdmi-device-live-test --parallel 4
```

Configure and target-only build both exited `0`; `test_live.cpp` compiled and
the named target linked. Counts for test-discovery commands, CTest commands,
direct test-binary execution, frozen live-environment markers, and AWS-action
text were all zero. The native tuple is therefore unchanged at R. The worktree
was clean, detached at its exact SHA, removed, and absent from the registry. No
live test, live-AWS binary, credential lookup, network mutation, or external
action occurred in any of these resolution experiments.

```sh
test -z "$(git -C "$wt" status --porcelain=v1 --untracked-files=all)"
test "$(git -C "$wt" rev-parse HEAD)" = "$R"
test -z "$(git -C "$wt" symbolic-ref --quiet HEAD)"
git worktree remove "$wt"
git worktree prune
cmake -E remove_directory "$root"
```

## Residual risk

1. **High - release readiness.** External reusable workflows, complete
   wheel/sdist metadata and contents, tags, Runtime/Development manifests,
   v1.1.0 version coherence, released MQT Core dependency, regenerated lock,
   artifact digests, and final-SHA CI correspondence remain cross-scope release
   work.
2. **Medium - cross-platform wheel consumers.** V8 now proves C-header,
   `find_package`, native symbol, runtime-copy, relocated catalogue, and
   isolated relocated-load behavior on Linux aarch64 at exact R. Windows
   repaired-wheel DLL behavior and macOS install names still await CI if those
   platform classes are required. No production layout or filename changed.
3. **Medium - optional import masking.** A0063/A0065 still conflate true
   top-level absence with internal `ImportError`. Split base-only and declared
   integration sessions before narrowing.
4. **Scoped catalogue policy.** V7 applies only to the device-shipped default:
   absent or empty `session` is accepted and every non-empty value is rejected.
   Externally managed HPC-centre catalogues remain outside this repository
   assertion and may carry site-chosen sensitive values.
5. **Intentional retained mechanisms.** V4 retains persistent build isolation,
   V9 retains the single-author and split-license policy, A0072 retains
   `ImportError`, and V11 retains the documented fatal contradictory-option
   policy without `CACHE FORCE`.
6. **Intentional exclusion - live AWS.** No conclusion covers credentials,
   service availability, task submission, result storage, or live devices.

## Found along the way, not blocked by an assertion

- CMake/scikit-build, the catalogue, and Python locator are three authorities
  for one installed layout. A later architecture review should consider one
  build-generated installed-resource description. V8 added semantic consumers;
  it is not that redesign and changed no production layout.
- The audit probe's shared local-project cache allowed stale installed catalogue
  artifacts. Future framework runs must isolate UV cache, scikit-build
  directory, nox environment, temp directory, coverage file, and
  exact-environment artifact coupling per arm. This is a framework/architecture
  handoff, not a product verdict.
- The exact-R installed consumer now covers the Python CMake locator,
  `mqt_copy_qdmi_runtime`, relocation, catalogue parsing, and native symbol
  loading on Linux aarch64. Other release platforms remain CI work.
- Release review should inspect the pinned external workflows and establish one
  release-version/dependency/artifact authority at the final candidate SHA.
- The resolution tests now exercise the public CMake locator through
  `find_package` and the complete PennyLane name-to-class mapping through ten
  entry points.

## Reconciliation

The maintainer decisions were recorded against branch head
`f11351db141f310590d507185ffd2a527d67aa97` before resolution work. This is the
decision-record base, not B, E, or a resolution SHA. The signed decision commit
is `e934b6249c49f63cbf0bb04d352c24c5c8fd03c3`. Resolution was then applied
linearly through the signed commits recorded above and revalidated at exact R.

| Finding/assertion | Decision | Resolution     | Resolving change and exact-R evidence                                                                                                                                                                                                                                                                                                                                                                                                               |
| :---------------- | :------- | :------------- | :-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `V1`              | Accepted | Applied        | `454c203191dc3517e838079a0f90ac6d2656ce28`; retained type and non-empty oracles; exact-R Python matrix passed.                                                                                                                                                                                                                                                                                                                                      |
| `V2`              | Accepted | Applied        | `e08d323f40eb38dd6ba629dcabcdb42feff075b5` then `454c203191dc3517e838079a0f90ac6d2656ce28`; semantic option inventory passed.                                                                                                                                                                                                                                                                                                                       |
| `V3`              | Accepted | Narrowed       | `fa98c4d8b016fcce45495e7d9896cd6d36451b84` added the standalone generated-header consumer; `2232c69e2dce7aae515e0864b7c49ada91552dbb` removed the three direct test-TU includes; hosted C++ lint job `96476973181` then proved include-cleaner requires those direct providers in `test/test_device.cpp`, so the CI follow-up restores the direct includes and makes `test/test_generated_device_header.c` semantically consume a generated symbol. |
| `V4`              | Rejected | Not applicable | Intentional persistent-build and explicit-uv-isolation policy retained; no resolving change.                                                                                                                                                                                                                                                                                                                                                        |
| `V5`              | Accepted | Applied        | `e08d323f40eb38dd6ba629dcabcdb42feff075b5` then `454c203191dc3517e838079a0f90ac6d2656ce28`; shared pre-import floor gate passed on all supported Pythons.                                                                                                                                                                                                                                                                                           |
| `V6`              | Accepted | Applied        | `e08d323f40eb38dd6ba629dcabcdb42feff075b5` then `8b1c42aa0aab02045be45c7c6c1c9dbdcc82e232`; exact-R behavioral oracle killed the eager-import mutant.                                                                                                                                                                                                                                                                                               |
| `V7`              | Accepted | Applied        | `e08d323f40eb38dd6ba629dcabcdb42feff075b5` then `454c203191dc3517e838079a0f90ac6d2656ce28`; shipped default accepts absent/empty and rejects non-empty session values.                                                                                                                                                                                                                                                                              |
| `V8`              | Accepted | Applied        | `e08d323f40eb38dd6ba629dcabcdb42feff075b5`, `99d7c510689365e024f8912a72560288e96d0e7e`, `454c203191dc3517e838079a0f90ac6d2656ce28`, and `30269f5ea8aaf6c524c583685038b2fc8129bf4d`; Linux aarch64 semantic consumer matrix passed with no layout change.                                                                                                                                                                                            |
| `V9`              | Rejected | Not applicable | Intentional single-author, Apache/LLVM-exception, and separately excluded GPL SPANK policy retained; no resolving change.                                                                                                                                                                                                                                                                                                                           |
| `V10/A0072`       | Rejected | Not applicable | Stable `ImportError` domain retained and revalidated at exact R.                                                                                                                                                                                                                                                                                                                                                                                    |
| `V10/A0073`       | Accepted | Applied        | `e08d323f40eb38dd6ba629dcabcdb42feff075b5` then `8b1c42aa0aab02045be45c7c6c1c9dbdcc82e232`; ten-target sentinel oracle killed the floor-bypass mutant without message matching.                                                                                                                                                                                                                                                                     |
| `V11`             | Accepted | Applied        | `4e1beface7e11ed22663ac6949eaa0ec0b631130`; fatal policy retained and documented, no `CACHE FORCE`; exact-R refusal and PRE_TEST compile evidence passed.                                                                                                                                                                                                                                                                                           |

These dispositions are conditionally closed only if PR `#185` is merged without
squashing and preserves the signed audit A, audit formatting, resolution policy,
maintainer decision, replacement-oracle, header-consumer, lazy-import,
installed-consumer, assertion-narrowing, include-cleanup, policy-documentation,
isolated-relocation, and signed reconciliation commits. Until that merge, this
audit records complete exact-R evidence but not final merged closure.

## Progress

- [x] (`2026-08-19`) Scope and ownership fixed at the pinned baseline.
- [x] (`2026-08-19`) Test-blind spec ledger completed by four source classes.
- [x] (`2026-08-19`) Assertion census completed with stable IDs.
- [x] (`2026-08-19`) Prosecution and provenance completed.
- [x] (`2026-08-19`) Fresh defenders reviewed every accused assertion.
- [x] (`2026-08-20`) T1 and T2 experiments completed.
- [x] (`2026-08-20`) Seven selected T3 groups compared in fresh paired
      disposable worktrees at E.
- [x] (`2026-08-20`) Unlock and architecture-altitude analyses completed.
- [x] (`2026-08-20`) Two fresh independent red-team reviews completed.
- [x] (`2026-08-20`) Scope lead adjudicated the evidence.
- [x] (`2026-08-20`) Fresh census amendment added A0072-A0074 and completed
      independent unlock and consumer/evidence-release challenges.
- [x] (`2026-08-20`) Complete Python 3.10 offline replacements `T2-042R1` and
      `T2-043R1`, plus configure-only `T2-044`, completed with exact coupling,
      outcome, and cleanup records; non-adjudicable `T3-008` was excluded.
- [x] (`2026-08-20`) Census has one owner, class, ledger result, and verdict or
      anchor for every assertion, with no Pending census cell.
- [x] (`2026-08-20`) Drift checked against exact current main; original evidence
      preserved.
- [x] (`2026-08-20`) Evidence restoration and detached-at-E checks passed.
- [x] (`2026-08-20`) Maintainer decisions recorded against exact branch head
      `f11351db141f310590d507185ffd2a527d67aa97` before resolution.
- [x] (`2026-08-20`) Self-contained canonical recipes and targeted
      shell/markdown/mapping/diff checks passed; the recipes themselves were not
      executed during authoring.
- [x] (`2026-08-20`) Full `uvx nox -s lint` passed after audit authoring and
      Markdown formatting completed.
- [x] (`2026-08-20`) All accepted assertion changes and replacement oracles were
      applied in signed commits after the signed decision record.
- [x] (`2026-08-20`) Exact-R Linux aarch64 lint, supported Python, non-live C++,
      documentation, V3 reachability, V6/V10 mutant, V8 consumer, and V11 policy
      evidence passed without live AWS activity.
- [x] (`2026-08-20`) The living audit was reconciled without changing B, E,
      assertion IDs, original experiments, decisions, or rejected mechanisms.
- [ ] PR `#185` merged without squashing and preserving every signed audit,
      policy, decision, resolution, and reconciliation commit required above.
