/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app_state, LOG_LEVEL_DBG);

#include <net/golioth/system_client.h>
#include <zephyr/data/json.h>
#include "json_helper.h"

#include "app_state.h"
#include "app_work.h"

#define DEVICE_STATE_FMT "{\"ontime_ch0\":%lld,\"ontime_ch1\":%lld}"
#define DEVICE_DESIRED_FMT "{\"example_int0\":%d,\"example_int1\":%d}"

uint32_t _example_int0 = 0;
uint32_t _example_int1 = 1;

static struct golioth_client *client;

static K_SEM_DEFINE(update_actual, 0, 1);

static struct ontime ot;

static int async_handler(struct golioth_req_rsp *rsp)
{
	if (rsp->err) {
		LOG_WRN("Failed to set state: %d", rsp->err);
		return rsp->err;
	}

	LOG_DBG("State successfully set");

	return 0;
}

void app_state_init(struct golioth_client* state_client) {
	client = state_client;
	k_sem_give(&update_actual);
}

static void reset_desired_state(void) {
	LOG_INF("Resetting \"%s\" LightDB State endpoint to defaults.",
			APP_STATE_DESIRED_ENDP
			);

	char sbuf[strlen(DEVICE_STATE_FMT)+8]; /* small bit of extra space */
	snprintk(sbuf, sizeof(sbuf), DEVICE_STATE_FMT, -1, -1);

	int err;
	err = golioth_lightdb_set_cb(client, APP_STATE_DESIRED_ENDP,
			GOLIOTH_CONTENT_FORMAT_APP_JSON, sbuf, strlen(sbuf),
			async_handler, NULL);
	if (err) {
		LOG_ERR("Unable to write to LightDB State: %d", err);
	}
}

void app_state_update_actual(void) {

	char sbuf[strlen(DEVICE_STATE_FMT)+8]; /* small bit of extra space */
	snprintk(sbuf, sizeof(sbuf), DEVICE_STATE_FMT, ot.ch0, ot.ch1);

	int err;
	err = golioth_lightdb_set_cb(client, APP_STATE_ACTUAL_ENDP,
			GOLIOTH_CONTENT_FORMAT_APP_JSON, sbuf, strlen(sbuf),
			async_handler, NULL);
	if (err) {
		LOG_ERR("Unable to write to LightDB State: %d", err);
	}
}

void app_state_observe(void) {
	int err = golioth_lightdb_observe_cb(client, APP_STATE_DESIRED_ENDP,
			GOLIOTH_CONTENT_FORMAT_APP_JSON, app_state_desired_handler, NULL);
	if (err) {
	   LOG_WRN("failed to observe lightdb path: %d", err);
	}
}

void app_state_update_actual(void) {
	get_ontime(&ot);
	if (k_sem_take(&update_actual, K_NO_WAIT) == 0) {
		k_work_submit(&update_actual_state_work);
	}
}

int app_state_desired_handler(struct golioth_req_rsp *rsp) {
	if (rsp->err) {
		LOG_ERR("Failed to receive '%s' endpoint: %d", APP_STATE_DESIRED_ENDP, rsp->err);
		return rsp->err;
	}

	LOG_HEXDUMP_DBG(rsp->data, rsp->len, APP_STATE_DESIRED_ENDP);

	struct template_state parsed_state;

	int ret = json_obj_parse((char *)rsp->data, rsp->len,
			template_state_descr, ARRAY_SIZE(template_state_descr),
			&parsed_state);

	if (ret < 0) {
		LOG_ERR("Error parsing desired values: %d", ret);
		reset_desired_state();
		return 0;
	}

	uint8_t desired_processed_count = 0;
	uint8_t state_change_count = 0;
	if (ret & 1<<0) {
		// Process example_int0
		if ((parsed_state.example_int0 >= 0) && (parsed_state.example_int0 < 10000)) {
			LOG_DBG("Validated desired example_int0 value: %d", parsed_state.example_int0);
			_example_int0 = parsed_state.example_int0;
			++desired_processed_count;
			++state_change_count;
		}
		else if (parsed_state.example_int0 == -1) {
			LOG_DBG("No change requested for example_int0");
		}
		else {
			LOG_ERR("Invalid desired example_int0 value: %d", parsed_state.example_int0);
			++desired_processed_count;
		}
	}
	if (ret & 1<<1) {
		// Process example_int1
		if ((parsed_state.example_int1 >= 0) && (parsed_state.example_int1 < 10000)) {
			LOG_DBG("Validated desired example_int1 value: %d", parsed_state.example_int1);
			_example_int1 = parsed_state.example_int1;
			++desired_processed_count;
			++state_change_count;
		}
		else if (parsed_state.example_int1 == -1) {
			LOG_DBG("No change requested for example_int1");
		}
		else {
			LOG_ERR("Invalid desired example_int1 value: %d", parsed_state.example_int1);
			++desired_processed_count;
		}
	}

	if (state_change_count) {
		// The state was changed, so update the state on the Golioth servers
		app_state_update_actual();
	}
	if (desired_processed_count) {
		// We processed some desired changes to return these to -1 on the server
		// to indicate the desired values were received.
		reset_desired_state();
	}
	return 0;
}

void app_state_observe(void) {
	int err = golioth_lightdb_observe_cb(client, APP_STATE_DESIRED_ENDP,
			GOLIOTH_CONTENT_FORMAT_APP_JSON, app_state_desired_handler, NULL);
	if (err) {
	   LOG_WRN("failed to observe lightdb path: %d", err);
	}

	// This will only run when we first connect. It updates the actual state of
	// the device with the Golioth servers. Future updates will be sent whenever
	// changes occur.
	if (k_sem_take(&update_actual, K_NO_WAIT) == 0) {
		app_state_update_actual();
	}
}

