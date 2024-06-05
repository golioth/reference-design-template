/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(network_location, LOG_LEVEL_DBG);

#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/wifi_mgmt.h>
#include <golioth/client.h>
#include <golioth/stream.h>
#include <modem/modem_info.h>
#include <zephyr/data/json.h>

#include "network_location.h"

#define WIFI_MAC_STRING_SIZE	  sizeof("xx:xx:xx:xx:xx:xx")
#define WIFI_SCAN_RESULT_TEMPLATE "{\"mac\":\"%s\",\"rss\":%d}"
#define WIFI_SCAN_MGMT_EVENTS	  (NET_EVENT_WIFI_SCAN_RESULT | NET_EVENT_WIFI_SCAN_DONE)

static struct golioth_client *_client;
static char json_buf[1024];
struct modem_param_info modem_param;
static struct net_mgmt_event_callback wifi_scan_mgmt_cb;
/* clang-format off */
static struct network_location_info network_location_results = {
	.lte_results = {
		.mcc = (char[MODEM_INFO_MAX_RESPONSE_SIZE]){0},
		.mnc = (char[MODEM_INFO_MAX_RESPONSE_SIZE]){0},
		.cid = 0,
		.tac = 0,
		.rsrp = 0
	},
	.wlan_results = {
		/* This uses the "Range Expressions" GCC extension */
		[0 ... WIFI_LOCATION_MAX_SCAN_RESULTS - 1] = {
			.rssi = 0,
			.mac = (char[sizeof("xx:xx:xx:xx:xx:xx")]){0}
		}
	},
	.wlan_results_len = 0
};
/* clang-format on */

/* from deps/nrf/lib/location/cloud_service/cloud_service_here_rest.c */
#define HERE_MIN_RSRP -140
#define HERE_MAX_RSRP -44
static int here_adjust_rsrp(int input)
{
	int return_value;

	return_value = RSRP_IDX_TO_DBM(input);

	if (return_value < HERE_MIN_RSRP) {
		return_value = HERE_MIN_RSRP;
	} else if (return_value > HERE_MAX_RSRP) {
		return_value = HERE_MAX_RSRP;
	}

	return return_value;
}

static void get_modem_info_params(void)
{
	int err;
	struct lte_location_params *lte_results = &network_location_results.lte_results;

	err = modem_info_params_get(&modem_param);
	if (err) {
		LOG_ERR("Getting modem info parameters failed: %d", err);
	}

	strncpy(lte_results->mcc, modem_param.network.mcc.value_string,
		MODEM_INFO_MAX_RESPONSE_SIZE);
	strncpy(lte_results->mnc, modem_param.network.mnc.value_string,
		MODEM_INFO_MAX_RESPONSE_SIZE);
	lte_results->cid = (uint32_t)modem_param.network.cellid_dec;
	lte_results->tac = (int16_t)modem_param.network.area_code.value;
	lte_results->rsrp = (int32_t)here_adjust_rsrp(modem_param.network.rsrp.value);
}

static void print_network_location_info(void)
{
	const struct lte_location_params *lte_results = &network_location_results.lte_results;
	const struct wifi_location_scan_result *wlan_result;
	const int32_t *wlan_results_len = &network_location_results.wlan_results_len;

	printk("\n");
	printk("LTE Modem Parameters:\n");
	printk("Mobile Country Code (mcc): %s\n", lte_results->mcc);
	printk("Mobile Network Code (mnc): %s\n", lte_results->mnc);
	printk("Cell Identifier (cid): %d\n", lte_results->cid);
	printk("Tracking Area Code (tac): %d\n", lte_results->tac);
	printk("Reference Signal Received Power in dBm (rsrp): %d\n", lte_results->rsrp);
	printk("\n");
	printk("WiFi Scan Results:\n");
	for (int i = 0; i < *wlan_results_len; i++) {
		wlan_result = &network_location_results.wlan_results[i];
		printk("(%d) MAC: %s, RSSI: %d\n", i + 1, wlan_result->mac, wlan_result->rssi);
	}
	printk("\n");
}

static void encode_network_location_info(void)
{
	int err;

	err = json_obj_encode_buf(network_location_info_descr,
				  ARRAY_SIZE(network_location_info_descr),
				  &network_location_results, json_buf, sizeof(json_buf));
	if (err != 0) {
		LOG_ERR("Failed to encode scan results");
	}
}

static void stream_network_location_info_cb(struct golioth_client *client,
					    const struct golioth_response *response,
					    const char *path, void *arg)
{
	if (response->status != GOLIOTH_OK) {
		LOG_WRN("Failed to stream network location info: %d", response->status);
		return;
	}

	return;
}

static void stream_network_location_info(void)
{
	int err;

	err = golioth_stream_set_async(_client, "", GOLIOTH_CONTENT_TYPE_JSON, json_buf,
				       strlen(json_buf), stream_network_location_info_cb, NULL);
	if (err) {
		LOG_ERR("Failed to enqueue network location info stream request: %d", err);
	}
}

static void handle_wifi_scan_result(struct net_mgmt_event_callback *cb)
{
	const struct wifi_scan_result *result = (const struct wifi_scan_result *)cb->info;
	int32_t *wlan_results_len = &network_location_results.wlan_results_len;
	struct wifi_location_scan_result *wlan_result =
		&network_location_results.wlan_results[*wlan_results_len];

	if (result->mac_length) {
		if (*wlan_results_len < WIFI_LOCATION_MAX_SCAN_RESULTS) {
			snprintk(wlan_result->mac, WIFI_MAC_STRING_SIZE,
				 "%02x:%02x:%02x:%02x:%02x:%02x", result->mac[0], result->mac[1],
				 result->mac[2], result->mac[3], result->mac[4], result->mac[5]);
			wlan_result->rssi = result->rssi;
			(*wlan_results_len)++;
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
		LOG_ERR("WiFi scan request failed: %d", status->status);
		return;
	}

	get_modem_info_params();
	print_network_location_info();
	encode_network_location_info();
	stream_network_location_info();
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

int network_location_execute(void)
{
	int err;
	struct net_if *wifi_iface = net_if_get_first_wifi();
	struct wifi_scan_params params = {0};

	network_location_results.wlan_results_len = 0;

	err = net_mgmt(NET_REQUEST_WIFI_SCAN, wifi_iface, &params, sizeof(params));
	if (err) {
		LOG_ERR("WiFi scan request failed: %d", err);
		return -ENOEXEC;
	}

	return 0;
}

int network_location_init(struct golioth_client *client)
{
	int err;

	_client = client;

	err = modem_info_init();
	if (err) {
		LOG_ERR("Failed to initialize modem info: %d", err);
		return err;
	}

	err = modem_info_params_init(&modem_param);
	if (err) {
		LOG_ERR("Failed to initialize modem info: %d", err);
		return err;
	}

	net_mgmt_init_event_callback(&wifi_scan_mgmt_cb, wifi_mgmt_event_handler,
				     WIFI_SCAN_MGMT_EVENTS);
	net_mgmt_add_event_callback(&wifi_scan_mgmt_cb);

	return 0;
}
