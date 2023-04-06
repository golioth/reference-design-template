/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
LOG_MODULE_REGISTER(app_rpc, LOG_LEVEL_DBG);

#include <net/golioth/system_client.h>
#include <net/golioth/rpc.h>
#include <qcbor/qcbor.h>
#include <qcbor/qcbor_spiffy_decode.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/sys/reboot.h>

#include "app_rpc.h"

static struct golioth_client *client;

static void reboot_work_handler(struct k_work *work)
{
	/* Sync longs before reboot */
	LOG_PANIC();

	for (int8_t i = 5; i >= 0; i--) {
		if (i) {
			LOG_INF("Rebooting in %d seconds...", i);
		}
		k_sleep(K_SECONDS(1));
	}

	sys_reboot(SYS_REBOOT_COLD);
}
K_WORK_DEFINE(reboot_work, reboot_work_handler);

static enum golioth_rpc_status on_set_log_level(QCBORDecodeContext *request_params_array,
					   QCBOREncodeContext *response_detail_map,
					   void *callback_arg)
{
	double a;
	uint32_t log_level;
	QCBORError qerr;

	QCBORDecode_GetDouble(request_params_array, &a);
	qerr = QCBORDecode_GetError(request_params_array);
	if (qerr != QCBOR_SUCCESS) {
		LOG_ERR("Failed to decode array item: %d (%s)", qerr, qcbor_err_to_str(qerr));
		return GOLIOTH_RPC_INVALID_ARGUMENT;
	}

	log_level = (uint32_t)a;

	if ((log_level < 0) || (log_level > LOG_LEVEL_DBG)) {

		LOG_ERR("Requested log level is out of bounds: %d", log_level);
		return GOLIOTH_RPC_INVALID_ARGUMENT;
	}

	int source_id = 0;
	char *source_name;

	while (1) {
		source_name = (char *)log_source_name_get(0, source_id);
		if (source_name == NULL) {
			break;
		}

		LOG_WRN("Settings %s log level to: %d", source_name, log_level);
		log_filter_set(NULL, 0, source_id, log_level);
		++source_id;
	}

	return GOLIOTH_RPC_OK;
}

static enum golioth_rpc_status on_reboot(QCBORDecodeContext *request_params_array,
					   QCBOREncodeContext *response_detail_map,
					   void *callback_arg)
{
	k_work_submit(&reboot_work);

	return GOLIOTH_RPC_OK;
}

static void rpc_log_if_register_failure(int err)
{
	if (err) {
		LOG_ERR("Failed to register RPC: %d", err);
	}
}

void app_rpc_init(struct golioth_client *state_client)
{
	client = state_client;
	app_register_rpc(client);
}

void app_rpc_observe(void) {
	int err = golioth_rpc_observe(client);
	if (err) {
		LOG_ERR("Failed to observe RPC: %d", err);
	}
}

int app_register_rpc(struct golioth_client *rpc_client)
{
	int err;

	err = golioth_rpc_register(rpc_client, "reboot", on_reboot, NULL);
	rpc_log_if_register_failure(err);

	err = golioth_rpc_register(rpc_client, "set_log_level", on_set_log_level, NULL);
	rpc_log_if_register_failure(err);

	return err;
}
