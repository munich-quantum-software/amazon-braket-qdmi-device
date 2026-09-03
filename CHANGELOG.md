<!-- Entries in each category are sorted by merge time, with the latest PRs appearing first. -->

# Changelog

All notable changes to this project will be documented in this file.

The format is based on a mixture of [Keep a Changelog] and [Common Changelog].
This project adheres to [Semantic Versioning], with the exception that minor
releases may include breaking changes.

## [Unreleased]

### Changed

- ⚡ Prefetch completed task results with a separate bounded worker pool,
  overlapping status checks and S3 downloads with later submissions. Foreground
  reads remain independent and retry through the normal path if prefetch fails.
- ⚡ Submit QuantumTasks asynchronously through a bounded AWS request pool.
  Preserve AWS task IDs, report asynchronous submission errors through job
  status/wait calls, and drain pending requests before freeing their handles.
- ⚡ Reuse session architecture for static property queries and let
  `CreateQuantumTask` validate submissions without redundant `GetDevice`
  requests. Explicit device status and queue queries still refresh AWS data.
- 💥 Drop support for x86 macOS and stop publishing the respective wheels
  ([#194]) ([**@denialhaag**])
- ⬆️ Raise the macOS deployment target to 13.3 ([#194]) ([**@denialhaag**])

## [1.1.1] - 2026-08-26

### Changed

- 📝 Clarify the roles and installation boundaries of the native Runtime,
  optional SPANK plugin, Python adapters, and MQT Core on Slurm clusters
  ([#193]) ([**@burgholzer**])

### Fixed

- 🐛 Make repaired Linux wheels use the host CA trust store for AWS requests and
  include actionable AWS error messages in diagnostics ([#193])
  ([**@burgholzer**])

## [1.1.0] - 2026-08-25

### Added

- ✨ Add a Qiskit backend with Amazon Braket operation mappings, barriers, and
  self-contained OpenQASM 3 ([#192]) ([**@burgholzer**])
- 📝 Document Slurm and SPANK deployment, license-selected workloads, and live
  SV1 execution ([#192]) ([**@burgholzer**])
- ✨ Add PennyLane support for `amazon.braket.default` and every concrete device
  in the installed catalogue ([#168]) ([**@burgholzer**])
- 📝 Add Sphinx and MyST documentation for installation, configuration,
  capabilities, usage, and a standalone native API generated with Doxygen
  without Breathe ([#176]) ([**@burgholzer**])
- ✨ Install a relocatable catalogue containing `amazon.braket.default` and
  convenient definitions for supported backends, and expose each device's
  broader OpenQASM operation set, including measurement, through a custom QDMI
  property ([#171]) ([**@burgholzer**])
- ✨ Retrieve existing Amazon Braket QuantumTasks by their QDMI job IDs ([#160])
  ([**@burgholzer**])
- ✨ Expose current device queue length and queued job position through QDMI,
  refreshing Amazon Braket task status for every position query ([#167])
  ([**@burgholzer**])
- ✨ Expose AWS QuantumTask ARNs as QDMI job IDs ([#157]) ([**@burgholzer**])
- ✨ Add an optional GPL-licensed Slurm SPANK plugin for validating Amazon
  Braket devices and injecting QDMI session configuration, including the
  distinct plugin and core library license texts ([#134]) ([**@flowerthrower**],
  [**@burgholzer**])
- ✨ Export the stable Amazon Braket device ID and symbol prefix for MQT Core
  configuration and runtime packaging ([#147]) ([**@burgholzer**])
- ✨ Use the AWS SDK default credential provider chain when no explicit session
  credentials are configured ([#150]) ([**@burgholzer**])

### Changed

- ⬆️ Update QDMI to 1.3.3, the AWS SDK for C++ to 1.11.876, and MQT Core to
  3.9.1 ([#192]) ([**@burgholzer**])
- 💥 Require Python 3.11, advertise Python 3.15, and update scikit-build-core
  and cibuildwheel configuration ([#192]) ([**@burgholzer**])
- ♻️ Make the optional Slurm SPANK plugin inject the installed catalogue and AWS
  configuration references, with MQT Core performing device preflight ([#173])
  ([**@burgholzer**])
- ♻️ Use the AWS SDK default credential provider chain for all AWS clients and
  automatic regional result bucket, with optional process and per-job overrides
  ([#172]) ([**@burgholzer**])
- ♻️ Parse all gate-model capability documents through one schema-driven
  pipeline while preserving provider-specific calibration enrichment ([#171])
  ([**@burgholzer**])
- 💥 Prefix Amazon Braket-specific QDMI parameter aliases with the
  `AMAZON_BRAKET_` vendor namespace and add a semantic device-ARN alias that
  retains the existing `BASEURL` mapping ([#149]) ([**@burgholzer**])
- ⬆️ Update QDMI to version 1.3.2 ([#130]) ([**@denialhaag**])

### Fixed

- 🐛 Reject malformed or oversized SPANK job environment values instead of
  treating them as absent ([#173]) ([**@burgholzer**])
- 🐛 Make parameter capability probes side-effect free, preserve AWS permission
  errors, and keep all live AWS tests out of the default CTest suite ([#172])
  ([**@burgholzer**])
- 🐛 Preserve numeric site ordering, expose coherence times through QDMI's
  integer duration ABI, keep retired-device status queryable, and prevent C++
  exceptions from crossing the C API boundary ([#171]) ([**@burgholzer**])
- 🐛 Convert Amazon Braket measurement rows to QDMI basis-state order ([#175])
  ([**@burgholzer**])
- 🐛 Validate that QDMI site and operation query handles belong to an
  initialized device session before dereferencing them ([#166])
  ([**@burgholzer**])
- 🐛 Report exact Braket operation signatures, sites, and available fidelities
  through QDMI ([#158]) ([**@burgholzer**])
- 🐛 Interpret QDMI job wait timeouts in seconds ([#156]) ([**@burgholzer**])
- 🐛 Normalize histogram keys as a comma-separated QDMI string ([#159])
  ([**@burgholzer**])

## [1.0.1] - 2026-06-26

### Fixed

- 👷 Fix deployment to PyPI by configuring job to use `ubuntu-latest` instead of
  `ubuntu-slim` ([#117]) ([**@denialhaag**])

## [1.0.0] - 2026-06-26

_This is the initial release of the `amazon-braket-qdmi-device` project._

### Added

- ✨ Add initial version of the Amazon Braket QDMI Device ([**@flowerthrower**],
  [**@burgholzer**])

<!-- Version links -->

[unreleased]: https://github.com/munich-quantum-software/amazon-braket-qdmi-device/compare/v1.1.1...HEAD
[1.1.1]: https://github.com/munich-quantum-software/amazon-braket-qdmi-device/compare/v1.1.0...v1.1.1
[1.1.0]: https://github.com/munich-quantum-software/amazon-braket-qdmi-device/compare/v1.0.1...v1.1.0
[1.0.1]: https://github.com/munich-quantum-software/amazon-braket-qdmi-device/releases/tag/v1.0.1
[1.0.0]: https://github.com/munich-quantum-software/amazon-braket-qdmi-device/releases/tag/v1.0.0

<!-- PR links -->

[#194]: https://github.com/munich-quantum-software/amazon-braket-qdmi-device/pull/194
[#193]: https://github.com/munich-quantum-software/amazon-braket-qdmi-device/pull/193
[#192]: https://github.com/munich-quantum-software/amazon-braket-qdmi-device/pull/192
[#176]: https://github.com/munich-quantum-software/amazon-braket-qdmi-device/pull/176
[#175]: https://github.com/munich-quantum-software/amazon-braket-qdmi-device/pull/175
[#173]: https://github.com/munich-quantum-software/amazon-braket-qdmi-device/pull/173
[#172]: https://github.com/munich-quantum-software/amazon-braket-qdmi-device/pull/172
[#171]: https://github.com/munich-quantum-software/amazon-braket-qdmi-device/pull/171
[#168]: https://github.com/munich-quantum-software/amazon-braket-qdmi-device/pull/168
[#167]: https://github.com/munich-quantum-software/amazon-braket-qdmi-device/pull/167
[#166]: https://github.com/munich-quantum-software/amazon-braket-qdmi-device/pull/166
[#160]: https://github.com/munich-quantum-software/amazon-braket-qdmi-device/pull/160
[#159]: https://github.com/munich-quantum-software/amazon-braket-qdmi-device/pull/159
[#158]: https://github.com/munich-quantum-software/amazon-braket-qdmi-device/pull/158
[#157]: https://github.com/munich-quantum-software/amazon-braket-qdmi-device/pull/157
[#156]: https://github.com/munich-quantum-software/amazon-braket-qdmi-device/pull/156
[#150]: https://github.com/munich-quantum-software/amazon-braket-qdmi-device/pull/150
[#149]: https://github.com/munich-quantum-software/amazon-braket-qdmi-device/pull/149
[#147]: https://github.com/munich-quantum-software/amazon-braket-qdmi-device/pull/147
[#134]: https://github.com/munich-quantum-software/amazon-braket-qdmi-device/pull/134
[#130]: https://github.com/munich-quantum-software/amazon-braket-qdmi-device/pull/130
[#117]: https://github.com/munich-quantum-software/amazon-braket-qdmi-device/pull/117

<!-- Contributor -->

[**@flowerthrower**]: https://github.com/flowerthrower
[**@burgholzer**]: https://github.com/burgholzer
[**@denialhaag**]: https://github.com/denialhaag

<!-- General links -->

[Keep a Changelog]: https://keepachangelog.com/en/1.1.0/
[Common Changelog]: https://common-changelog.org
[Semantic Versioning]: https://semver.org/spec/v2.0.0.html
