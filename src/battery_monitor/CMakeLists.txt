# Copyright (c) 2023 Golioth, Inc.
# SPDX-License-Identifier: Apache-2.0

zephyr_library_sources_ifdef(CONFIG_ALUDEL_BATTERY_MONITOR battery.c)

zephyr_include_directories(include)
