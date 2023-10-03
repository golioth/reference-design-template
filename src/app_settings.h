/*
 * Copyright (c) 2022-2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * Process changes received from the Golioth Settings Service and return a code
 * to Golioth to indicate the success or failure of the update.
 *
 * In this demonstration, the device looks for the `LOOP_DELAY_S` key from the
 * Settings Service and uses this value to determine the delay between sensor
 * reads (the period of sleep in the loop of `main.c`.
 *
 * https://docs.golioth.io/firmware/zephyr-device-sdk/device-settings-service
 */

#ifndef __APP_SETTINGS_H__
#define __APP_SETTINGS_H__

#include <stdint.h>
#include <golioth/client.h>

int32_t get_loop_delay_s(void);
int app_settings_register(struct golioth_client *client);

#endif /* __APP_SETTINGS_H__ */
