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

#define LIVE_RUNTIME_FMT "{\"live_runtime\":{\"ch0\":%lld,\"ch1\":%lld}"
#define CUMULATIVE_RUNTIME_FMT ",\"cumulative\":{\"ch0\":%lld,\"ch1\":%lld}}"
#define DEVICE_STATE_FMT LIVE_RUNTIME_FMT "}"
#define DEVICE_STATE_FMT_CUMULATIVE LIVE_RUNTIME_FMT CUMULATIVE_RUNTIME_FMT
#define DEVICE_DESIRED_FMT "{\"example_int0\":%d,\"example_int1\":%d}"

uint32_t _example_int0 = 0;
uint32_t _example_int1 = 1;

static struct golioth_client *client;

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
	app_state_update_actual();
}

static int reset_desired(void) {
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
		return err;
	}
	return 0;
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

int app_state_report_ontime(adc_node_t* ch0, adc_node_t* ch1) {
	int err;
	char json_buf[128];

	if (k_sem_take(&adc_data_sem, K_MSEC(300)) == 0) {

		if (ch0->loaded_from_cloud) {
			snprintk(
					json_buf,
					sizeof(json_buf),
					DEVICE_STATE_FMT_CUMULATIVE,
					ch0->runtime,
					ch1->runtime,
					ch0->total_cloud + ch0->total_unreported,
					ch1->total_cloud + ch1->total_unreported
					);
		} else {
			snprintk(
					json_buf,
					sizeof(json_buf),
					DEVICE_STATE_FMT,
					ch0->runtime,
					ch1->runtime
					);
			/* Cumulative not yet loaded from LightDB State */
			/* Try to load it now */
			app_work_on_connect();
		}

		err = golioth_lightdb_set_cb(client, APP_STATE_ACTUAL_ENDP,
				GOLIOTH_CONTENT_FORMAT_APP_JSON, json_buf, strlen(json_buf),
				async_handler, NULL);

		if (err) {
			LOG_ERR("Failed to send sensor data to Golioth: %d", err);
			k_sem_give(&adc_data_sem);
			return err;
		} else {
			if (ch0->loaded_from_cloud) {
				ch0->total_cloud += ch0->total_unreported;
				ch0->total_unreported = 0;
				ch1->total_cloud += ch1->total_unreported;
				ch1->total_unreported = 0;
			}
		}
		k_sem_give(&adc_data_sem);
	}

	return 0;
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
		reset_desired();
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
		reset_desired();
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

