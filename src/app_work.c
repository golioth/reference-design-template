/*
 * Copyright (c) 2022-2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app_work, LOG_LEVEL_DBG);

#include <net/golioth/system_client.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>

#include "app_work.h"

#ifdef CONFIG_LIB_OSTENTUS
#include <libostentus.h>
#endif
#ifdef CONFIG_ALUDEL_BATTERY_MONITOR
#include "battery_monitor/battery.h"
#endif

static struct golioth_client *client;
/* Add Sensor structs here */
#if DT_HAS_COMPAT_STATUS_OKAY(maxim_max17262)
const struct device *const fuel_gauge_dev = DEVICE_DT_GET_ONE(maxim_max17262);
#endif /* DT_HAS_COMPAT_STATUS_OKAY(maxim_max17262) */

/* Formatting string for sending sensor JSON to Golioth */
#define JSON_FMT "{\"counter\":%d}"

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
#if DT_HAS_COMPAT_STATUS_OKAY(maxim_max17262)
	struct sensor_value voltage, avg_current, temperature;
	float i_avg;

	if (!device_is_ready(fuel_gauge_dev)) {
		printk("sensor: device not ready.\n");
		return;
	}

	sensor_sample_fetch(fuel_gauge_dev);
	sensor_channel_get(fuel_gauge_dev, SENSOR_CHAN_GAUGE_VOLTAGE, &voltage);
	sensor_channel_get(fuel_gauge_dev, SENSOR_CHAN_GAUGE_AVG_CURRENT, &avg_current);
	sensor_channel_get(fuel_gauge_dev, SENSOR_CHAN_GAUGE_TEMP, &temperature);

	i_avg = avg_current.val1 + (avg_current.val2 / 1000000.0);

	LOG_DBG("MAX17262: Voltage: %d.%06d V; Current: %f mA; Temperature: %d.%06d °C",
		voltage.val1, voltage.val2, (double)i_avg, temperature.val1, temperature.val2);
#endif /* DT_HAS_COMPAT_STATUS_OKAY(maxim_max17262) */

	IF_ENABLED(CONFIG_ALUDEL_BATTERY_MONITOR, (
		read_and_report_battery();
		IF_ENABLED(CONFIG_LIB_OSTENTUS, (
			slide_set(BATTERY_V, get_batt_v_str(), strlen(get_batt_v_str()));
			slide_set(BATTERY_LVL, get_batt_lvl_str(), strlen(get_batt_lvl_str()));
		));
	));

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
	LOG_DBG("%s", json_buf);

	err = golioth_stream_push_cb(client, "sensor", GOLIOTH_CONTENT_FORMAT_APP_JSON, json_buf,
				     strlen(json_buf), async_error_handler, NULL);
	if (err) {
		LOG_ERR("Failed to send sensor data to Golioth: %d", err);
	}

	IF_ENABLED(CONFIG_LIB_OSTENTUS, (
		/* Update slide values on Ostentus
		 *  -values should be sent as strings
		 *  -use the enum from app_work.h for slide key values
		 */
		snprintk(json_buf, sizeof(json_buf), "%d", counter);
		slide_set(UP_COUNTER, json_buf, strlen(json_buf));
		snprintk(json_buf, sizeof(json_buf), "%d", 255 - counter);
		slide_set(DN_COUNTER, json_buf, strlen(json_buf));
	));
	++counter;
}

void app_work_init(struct golioth_client *work_client)
{
	client = work_client;
}
