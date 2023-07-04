/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __APP_SETTINGS_H__
#define __APP_SETTINGS_H__

#include <stdint.h>
#include <net/golioth/system_client.h>

int32_t get_loop_delay_s(void);
int app_settings_init(struct golioth_client *state_client);
int app_settings_observe(void);
int app_settings_register(struct golioth_client *settings_client);

#endif /* __APP_SETTINGS_H__ */
