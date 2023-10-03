/*
 * Copyright (c) 2022-2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app_settings, LOG_LEVEL_DBG);

#include "main.h"
#include "app_settings.h"

static golioth_client_t client;

static int32_t _loop_delay_s = 60;
#define LOOP_DELAY_S_MAX 43200
#define LOOP_DELAY_S_MIN 0

int32_t get_loop_delay_s(void)
{
	return _loop_delay_s;
}

static golioth_settings_status_t on_loop_delay_setting(int32_t new_value, void *arg)
{
	_loop_delay_s = new_value;
	LOG_INF("Set loop delay to %i seconds", new_value);
	return GOLIOTH_SETTINGS_SUCCESS;
}

int app_settings_init(golioth_client_t settings_client)
{
	client = settings_client;
	int err = app_settings_register(client);
	return err;
}

int app_settings_register(golioth_client_t settings_client)
{
	int err = golioth_settings_register_int_with_range(settings_client,
							   "LOOP_DELAY_S",
							   LOOP_DELAY_S_MIN,
							   LOOP_DELAY_S_MAX,
							   on_loop_delay_setting,
							   NULL);

	if (err) {
		LOG_ERR("Failed to register settings callback: %d", err);
	}

	return err;
}
