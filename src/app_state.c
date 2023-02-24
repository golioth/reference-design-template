/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app_state, LOG_LEVEL_DBG);

#include <net/golioth/system_client.h>

#include "app_state.h"
#include "app_work.h"
#include <qcbor/qcbor.h>
#include <qcbor/qcbor_decode.h>
#include <qcbor/qcbor_spiffy_decode.h>

#define LIVE_RUNTIME_FMT "{\"live_runtime\":{\"ch0\":%lld,\"ch1\":%lld}"
#define CUMULATIVE_RUNTIME_FMT ",\"cumulative\":{\"ch0\":%lld,\"ch1\":%lld}}"
#define DEVICE_STATE_FMT LIVE_RUNTIME_FMT "}"
#define DEVICE_STATE_FMT_CUMULATIVE LIVE_RUNTIME_FMT CUMULATIVE_RUNTIME_FMT
#define DESIRED_RESET_KEY "reset_cumulative"

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

	QCBOREncodeContext encode_ctx;
	UsefulBuf_MAKE_STACK_UB(useful_buf, 48);
	QCBOREncode_Init(&encode_ctx, useful_buf);
	QCBOREncode_OpenMap(&encode_ctx);
	QCBOREncode_AddBoolToMap(&encode_ctx, DESIRED_RESET_KEY, false);
	QCBOREncode_CloseMap(&encode_ctx);
	UsefulBufC EncodedCBOR;
	QCBORError qcErr = QCBOREncode_Finish(&encode_ctx, &EncodedCBOR);

	if (qcErr) {
		LOG_ERR("Error encoding CBOR to reset deired endpoint: %d", qcErr);
		return -qcErr;
	}

	int err;
	err = golioth_lightdb_set_cb(
			client,
			APP_STATE_DESIRED_ENDP,
			GOLIOTH_CONTENT_FORMAT_APP_CBOR,
			EncodedCBOR.ptr,
			EncodedCBOR.len,
			async_handler,
			NULL
			);
	if (err) {
		LOG_ERR("Unable to write to LightDB State: %d", err);
		return err;
	}
	return 0;

}

void app_state_observe(void) {
	int err = golioth_lightdb_observe_cb(client, APP_STATE_DESIRED_ENDP,
			GOLIOTH_CONTENT_FORMAT_APP_CBOR, app_state_desired_handler, NULL);
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

	if ((rsp->len == 1) && (rsp->data[0] == 0xf6)) {
		/* This is `null` in CBOR */
		LOG_ERR("Endpoint is null, resetting desired to defaults");
		reset_desired();
		return -EFAULT;
	}

	QCBORDecodeContext decode_ctx;
	bool reset_cumulative;
	UsefulBufC payload = { rsp->data, rsp->len };

	QCBORDecode_Init(&decode_ctx, payload, QCBOR_DECODE_MODE_NORMAL);
	QCBORDecode_EnterMap(&decode_ctx, NULL);
	QCBORDecode_GetBoolInMapSZ(&decode_ctx, DESIRED_RESET_KEY, &reset_cumulative);
	QCBORDecode_ExitMap(&decode_ctx);

	int err = QCBORDecode_Finish(&decode_ctx);
	if (err) {
		LOG_ERR("QCBOR decode error; resetting desired endpoint values.");
		LOG_ERR("QCBOR error code %d: %s", err, qcbor_err_to_str(err));
		reset_desired();
		return -ENOTSUP;
	} else {
		LOG_DBG("Decoded: reset_cumulative == %s",
				reset_cumulative ? "true" : "false"
				);
		if (reset_cumulative) {
			LOG_INF("Request to reset cumulative values received. Processing now.");
			reset_cumulative_totals();
			reset_desired();
		}
	}
	return 0;
}
