/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __APP_WORK_H__
#define __APP_WORK_H__

struct ontime {
	uint64_t ch0;
	uint64_t ch1;
};

void get_ontime(struct ontime *ot);
void app_work_init(struct golioth_client* work_client);
void app_work_sensor_read(void);

/**
 * Each Ostentus slide needs a unique key. You may add additional slides by
 * inserting elements with the name of your choice to this enum.
 */
typedef enum {
    UP_COUNTER,
    DN_COUNTER
}slide_key;

#endif /* __APP_WORK_H__ */
