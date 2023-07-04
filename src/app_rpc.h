/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __APP_RPC_H__
#define __APP_RPC_H__

#include <net/golioth/system_client.h>

int app_rpc_init(struct golioth_client *state_client);
int app_rpc_observe(void);
int app_rpc_register(struct golioth_client *rpc_client);

#endif /* __APP_RPC_H__ */
