/*
 * Copyright (c) 2022-2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * Handle remote procedure calls received from Golioth, returning a status code
 * indicating the success or failure of the call.
 *
 * This demonstration implements the following RPCs:
 * - `get_network_info`: Query and return network information.
 * - `reboot`: reboot the device (no arguments)
 * - `set_log_level`: adjust the logging level for all registered modules (valid
 *   argument values: 0..4)
 *
 * https://docs.golioth.io/firmware/zephyr-device-sdk/remote-procedure-call
 */

#ifndef __APP_RPC_H__
#define __APP_RPC_H__

#include <golioth/client.h>

int app_rpc_register(struct golioth_client *client);

#endif /* __APP_RPC_H__ */
