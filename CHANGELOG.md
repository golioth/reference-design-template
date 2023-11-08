<!-- Copyright (c) 2023 Golioth, Inc. -->
<!-- SPDX-License-Identifier: Apache-2.0 -->

# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [template_v1.2.0] - 2023-11-08

### Added
- GitHub workflow to create draft release and add compiled binaries to it.

### Changed
- Update to most recent Golioth Zephyr SDK release v0.8.0 which uses:
  - nRF Connect SDK v2.5.0(NCS)
  - Zephyr v3.5.0
- Upgrade `golioth/golioth-zephyr-boards` dependency to [`v1.0.1`](https://github.com/golioth/golioth-zephyr-boards/tree/v1.0.1)
- Dependencies use https instead of ssh GitHub URLs
- libostentus removed from code base and included as a Zephyr module

### Fixed
- Fix build error when `CONFIG_LIB_OSTENTUS=n` on the `aludel_mini_v1_sparkfun9160` board.

## [template_v1.1.0] - 2023-08-18

### Breaking Changes
- Golioth services (RPC, Settings, etc.) now use zcbor instead of qcbor
- golioth-zephyr-boards repo now included as a module
  - Remove `golioth-boards` directory
  - Remove `golioth-boards` from .gitignore
  - Remove `zephyr/module.yml`
- zephyr-network-info repo no included as a module
  - Remove `src/network_info` directory
  - Remove `network_info/` from .gitignore
  - Remove `add_subdirectory(src/network_info)` from CMakeLists.txt

### Changed
- update to most recent Golioth Zephyr SDK release v0.7.1 which uses:
  - nRF Connect SDK v2.4.1 (NCS)
  - Zephyr v3.3.99-ncs1-1
- update DFU flash.c/flash.h files
- update board config for nrf9160dk_nrf9160_ns and aludel_mini_v1_sparkfun9160_ns
- update LTE link control: Disable samples/common link control and use in-app link control to start
  connection asynchronously

### Fixed
- main.c: return int from main()
- battery_monitor.c: use void as initialization param
- main.c: use LOG_ERR() instead of printk() for button errors

## [template_v1.0.1] - 2023-07-14

### Fixed
- Turn on Golioth LED when connected
- Correctly reset `desired` endpoints when `example_int1` is changed by itself
- Fix deadlock behavior when running `set_log_level` RPC multiple times
- Add missing license info
- Removed unused dependencies
- Code formatting
- Typos
- Document forking/merging recommendations

## [template_v1.0.0] - 2023-07-11

### Added
- Initial release
- Support for aludel_mini_v1_sparkfun9160 (custom Golioth board)
- Support for nrf9160dk_nrf9160_ns (commercially available)
