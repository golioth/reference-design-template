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
#include "libostentus/libostentus.h"

#ifdef CONFIG_ALUDEL_BATTERY_MONITOR
#include "battery_monitor/battery.h"
#endif

static struct golioth_client *client;
/* Add Sensor structs here */

/* Formatting string for sending sensor JSON to Golioth */
#define JSON_FMT "{\"counter\":%d,\"batt_v\":%f,\"batt_lvl\":%f}"

/* Callback for LightDB Stream */
static int async_error_handler(struct golioth_req_rsp *rsp)
{
	if (rsp->err) {
		LOG_ERR("Async task failed: %d", rsp->err);
		return rsp->err;
	}
	return 0;
}

/* This will be called by the main() loop */
/* Do all of your work here! */
void app_work_sensor_read(void)
{
	int err;
	char json_buf[256];
	struct sensor_value batt_v = {-1, 0};
	struct sensor_value batt_lvl = {-1, 0};
	IF_ENABLED(CONFIG_ALUDEL_BATTERY_MONITOR, (char batt_v_str[7]; char batt_lvl_str[5];));

	/* Log battery levels if possible */
	IF_ENABLED(CONFIG_ALUDEL_BATTERY_MONITOR,
		   (read_battery_info(&batt_v, &batt_lvl);

		    LOG_INF("Battery measurement: voltage=%.2f V, level=%d%%",
			    sensor_value_to_double(&batt_v), batt_lvl.val1);));

	/* For this demo, we just send Hello to Golioth */
	static uint8_t counter;

	LOG_INF("Sending hello! %d", counter);

	err = golioth_send_hello(client);
	if (err) {
		LOG_WRN("Failed to send hello!");
	}

	/* Send sensor data to Golioth */
	/* For this demo we just fake it */
	snprintk(json_buf, sizeof(json_buf), JSON_FMT, counter,
		sensor_value_to_double(&batt_v),
		sensor_value_to_double(&batt_lvl));
	LOG_DBG("%s", json_buf);

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
	IF_ENABLED(CONFIG_ALUDEL_BATTERY_MONITOR,
		   (snprintk(batt_v_str, sizeof(batt_v_str), "%.2f V",
			     sensor_value_to_double(&batt_v));
		    slide_set(BATTERY_V, batt_v_str, strlen(batt_v_str));
		    snprintk(batt_lvl_str, sizeof(batt_lvl_str), "%d%%", batt_lvl.val1);
		    slide_set(BATTERY_LVL, batt_lvl_str, strlen(batt_lvl_str));));

	++counter;
}

void app_work_init(struct golioth_client *work_client)
{
	client = work_client;
}

