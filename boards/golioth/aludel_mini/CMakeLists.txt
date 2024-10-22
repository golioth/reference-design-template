# SPDX-License-Identifier: Apache-2.0

zephyr_library()
zephyr_library_sources(board.c)

set(mcuboot_OVERLAY_CONFIG
    "${CMAKE_CURRENT_LIST_DIR}/aludel_mini_nrf9160_mcuboot.conf"
    CACHE STRING "MCUBOOT configuration")
