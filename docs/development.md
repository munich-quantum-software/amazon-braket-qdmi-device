# Development and testing

## C++ tests

Configure and build the test suite with:

```console
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

The hermetic tests use injected AWS clients and need no credentials or network
access. Live tests are disabled unless their explicit opt-in environment
variables are present.

When live Amazon Braket tests are intentionally required, select an AWS SDK
credential source and a pre-provisioned result destination before configuring
the test-specific opt-in:

```console
export AWS_PROFILE=hpc-quantum
export AMZN_BRAKET_TASK_RESULTS_S3_URI=s3://my-braket-results/tasks
```

The test configuration has the following boundaries:

- Device ARNs are public identifiers and are stored directly in test code.
- Credentials are resolved and refreshed by the AWS SDK; tests do not pass them
  through QDMI parameters.
- QuantumTask tests use `AMZN_BRAKET_TASK_RESULTS_S3_URI` so they do not create
  AWS resources.
- `AMAZON_BRAKET_QDMI_RUN_LIVE_CATALOG=1` queries all nine concrete devices. The
  test is serial and does not submit paid QPU tasks.
- `AMAZON_BRAKET_QDMI_TEST_ALLOW_BUCKET_CREATION=1`, with
  `AMZN_BRAKET_TASK_RESULTS_S3_URI` unset, enables a separate SV1 test that can
  create and secure the standard regional bucket.

The final two modes make live AWS calls and must be enabled deliberately.

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
session. The full supported Python matrix is available as `uvx nox -s tests`.

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
