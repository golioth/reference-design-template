/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __APP_WORK_H__
#define __APP_WORK_H__

#include <stdint.h>
#include <drivers/spi.h>
#include <net/golioth/system_client.h>

extern struct k_sem adc_data_sem;

struct ontime {
	uint64_t ch0;
	uint64_t ch1;
};

typedef struct {
	const struct spi_dt_spec i2c;
	uint8_t ch_num;
	int64_t laston;
	uint64_t runtime;
	uint64_t total_unreported;
	uint64_t total_cloud;
	bool loaded_from_cloud;

} adc_node_t;

void get_ontime(struct ontime *ot);
void app_work_init(struct golioth_client* work_client);
void app_work_on_connect(void);
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
