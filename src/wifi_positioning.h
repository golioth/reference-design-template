/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __WIFI_POSITIONING_H__
#define __WIFI_POSITIONING_H__

#include <zephyr/net/wifi.h>

#define WIFI_POSITIONING_MAX_SCAN_RESULTS 10

struct wifi_positioning_scan_result {
	int32_t rssi;
	char mac[sizeof("xx:xx:xx:xx:xx:xx")];
};

struct wifi_positioning_scan_results {
	struct wifi_positioning_scan_result *results;
	int results_len;
};

struct wifi_positioning_scan_results *wifi_positioning_get_scan_results(void);
int wifi_positioning_request_scan(void);
int wifi_positioning_init(struct golioth_client *client);

#endif /* __WIFI_POSITIONING_H__ */
