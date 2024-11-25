/*
 * Copyright (c) 2022-2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app_state, LOG_LEVEL_DBG);

#include <golioth/client.h>
#include <golioth/lightdb_state.h>
#include <zephyr/data/json.h>
#include <zephyr/kernel.h>
#include "json_helper.h"

#include "app_state.h"
#include "app_sensors.h"

#define DEVICE_STATE_FMT "{\"example_int0\":%d,\"example_int1\":%d}"

uint32_t _example_int0;
uint32_t _example_int1 = 1;

static struct golioth_client *client;

static void async_handler(struct golioth_client *client,
			  enum golioth_status status,
			  const struct golioth_coap_rsp_code *coap_rsp_code,
			  const char *path,
			  void *arg)
{
	if (status != GOLIOTH_OK) {
		LOG_WRN("Failed to set state: %d", status);
		return;
	}

	LOG_DBG("State successfully set");
}

int app_state_reset_desired(void)
{
	LOG_INF("Resetting \"%s\" LightDB State endpoint to defaults.", APP_STATE_DESIRED_ENDP);

	char sbuf[sizeof(DEVICE_STATE_FMT) + 4]; /* space for two "-1" values */

	snprintk(sbuf, sizeof(sbuf), DEVICE_STATE_FMT, -1, -1);

	int err;
	err = golioth_lightdb_set_async(client,
					APP_STATE_DESIRED_ENDP,
					GOLIOTH_CONTENT_TYPE_JSON,
					sbuf,
					strlen(sbuf),
					async_handler,
					NULL);
	if (err) {
		LOG_ERR("Unable to write to LightDB State: %d", err);
	}
	return err;
}

int app_state_update_actual(void)
{

	char sbuf[sizeof(DEVICE_STATE_FMT) + 10]; /* space for uint16 values */

	snprintk(sbuf, sizeof(sbuf), DEVICE_STATE_FMT, _example_int0, _example_int1);

	int err;

	err = golioth_lightdb_set_async(client,
					APP_STATE_ACTUAL_ENDP,
					GOLIOTH_CONTENT_TYPE_JSON,
					sbuf,
					strlen(sbuf),
					async_handler,
					NULL);

	if (err) {
		LOG_ERR("Unable to write to LightDB State: %d", err);
	}
	return err;
}

static void app_state_desired_handler(struct golioth_client *client, enum golioth_status status,
				      const struct golioth_coap_rsp_code *coap_rsp_code,
				      const char *path, const uint8_t *payload, size_t payload_size,
				      void *arg)
{
	int err = 0;
	int ret;

	if (status != GOLIOTH_OK) {
		LOG_ERR("Failed to receive '%s' endpoint: %d", APP_STATE_DESIRED_ENDP, status);
		return;
	}

	LOG_HEXDUMP_DBG(payload, payload_size, APP_STATE_DESIRED_ENDP);

	struct app_state parsed_state;

	ret = json_obj_parse((char *)payload, payload_size, app_state_descr,
			     ARRAY_SIZE(app_state_descr), &parsed_state);

	if (ret < 0) {
		LOG_ERR("Error parsing desired values: %d", ret);
		app_state_reset_desired();
		return;
	}

	uint8_t desired_processed_count = 0;
	uint8_t state_change_count = 0;

	if (ret & 1 << 0) {
		/* Process example_int0 */
		if ((parsed_state.example_int0 >= 0) && (parsed_state.example_int0 < 65536)) {
			LOG_DBG("Validated desired example_int0 value: %d",
				parsed_state.example_int0);
			if (_example_int0 != parsed_state.example_int0) {
				_example_int0 = parsed_state.example_int0;
				++state_change_count;
			}
			++desired_processed_count;
		} else if (parsed_state.example_int0 == -1) {
			LOG_DBG("No change requested for example_int0");
		} else {
			LOG_ERR("Invalid desired example_int0 value: %d",
				parsed_state.example_int0);
			++desired_processed_count;
		}
	}
	if (ret & 1 << 1) {
		/* Process example_int1 */
		if ((parsed_state.example_int1 >= 0) && (parsed_state.example_int1 < 65536)) {
			LOG_DBG("Validated desired example_int1 value: %d",
				parsed_state.example_int1);
			if (_example_int1 != parsed_state.example_int1) {
				_example_int1 = parsed_state.example_int1;
				++state_change_count;
			}
			++desired_processed_count;
		} else if (parsed_state.example_int1 == -1) {
			LOG_DBG("No change requested for example_int1");
		} else {
			LOG_ERR("Invalid desired example_int1 value: %d",
				parsed_state.example_int1);
			++desired_processed_count;
		}
	}

	if (state_change_count) {
		/* The state was changed, so update the state on the Golioth servers */
		err = app_state_update_actual();
	}
	if (desired_processed_count) {
		/* We processed some desired changes to return these to -1 on the server
		 * to indicate the desired values were received.
		 */
		err = app_state_reset_desired();
	}

	if (err) {
		LOG_ERR("Failed to update cloud state: %d", err);
	}
}

int app_state_observe(struct golioth_client *state_client)
{
	int err;

	client = state_client;

	err = golioth_lightdb_observe_async(client,
					    APP_STATE_DESIRED_ENDP,
					    GOLIOTH_CONTENT_TYPE_JSON,
					    app_state_desired_handler,
					    NULL);
	if (err) {
		LOG_WRN("failed to observe lightdb path: %d", err);
		return err;
	}

	/* This will only run once. It updates the actual state of the device
	 * with the Golioth servers. Future updates will be sent whenever
	 * changes occur.
	 */
	err = app_state_update_actual();

	return err;
}
