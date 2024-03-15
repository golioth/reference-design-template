/*
 * Copyright (c) 2022-2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
LOG_MODULE_REGISTER(app_rpc, LOG_LEVEL_DBG);

#include <golioth/client.h>
#include <golioth/rpc.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/sys/reboot.h>

#include <network_info.h>
#include "app_rpc.h"

static void reboot_work_handler(struct k_work *work)
{
	for (int8_t i = 5; i >= 0; i--) {
		if (i) {
			LOG_INF("Rebooting in %d seconds...", i);
		}
		k_sleep(K_SECONDS(1));
	}

	/* Sync logs before reboot */
	LOG_PANIC();

	sys_reboot(SYS_REBOOT_COLD);
}
K_WORK_DEFINE(reboot_work, reboot_work_handler);

static enum golioth_rpc_status on_get_network_info(zcbor_state_t *request_params_array,
						   zcbor_state_t *response_detail_map,
						   void *callback_arg)
{
	network_info_add_to_map(response_detail_map);

	return GOLIOTH_RPC_OK;
}

static enum golioth_rpc_status on_set_log_level(zcbor_state_t *request_params_array,
						zcbor_state_t *response_detail_map,
						void *callback_arg)
{
	double param_0;
	uint8_t log_level;
	bool ok;

	LOG_WRN("on_set_log_level");

	ok = zcbor_float_decode(request_params_array, &param_0);
	if (!ok) {
		LOG_ERR("Failed to decode array item");
		return GOLIOTH_RPC_INVALID_ARGUMENT;
	}

	log_level = (uint8_t)param_0;

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

		log_filter_set(NULL, 0, source_id, log_level);
		++source_id;
	}

	LOG_WRN("Log levels for %d modules set to: %d", source_id, log_level);

	ok = zcbor_tstr_put_lit(response_detail_map, "log_modules") &&
	     zcbor_float64_put(response_detail_map, (double)source_id);

	return GOLIOTH_RPC_OK;
}

static enum golioth_rpc_status on_reboot(zcbor_state_t *request_params_array,
					 zcbor_state_t *response_detail_map,
					 void *callback_arg)
{
	/* Use work queue so this RPC can return confirmation to Golioth */
	k_work_submit(&reboot_work);

	return GOLIOTH_RPC_OK;
}

static void rpc_log_if_register_failure(int err)
{
	if (err) {
		LOG_ERR("Failed to register RPC: %d", err);
	}
}

void app_rpc_register(struct golioth_client *client)
{
	struct golioth_rpc *rpc = golioth_rpc_init(client);

	int err;

	err = golioth_rpc_register(rpc, "get_network_info", on_get_network_info, NULL);
	rpc_log_if_register_failure(err);

	err = golioth_rpc_register(rpc, "reboot", on_reboot, NULL);
	rpc_log_if_register_failure(err);

	err = golioth_rpc_register(rpc, "set_log_level", on_set_log_level, NULL);
	rpc_log_if_register_failure(err);
}
