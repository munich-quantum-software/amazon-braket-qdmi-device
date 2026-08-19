<!-- Entries in each category are sorted by merge time, with the latest PRs appearing first. -->

# Changelog

All notable changes to this project will be documented in this file.

The format is based on a mixture of [Keep a Changelog] and [Common Changelog].
This project adheres to [Semantic Versioning], with the exception that minor
releases may include breaking changes.

## [Unreleased]

### Added

- ✨ Install a relocatable catalogue containing `amazon.braket.default` and
  convenient definitions for supported backends, and expose each device's
  broader OpenQASM operation set through a custom QDMI property ([#171])
  ([**@burgholzer**])
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

- ♻️ Parse all gate-model capability documents through one schema-driven
  pipeline while preserving provider-specific calibration enrichment ([#171])
  ([**@burgholzer**])
- 💥 Prefix Amazon Braket-specific QDMI parameter aliases with the
  `AMAZON_BRAKET_` vendor namespace and add a semantic device-ARN alias that
  retains the existing `BASEURL` mapping ([#149]) ([**@burgholzer**])
- ⬆️ Update QDMI to version 1.3.2 ([#130]) ([**@denialhaag**])

### Fixed

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
- 🐛 Isolate scikit-build output from the direct CMake build tree ([#147])
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

[unreleased]: https://github.com/munich-quantum-software/amazon-braket-qdmi-device/compare/v1.0.1...HEAD
[1.0.1]: https://github.com/munich-quantum-software/amazon-braket-qdmi-device/releases/tag/v1.0.1
[1.0.0]: https://github.com/munich-quantum-software/amazon-braket-qdmi-device/releases/tag/v1.0.0

<!-- PR links -->

[#171]: https://github.com/munich-quantum-software/amazon-braket-qdmi-device/pull/171
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
[#175]: https://github.com/munich-quantum-software/amazon-braket-qdmi-device/pull/175

<!-- Contributor -->

[**@flowerthrower**]: https://github.com/flowerthrower
[**@burgholzer**]: https://github.com/burgholzer
[**@denialhaag**]: https://github.com/denialhaag

<!-- General links -->

[Keep a Changelog]: https://keepachangelog.com/en/1.1.0/
[Common Changelog]: https://common-changelog.org
[Semantic Versioning]: https://semver.org/spec/v2.0.0.html
