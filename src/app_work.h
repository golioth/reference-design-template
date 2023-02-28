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
	uint8_t i2c_addr;
} adc_node_t;

void get_ontime(struct ontime *ot);
int reset_cumulative_totals(void);
void app_work_init(struct golioth_client* work_client);
void app_work_on_connect(void);
void app_work_sensor_read(void);

/**
 * Each Ostentus slide needs a unique key. You may add additional slides by
 * inserting elements with the name of your choice to this enum.
 */
typedef enum {
    CH0_CURRENT,
    CH0_POWER,
    CH0_VOLTAGE,
    CH1_CURRENT,
    CH1_POWER,
    CH1_VOLTAGE
}slide_key;

/* Ostentus slide labels */
#define CH0_CUR_LABEL "Current ch0"
#define CH0_VOL_LABEL "Voltage ch0"
#define CH0_POW_LABEL "Power ch0"
#define CH1_CUR_LABEL "Current ch1"
#define CH1_VOL_LABEL "Voltage ch1"
#define CH1_POW_LABEL "Power ch1"

#endif /* __APP_WORK_H__ */
