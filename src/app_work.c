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

static struct golioth_client *client;
/* Add Sensor structs here */

/* Formatting string for sending sensor JSON to Golioth */
#define JSON_FMT	"{\"counter\":%d}"

/* Callback for LightDB Stream */
static int async_error_handler(struct golioth_req_rsp *rsp) {
	if (rsp->err) {
		LOG_ERR("Async task failed: %d", rsp->err);
		return rsp->err;
	}
	return 0;
}

/* Work handler will be called from main via app_work_submit() */
/* Do all of your work here! */
static void sensor_work_handler(struct k_work *work) {
	int err;
	char json_buf[256];

	/* For this demo, we just send Hello to Golioth */
	static int counter = 0;

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
	if (err) LOG_ERR("Failed to send sensor data to Golioth: %d", err);

	++counter;
}
K_WORK_DEFINE(sensor_work, sensor_work_handler);

void app_work_init(struct golioth_client* work_client) {
	client = work_client;
}

void app_work_submit(void) {
	/* Pattern for submitting some sensor work to the system work queue */
	k_work_submit(&sensor_work);
}
