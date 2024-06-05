/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(wifi_positioning, CONFIG_LOG_DEFAULT_LEVEL);

#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/wifi_mgmt.h>

#include "wifi_positioning.h"

#define WIFI_SCAN_RESULTS_MAX 10
#define WIFI_SHELL_MGMT_EVENTS                                                                     \
	(NET_EVENT_WIFI_SCAN_RESULT | NET_EVENT_WIFI_SCAN_DONE | NET_EVENT_WIFI_CONNECT_RESULT |   \
	 NET_EVENT_WIFI_DISCONNECT_RESULT | NET_EVENT_WIFI_TWT | NET_EVENT_WIFI_RAW_SCAN_RESULT)

static uint32_t scan_result_num;
static struct net_mgmt_event_callback wifi_scan_mgmt_cb;
static struct wifi_positioning_scan_result scan_results_arr[WIFI_POSITIONING_MAX_SCAN_RESULTS];
static struct wifi_positioning_scan_results scan_results = {.results = scan_results_arr,
							    .results_len = 0};
char json_buf[1024];

static void print_scan_results(void)
{
	const struct wifi_positioning_scan_result *result;

	printk("WiFi Scan Results:\n");
	for (int i = 0; i < scan_results.results_len; i++) {
		result = &scan_results.results[i];
		printk("MAC: %s, RSSI: %d\n", result->mac, result->rssi);
	}
}

static void handle_wifi_scan_result(struct net_mgmt_event_callback *cb)
{
	const struct wifi_scan_result *result = (const struct wifi_scan_result *)cb->info;

	if (result->mac_length) {
		if (scan_results.results_len < WIFI_POSITIONING_MAX_SCAN_RESULTS) {
			snprintk(scan_results.results[scan_results.results_len].mac,
				 sizeof(scan_results.results[scan_results.results_len].mac),
				 "%02x:%02x:%02x:%02x:%02x:%02x", result->mac[0], result->mac[1],
				 result->mac[2], result->mac[3], result->mac[4], result->mac[5]);
			scan_results.results[scan_results.results_len].rssi = result->rssi;
			scan_results.results_len++;
		} else {
			LOG_WRN("Scan result buffer full, dropped MAC: "
				"%02x:%02x:%02x:%02x:%02x:%02x",
				result->mac[0], result->mac[1], result->mac[2], result->mac[3],
				result->mac[4], result->mac[5]);
		}
	}
}

static void handle_wifi_scan_done(struct net_mgmt_event_callback *cb)
{
	const struct wifi_status *status = (const struct wifi_status *)cb->info;

	if (status->status) {
		LOG_ERR("WiFi scan request failed (%d)", status->status);
		return;
	}

	LOG_DBG("WiFi scan request done");

	print_scan_results();
}

static void wifi_mgmt_event_handler(struct net_mgmt_event_callback *cb, uint32_t mgmt_event,
				    struct net_if *iface)
{
	ARG_UNUSED(iface);

	switch (mgmt_event) {
	case NET_EVENT_WIFI_SCAN_RESULT:
		handle_wifi_scan_result(cb);
		break;
	case NET_EVENT_WIFI_SCAN_DONE:
		handle_wifi_scan_done(cb);
		break;
	default:
		break;
	}
}

struct wifi_positioning_scan_results *wifi_positioning_get_scan_results(void)
{
	return &scan_results;
}

int wifi_positioning_request_scan(void)
{
	struct net_if *wifi_iface = net_if_get_first_wifi();
	struct wifi_scan_params params = {0};

	scan_results.results_len = 0;

	if (net_mgmt(NET_REQUEST_WIFI_SCAN, wifi_iface, &params, sizeof(params))) {
		LOG_ERR("WiFi scan request failed");
		return -ENOEXEC;
	}

	LOG_DBG("WiFi scan requested");

	return 0;
}

int wifi_positioning_init()
{
	scan_result_num = 0U;

	net_mgmt_init_event_callback(&wifi_scan_mgmt_cb, wifi_mgmt_event_handler,
				     WIFI_SHELL_MGMT_EVENTS);
	net_mgmt_add_event_callback(&wifi_scan_mgmt_cb);

	return 0;
}
