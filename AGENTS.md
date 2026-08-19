# Amazon Braket QDMI Device Agent Guide

This file contains repository-specific instructions for coding agents working on
the Amazon Braket QDMI Device.

## Repository Layout

- `include/amazon-braket-qdmi-device/` contains the public C++ headers and
  Amazon Braket-specific QDMI constants.
- `src/` contains the device implementation, AWS SDK integration, device parser,
  queue handling, and wait logic.
- `python/amazon/braket/qdmi/` contains the Python package and command-line
  entry point. The compiled library is installed into this package by
  scikit-build-core.
- `test/` contains GoogleTest-based C++ tests and pytest-based Python tests.
  Some C++ tests contact Amazon Braket and create remote quantum tasks.
- `spank/` contains the optional Linux-only Slurm SPANK plugin and its Docker
  tests. It is licensed separately under GPL-3.0-or-later.
- `cmake/`, `CMakeLists.txt`, and `pyproject.toml` define native and Python
  builds. Keep generated output in `build/` and do not commit it.

## Working Principles

- Keep changes focused on the assigned task. Do not perform unrelated cleanup,
  broad reformatting, dependency upgrades, or refactors without explicit
  authorization.
- Preserve user changes and inspect the working tree before editing. Never
  discard or overwrite changes outside the task.
- Follow neighboring code and prefer the smallest change that fully solves the
  problem.
- Write comments, documentation, tests, changelog entries, diagnostics, and
  public text for the final design. Do not preserve prompts, review chronology,
  former names, or abandoned approaches unless they remain necessary user-facing
  context.
- Use short, direct sentences and active voice. Use one established term for one
  meaning. Preserve the capitalization of QDMI, Amazon Braket, QuantumTask,
  OpenQASM, AWS, S3, and SPANK.
- Add or update automated tests for every behavioral code change. Run the
  narrowest relevant test first and the complete lint suite before handoff.
- Before auditing tests for spec debt, read `.agent/AUDITS.md` in full and use
  `.agent/audits/TEMPLATE.md`. Keep audit evidence separate from remediation.
- Update `CHANGELOG.md` for user-facing, breaking, or otherwise noteworthy
  changes.
- Never commit or print credentials, access keys, session tokens, account IDs,
  bucket names, private device details, or other secrets. Use the AWS SDK
  credential provider chain or process-local environment variables.
- Do not edit files whose headers say that they are generated from an external
  template. Make those changes in the owning template repository instead.

## Build and Test

### Dependency-only setup

Install development dependencies without building the compiled Python package:

```bash
uv sync --locked --only-group dev
```

Building the Python package also builds the native project. Do that only for an
explicit build or test request.

### C++

Configure and build a release tree with tests:

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_AMAZON_BRAKET_TESTS=ON
cmake --build build --config Release
```

Run offline tests by default. The exclusion keeps every fixture that requires
live AWS access out of the command:

```bash
ctest -C Release --test-dir build --output-on-failure \
  -E '^(AmazonBraketQDMISpecificationTest|AmazonBraketQDMIJobSpecificationTest|AmazonBraketQDMIPerJobS3Test|DeviceParsingTestFixture)\.|^AmazonBraketQDMIWaitTimeoutTest\.JobWaitTimeout$'
```

Run a focused C++ test with a GoogleTest filter, for example:

```bash
./build/test/amazon-braket-qdmi-device-test \
  --gtest_filter='AmazonBraketQDMIOfflineTest.*'
```

The C++ code targets C++20. Preserve the public QDMI C ABI, the `AMAZON_BRAKET_`
symbol prefix, and the stable `amazon.braket.default` device ID. Do not allow
C++ exceptions to cross the C boundary. Keep QDMI size-query behavior, handle
validation, status-code mapping, and null-pointer handling consistent with
neighboring entry points.

AWS SDK initialization and shutdown are process-wide and expensive. Preserve the
existing singleton lifetime, session isolation, credential-provider refresh
behavior, caches, and mutex protection. Treat device ARN region, client region,
S3 result location, and reservation ARN as distinct inputs.

### Online Amazon Braket tests

Online tests can submit remote quantum tasks, write S3 objects, incur charges,
and change external state. Run them only when the human explicitly requests
online validation. Obtain temporary credentials without printing them, for
example:

```bash
set +x
eval "$(aws configure export-credentials --profile braket --format env)"
export AWS_S3_BUCKET="<dedicated-test-results-bucket>"
```

Run the SV1, job, S3, and timeout fixtures in `us-east-1`. Run the three IQM
Garnet metadata fixtures in `eu-north-1`, matching the ARN embedded in those
tests. Use the repository's configured Codex action when available because it
applies this region split.

Never replace short-lived credentials with a committed credentials file. Never
echo credential environment variables. Keep offline and online test filters
complementary when adding or renaming fixtures.

### Python

- Run the supported Python test session with `uvx nox -s tests`; use
  `uvx nox -s tests-3.14` for Python 3.14.
- Run minimum dependency tests with `uvx nox -s minimums` or
  `uvx nox -s minimums-3.14`.
- Pass pytest paths or `-k` expressions after `--` while iterating.
- Do not hand-edit generated version files or files installed into the wheel's
  native-library data directory.

Use Google-style Python docstrings. Preserve the `amazon.braket.qdmi` namespace
package and the stable Python entry point. Prefer fixing diagnostics from Ruff
and ty over suppressing them; document necessary suppressions.

### SPANK

Configure the optional SPANK plugin only for tasks that affect it:

```bash
cmake -S . -B build-spank -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_AMAZON_BRAKET_SPANK_PLUGIN=ON
cmake --build build-spank --target amazon-braket-qdmi-spank --parallel
```

Run its isolated Docker tests with:

```bash
docker build -t amazon-braket-spank-tests -f spank/Dockerfile .
docker run --rm amazon-braket-spank-tests
```

Do not mix the SPANK plugin's GPL-3.0-or-later source with the main library's
Apache-2.0 WITH LLVM-exception license headers.

## Generated Files and Validation

- Do not hand-edit CMake-generated files, built libraries, wheel contents,
  coverage output, or downloaded dependencies.
- Run `uvx nox -s lint` after each completed batch of changes. It runs the full
  `prek` hook set, including formatting, spelling, type, metadata, CMake, and
  C++ checks.
- Inspect the final diff and working-tree status. Remove generated test outputs
  and report every check run, clearly distinguishing passes, failures, and
  checks that could not be run.

## Git and GitHub Actions

- A coding agent may perform coding, Git, and GitHub workflow tasks that a human
  explicitly delegates. Request fresh authorization before changing remote state
  outside that scope.
- Every public text body authored or edited by an agent, including issue and
  pull-request descriptions, comments, and reviews, must visibly include the
  exact disclosure `🤖 *AI text below* 🤖`. Titles are exempt.
- Do not push, open or merge a pull request, post on GitHub, submit a Braket
  task, or otherwise change remote state unless the human explicitly authorizes
  that action.
- Never use an agent to work on an issue labeled `good first issue`, and never
  generate spam, repetitive reviews, or unreviewed contributions.

## Handoff Checklist

- The diff is focused and follows neighboring conventions.
- Behavioral changes have automated coverage and targeted offline tests pass.
- Online tests were run only with explicit authorization and are reported
  separately.
- `uvx nox -s lint` passes.
- User-facing changes update `README.md` and `CHANGELOG.md` when appropriate.
- Generated, secret, template-managed, and unrelated files are absent from the
  diff.
- AI assistance and validation results are reported transparently.
