/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __APP_SETTINGS_H__
#define __APP_SETTINGS_H__

int32_t get_loop_delay_s(void);
enum golioth_settings_status on_setting(
		const char *key,
		const struct golioth_settings_value *value);

#endif /* __APP_SETTINGS_H__ */
