# Amazon Braket QDMI Device Agent Guide

Follow this guide and the nearest scoped `AGENTS.md`. Use
[`docs/development.md`](docs/development.md) for build, test, and documentation
commands. Keep this file focused on repository-specific guardrails.

## Repository Layout

- `include/amazon-braket-qdmi-device/` and `src/` contain the C++ implementation
  and Amazon Braket-specific QDMI constants.
- `python/amazon/braket/qdmi/` contains the Python package; `config/` contains
  the installed device catalogue.
- `test/` contains GoogleTest and pytest tests, including optional live tests.
- `spank/` contains the optional Slurm plugin under GPL-3.0-or-later. The main
  library uses Apache-2.0 WITH LLVM-exception; keep their source separate.
- `cmake/`, `CMakeLists.txt`, and `pyproject.toml` define builds. Keep generated
  output in `build/` and `docs/_build/`, never in commits.

## Working Principles

- Inspect the worktree before editing and preserve unrelated user changes. Do
  not broaden a task into dependency upgrades or unrelated cleanup.
- Prefer the smallest change that fully solves the problem. Reuse existing code,
  C++20 facilities, and the AWS SDK before adding abstractions or settings.
- Follow documented policy. Neighboring code shows existing practice but does
  not override current instructions.
- Write comments, documentation, tests, diagnostics, and public text for the
  final design. Remove prompts, review chronology, former names, and abandoned
  approaches unless users still need that context.
- Use short sentences, active voice, and one established term per concept.
  Remove words that add no meaning. Preserve the capitalization of QDMI, Amazon
  Braket, QuantumTask, OpenQASM, AWS, S3, and SPANK.
- Add tests for changed behavior or a concrete regression, not provisional
  implementation choices. Keep tests in `test/` and start with focused checks.
- Keep cleanup separate from behavior changes unless correctness requires both.
  Remove obsolete scaffolding and suppressions; retain only narrow, justified
  exceptions.
- Keep documentation at its existing source of truth. Link to it instead of
  repeating the same contract in several pages.
- Group related changes in a concise changelog entry about user-visible
  behavior. Include the PR and every contributing author as
  `([#123]) ([**@username**])` and define their links at the bottom of
  `CHANGELOG.md`.
- Never edit generated or template-managed files. Make template changes in the
  owning repository.
- For test-contract audits, read `.agent/AUDITS.md` and use
  `.agent/audits/TEMPLATE.md`. Keep audit evidence separate from remediation.

## C++ and QDMI Contracts

The project targets C++20. Use `///` for documentation comments and keep API
documentation at the declaration. Explain only details that names and signatures
do not convey. Use C++ casts, not C-style casts.

Preserve the QDMI C ABI, the `AMAZON_BRAKET_` symbol prefix, and the stable
`amazon.braket.default` device ID. No C++ exception may cross the C boundary.
Preserve size-query behavior, handle validation, status codes, and null checks.

AWS SDK initialization and shutdown are process-wide. Preserve singleton and
request lifetimes, session isolation, credential-provider refresh, caches, and
mutex protection. Device ARN region, client region, S3 result location, and
reservation ARN are distinct inputs.

## Build and Validation

Run offline tests by default. Configure with
`-DBUILD_AMAZON_BRAKET_LIVE_TESTS=OFF`; do not reuse a live-test configuration
for an offline run. The development guide lists the native and Python commands.
Use its dedicated Nox session to build documentation and the Doxygen API.

Nox package builds and direct CMake builds share `build/`. Do not run them
concurrently in the same checkout. For dependency-only setup, use
`uv sync --locked --only-group dev`.

- Run `uvx nox -s lint` after each completed batch of changes. Inspect and keep
  only relevant formatter changes, then rerun the check.
- For C++ changes, reproduce `.github/workflows/cpp-linter.yml` against every
  line of each changed file. Changed-line checks alone are insufficient. Report
  existing diagnostics; do not hide them with broad suppressions.
- Run focused Python tests with `uvx nox -s tests-3.14 -- <test path>`. Use
  `uvx nox -s tests minimums` for the supported Python matrix.
- Use Google-style Python docstrings. Preserve the `amazon.braket.qdmi`
  namespace and stable entry point. Fix Ruff and ty findings where possible.
- Build SPANK only when it is in scope. Keep its build output separate from the
  native library build and validate changes with its Docker tests.

### Live AWS Access

Live tests can create paid tasks and S3 objects. Run them only with explicit
authorization for the service, device, Region, and spending scope. Verify the
selected credential source first. Metadata-only catalogue tests also contact AWS
and require authorization.

Use the AWS credential provider chain and short-lived credentials. Never print
or commit credentials, tokens, account IDs, bucket names, or private device
details. Use the automatic regional result bucket unless an explicit destination
override is under test. Keep live and offline test selections separate.

## Git and GitHub

- Keep commits focused. Start subjects with a gitmoji and an imperative verb;
  target 50 characters and never exceed 72. Use the body for constraints and
  non-obvious decisions, not a description of every changed line.
- Sign commits and annotated tags with the configured key. Verify commits with
  `git verify-commit HEAD` before pushing; never bypass a signing failure.
- Preserve human attribution. Use `Assisted-by` for AI assistance, never an AI
  `Co-authored-by` trailer.
- Push, open or merge PRs, and post public text only within the human's explicit
  authorization. Request new authority before expanding that scope.
- Start every agent-authored or agent-edited public text body with
  `🤖 *AI text below* 🤖`; titles are exempt. State how AI assisted the work and
  leave acceptance and responsibility with the human reviewer.
- Use the PR template, appropriate labels, and the requested assignee. Do not
  invent checklist items or attest to human review on the user's behalf.
- Do not work on `good first issue` tasks or post repetitive reviews.
- A push does not authorize CI monitoring. Verify the remote head, report the
  available status, and stop unless monitoring was requested.

## Handoff

Inspect the final diff and worktree. Report what changed and each check run,
including failures or checks that could not run. Distinguish local validation,
live AWS validation, and hosted CI for the final head. Do not describe queued
checks or checks from an older commit as passing validation of the new head.
