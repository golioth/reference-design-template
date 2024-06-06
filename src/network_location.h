/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __NETWORK_LOCATION_H__
#define __NETWORK_LOCATION_H__

#include <zephyr/net/wifi.h>

struct wifi_location_scan_result {
	int32_t rssi;
	char mac[sizeof("xx:xx:xx:xx:xx:xx")];
};

struct wifi_location_scan_results {
	struct wifi_location_scan_result *results;
	int results_len;
};

int network_location_execute(void);
int network_location_init(struct golioth_client *client);

#endif /* __NETWORK_LOCATION_H__ */
