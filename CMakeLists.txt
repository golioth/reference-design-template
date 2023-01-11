# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(powermonitor)

target_sources(app PRIVATE src/main.c)
target_sources(app PRIVATE src/app_rpc.c)
target_sources(app PRIVATE src/app_settings.c)
target_sources(app PRIVATE src/app_state.c)
target_sources(app PRIVATE src/app_work.c)
target_sources(app PRIVATE src/dfu/app_dfu.c)
target_sources(app PRIVATE src/libostentus/libostentus.c)
target_sources_ifdef(CONFIG_BOOTLOADER_MCUBOOT app PRIVATE src/dfu/flash.c)
