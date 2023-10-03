/*
 * Copyright (c) 2022-2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/** Observe and write to example endpoints for stateful data on the Golioth
 * LightDB State Service.
 *
 * This demonstration exhibits (the concept of Digital
 * Twin)[https://blog.golioth.io/better-iot-design-patterns-desired-state-vs-actual-state/].
 * It implements a _desired_ state which the cloud can set to request the device
 * change its state, and an _actual_ state where the device reports its state.
 *
 * After receiving and processing a desired state, the device will reset the
 * desired state (`APP_STATE_DESIRED_ENDP`) to `-1` indicating the data has been
 * processed, and update the actual state (`APP_STATE_ACTUAL_ENDP`) to report
 * the new state of the device.
 *
 * The device should write to the _actual state_ endpoint, the cloud should not.
 * By convention the cloud should consider the _actual state_ values read-only.
 *
 * https://docs.golioth.io/firmware/zephyr-device-sdk/light-db/
 */

#ifndef __APP_STATE_H__
#define __APP_STATE_H__

#include <golioth/client.h>

#define APP_STATE_DESIRED_ENDP "desired"
#define APP_STATE_ACTUAL_ENDP  "state"

int app_state_observe(struct golioth_client *state_client);
int app_state_update_actual(void);

#endif /* __APP_STATE_H__ */
