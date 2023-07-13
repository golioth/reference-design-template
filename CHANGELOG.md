<!-- Copyright (c) 2023 Golioth, Inc. -->
<!-- SPDX-License-Identifier: Apache-2.0 -->

# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.1] - 2023-07-14

### Fixed
- Turn on Golioth LED when connected
- Correctly reset `desired` endpoints when `example_int1` is changed by itself
- Fix deadlock behavior when running `set_log_level` RPC multiple times
- Add missing license info
- Removed unused dependencies
- Code formatting
- Typos
- Document forking/merging recommendations

## [1.0.0] - 2023-07-11

### Added
- Initial release
- Support for aludel_mini_v1_sparkfun9160 (custom Golioth board)
- Support for nrf9160dk_nrf9160_ns (commercially available)
