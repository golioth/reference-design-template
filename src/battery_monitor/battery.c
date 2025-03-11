/*
 * Copyright (c) 2018-2019 Peter Bigot Consulting, LLC
 * Copyright (c) 2019-2020 Nordic Semiconductor ASA
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <golioth/client.h>
#include <golioth/stream.h>

#include "battery_monitor/battery.h"
#include "battery_measurement_api.h"
#include "../app_sensors.h"

LOG_MODULE_REGISTER(battery, LOG_LEVEL_DBG);

/* Formatting string for sending battery JSON to Golioth */
#define JSON_FMT "{\"batt_v\":%d.%03d,\"batt_lvl\":%d.%02d}"

#define LABEL_BATTERY "Battery"

char stream_endpoint[] = "battery";

char _batt_v_str[8] = "0.0 V";
char _batt_lvl_str[5] = "none";

static int battery_setup(void)
{
	LOG_INF("Initializing battery measurement");

	int rc = battery_measurement_setup();

	if (rc) {
		LOG_ERR("Battery measurement setup failed: %d", rc);
	}

	return rc;
}

SYS_INIT(battery_setup, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

int read_battery_data(struct battery_data *batt_data)
{
	return battery_read_voltage(batt_data);
}

char *get_batt_v_str(void)
{
	return _batt_v_str;
}

char *get_batt_lvl_str(void)
{
	return _batt_lvl_str;
}

void log_battery_data(void)
{
	LOG_INF("Battery measurement: voltage=%s, level=%s", get_batt_v_str(), get_batt_lvl_str());
}

static void async_error_handler(struct golioth_client *client, enum golioth_status status,
				const struct golioth_coap_rsp_code *coap_rsp_code, const char *path,
				void *arg)
{
	if (status != GOLIOTH_OK) {
		LOG_ERR("Failed to stream battery data: %d", status);
		return;
	}
}

int stream_battery_data(struct golioth_client *client, struct battery_data *batt_data)
{
	int err;
	/* {"batt_v":X.XXX,"batt_lvl":XXX.XX} */
	char json_buf[35];

	/* Send battery data to Golioth */
	snprintk(json_buf, sizeof(json_buf), JSON_FMT, batt_data->battery_voltage_mv / 1000,
		 batt_data->battery_voltage_mv % 1000, batt_data->battery_level_pptt / 100,
		 batt_data->battery_level_pptt % 100);
	LOG_DBG("%s", json_buf);

	err = golioth_stream_set_async(client, stream_endpoint, GOLIOTH_CONTENT_TYPE_JSON, json_buf,
				       strlen(json_buf), async_error_handler, NULL);
	if (err) {
		LOG_ERR("Failed to send battery data to Golioth: %d", err);
	}

	return 0;
}

int read_and_report_battery(struct golioth_client *client)
{
	int err;
	struct battery_data batt_data;

	err = read_battery_data(&batt_data);
	if (err) {
		LOG_ERR("Error reading battery data");
		return err;
	}

	/* Format as global string for easy access */
	snprintk(_batt_v_str, sizeof(_batt_v_str), "%d.%03d V", batt_data.battery_voltage_mv / 1000,
		 batt_data.battery_voltage_mv % 1000);
	snprintk(_batt_lvl_str, sizeof(_batt_lvl_str), "%d%%", batt_data.battery_level_pptt / 100);

	log_battery_data();

	if (golioth_client_is_connected(client)) {
		err = stream_battery_data(client, &batt_data);
		if (err) {
			LOG_ERR("Error streaming battery info");
			return err;
		}
	} else {
		LOG_DBG("No connection available, skipping streaming battery info");
	}

	return 0;
}
