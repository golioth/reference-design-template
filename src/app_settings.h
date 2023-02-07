/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __APP_SETTINGS_H__
#define __APP_SETTINGS_H__

int32_t get_loop_delay_s(void);
uint16_t get_adc_floor(uint8_t ch_num);
int app_register_settings(struct golioth_client *settings_client);

#endif /* __APP_SETTINGS_H__ */
