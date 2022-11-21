/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app_rpc, LOG_LEVEL_DBG);

#include <net/golioth/system_client.h>
#include <net/golioth/rpc.h>
#include <qcbor/qcbor.h>
#include <qcbor/qcbor_spiffy_decode.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/sys/reboot.h>

static void reboot_work_handler(struct k_work *work) {
	/* Sync longs before reboot */
	LOG_PANIC();

	for (int8_t i=5; i>=0; i--) {
		if (i) {
			LOG_INF("Rebooting in %d seconds...", i);
		}
		k_sleep(K_SECONDS(1));
	}

	sys_reboot(SYS_REBOOT_COLD);
}
K_WORK_DEFINE(reboot_work, reboot_work_handler);

static enum golioth_rpc_status on_reboot(QCBORDecodeContext *request_params_array,
					   QCBOREncodeContext *response_detail_map,
					   void *callback_arg)
{
	k_work_submit(&reboot_work);

	return GOLIOTH_RPC_OK;
}

int app_register_rpc(struct golioth_client *rpc_client) {
	int err = golioth_rpc_register(rpc_client, "reboot", on_reboot, NULL);

	if (err) {
		LOG_ERR("Failed to register RPC: %d", err);
	}

	return err;
}
