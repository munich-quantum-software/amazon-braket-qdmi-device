<!-- Entries in each category are sorted by merge time, with the latest PRs appearing first. -->

# Changelog

All notable changes to this project will be documented in this file.

The format is based on a mixture of [Keep a Changelog] and [Common Changelog].
This project adheres to [Semantic Versioning], with the exception that minor
releases may include breaking changes.

## [Unreleased]

### Added

- ✨ Add an optional GPL-licensed Slurm SPANK plugin for validating Amazon
  Braket devices and injecting QDMI session configuration, including the
  distinct plugin and core library license texts ([#134]) ([**@flowerthrower**],
  [**@burgholzer**])
- ✨ Export the stable Amazon Braket device ID and symbol prefix for MQT Core
  configuration and runtime packaging ([#147]) ([**@burgholzer**])
- ✨ Use the AWS SDK default credential provider chain when no explicit session
  credentials are configured ([#150]) ([**@burgholzer**])

### Changed

- 💥 Prefix Amazon Braket-specific QDMI parameter aliases with the
  `AMAZON_BRAKET_` vendor namespace and add a semantic device-ARN alias that
  retains the existing `BASEURL` mapping ([#149]) ([**@burgholzer**])
- ⬆️ Update QDMI to version 1.3.2 ([#130]) ([**@denialhaag**])

### Fixed

- 🐛 Interpret QDMI job wait timeouts in seconds ([#156]) ([**@burgholzer**])
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
