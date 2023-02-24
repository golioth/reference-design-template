/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __APP_STATE_H__
#define __APP_STATE_H__

#include <net/golioth/system_client.h>
#include "app_work.h"

#define APP_STATE_DESIRED_ENDP "desired"
#define APP_STATE_ACTUAL_ENDP  "state"

int app_state_desired_handler(struct golioth_req_rsp *rsp);
void app_state_init(struct golioth_client* state_client);
void app_state_observe(void);
void app_state_update_actual(void);
int app_state_report_ontime(adc_node_t* ch0, adc_node_t* ch1);

#endif /* __APP_STATE_H__ */
