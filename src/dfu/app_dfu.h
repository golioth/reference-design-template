/*
 * Copyright (c) 2021-2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __APP_DFU_H__
#define __APP_DFU_H__

#include <net/golioth/system_client.h>

void app_dfu_init(struct golioth_client *client);
void app_dfu_observe(void);
void app_dfu_report_state_to_golioth(void);

#endif /* __APP_DFU_H__ */
