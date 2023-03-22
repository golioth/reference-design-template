/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app_work, LOG_LEVEL_DBG);

#include <net/golioth/system_client.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>

#include "app_work.h"
#include "battery.h"
#include "libostentus/libostentus.h"

static struct golioth_client *client;
/* Add Sensor structs here */

/* Borrowed from samples/boards/nrf/battery/main.c */
static const struct battery_level_point batt_levels[] = {
	/* "Curve" here eyeballed from captured data for the [Adafruit
	 * 3.7v 2000 mAh](https://www.adafruit.com/product/2011) LIPO
	 * under full load that started with a charge of 3.96 V and
	 * dropped about linearly to 3.58 V over 15 hours.  It then
	 * dropped rapidly to 3.10 V over one hour, at which point it
	 * stopped transmitting.
	 *
	 * Based on eyeball comparisons we'll say that 15/16 of life
	 * goes between 3.95 and 3.55 V, and 1/16 goes between 3.55 V
	 * and 3.1 V.
	 */

	{ 10000, 3950 },
	{ 625, 3550 },
	{ 0, 3100 },
};

/* Formatting string for sending sensor JSON to Golioth */
#define JSON_FMT	"{\"counter\":%d}"

/* Callback for LightDB Stream */
static int async_error_handler(struct golioth_req_rsp *rsp)
{
	if (rsp->err) {
		LOG_ERR("Async task failed: %d", rsp->err);
		return rsp->err;
	}
	return 0;
}

int log_battery_info(void)
{
	if (IS_ENABLED(CONFIG_BOARD_ALUDEL_MINI_V1_SPARKFUN9160) ||
			IS_ENABLED(CONFIG_BOARD_ALUDEL_MINI_V1_SPARKFUN9160_NS)) {
		struct sensor_value batt_v = {0, 0};
		struct sensor_value batt_lvl = {0, 0};

		/* Turn on the voltage divider circuit */
		int err = battery_measure_enable(true);

		if (err) {
			LOG_ERR("Failed to enable battery measurement power: %d", err);
			return err;
		}

		/* Read the battery voltage */
		int batt_mV = battery_sample();

		if (batt_mV < 0) {
			LOG_ERR("Failed to read battery voltage: %d", batt_mV);
			return err;
		}

		/* Turn off the voltage divider circuit */
		err = battery_measure_enable(false);
		if (err) {
			LOG_ERR("Failed to disable battery measurement power: %d", err);
			return err;
		}

		sensor_value_from_double(&batt_v, batt_mV / 1000.0);
		sensor_value_from_double(&batt_lvl, battery_level_pptt(batt_mV,
			batt_levels) / 100.0);
		LOG_INF("Battery measurement: voltage=%d.%d V, level=%d.%d",
			batt_v.val1, batt_v.val2, batt_lvl.val1, batt_lvl.val2);
	}
	return 0;
}

/* This will be called by the main() loop */
/* Do all of your work here! */
void app_work_sensor_read(void)
{
	int err;
	char json_buf[256];

	/* Log battery levels */
	log_battery_info();

	/* For this demo, we just send Hello to Golioth */
	static uint8_t counter;

	LOG_INF("Sending hello! %d", counter);

	err = golioth_send_hello(client);
	if (err) {
		LOG_WRN("Failed to send hello!");
	}

	/* Send sensor data to Golioth */
	/* For this demo we just fake it */
	snprintk(json_buf, sizeof(json_buf), JSON_FMT, counter);

	err = golioth_stream_push_cb(client, "sensor",
			GOLIOTH_CONTENT_FORMAT_APP_JSON,
			json_buf, strlen(json_buf),
			async_error_handler, NULL);
	if (err) {
		LOG_ERR("Failed to send sensor data to Golioth: %d", err);
	}

	/* Update slide values on Ostentus
	 *  -values should be sent as strings
	 *  -use the enum from app_work.h for slide key values
	 */
	snprintk(json_buf, 6, "%d", counter);
	slide_set(UP_COUNTER, json_buf, strlen(json_buf));
	snprintk(json_buf, 6, "%d", 255-counter);
	slide_set(DN_COUNTER, json_buf, strlen(json_buf));

	++counter;
}

void app_work_init(struct golioth_client *work_client)
{
	client = work_client;
}

