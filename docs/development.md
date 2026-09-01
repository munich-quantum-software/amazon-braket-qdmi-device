# Development and testing

## C++ tests

Configure and build the test suite with:

```console
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

The hermetic tests use injected AWS clients and need no credentials or network
access. CMake does not register tests that access AWS unless
`BUILD_AMAZON_BRAKET_LIVE_TESTS=ON` is set explicitly.

When live Amazon Braket tests are intentionally required, select an AWS SDK
credential source before configuring the test-specific opt-in:

```console
cmake -S . -B build-live -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_AMAZON_BRAKET_LIVE_TESTS=ON
cmake --build build-live --config Release
ctest --test-dir build-live -L amazon-braket-live --output-on-failure
```

The example relies on an already selected AWS SDK credential source, such as
`AWS_PROFILE=hpc-quantum` or a workload role. Temporary environment credentials
must include `AWS_SESSION_TOKEN`.

The test configuration has the following boundaries:

- Device ARNs are public identifiers and are stored directly in test code.
- Credentials are resolved and refreshed by the AWS SDK; tests do not pass them
  through QDMI parameters.
- QuantumTask tests use the automatic standard regional result bucket.
- `AMAZON_BRAKET_QDMI_RUN_LIVE_CATALOG=1` queries all nine concrete devices. The
  test is serial and does not submit paid QPU tasks.

The catalogue mode makes additional live AWS calls and must be enabled
deliberately. The older SV1 and device integration fixtures are registered under
the same `amazon-braket-live` label only in the live CMake configuration.

To test a pre-provisioned destination instead, set
`AMZN_BRAKET_TASK_RESULTS_S3_URI`. The dedicated per-job S3 fixture exercises
the job-parameter override.

## Python tests

Run a single supported Python version during development:

```console
uvx nox -s tests-3.14
```

Run the minimum direct dependency versions with:

```console
uvx nox -s minimums-3.14
```

The regular Python tests build the native package through the repository's nox
session and exercise the optional PennyLane and Qiskit integrations. The tested
Python 3.11 through 3.14 matrix is available as `uvx nox -s tests`. Python 3.15
is advertised for forward compatibility but is not tested yet.

The PennyLane SV1 smoke test is opt-in because it submits paid QuantumTasks. To
run it with any credential source supported by the AWS SDK, use:

```console
AWS_PROFILE=hpc-quantum AMAZON_BRAKET_PENNYLANE_LIVE=1 \
  uvx nox -s tests-3.14 -- \
  test/python/test_pennylane_qaoa.py -k cost_capped_on_sv1
```

The Python version, platform, and explicit-secret restrictions apply only to the
dedicated CI lane. Local runs use the standard AWS credential provider chain.

## Documentation

Build the Sphinx and MyST documentation and standalone Doxygen native API only
through the dedicated nox session. Doxygen must be available on `PATH`:

```console
uvx nox --non-interactive -s docs
```

Use the same session for link validation:

```console
uvx nox --non-interactive -s docs -- -b linkcheck
```

Generated HTML is written to `docs/_build/html`.

## Repository structure

```text
amazon-braket-qdmi-device/
├── cmake/       CMake package configuration and dependencies
├── config/      Installed device catalogue
├── docs/        Sphinx and MyST documentation
├── include/     Amazon Braket-specific public and internal headers
├── python/      Python package and installed-artifact discovery
├── spank/       Optional GPL-licensed Slurm integration
├── src/         QDMI device implementation and schema parser
└── test/        Hermetic, integration, and Python tests
```
