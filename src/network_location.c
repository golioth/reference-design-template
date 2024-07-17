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

#include "network_location.h"

#define WIFI_LOCATION_MAX_SCAN_RESULTS 10

#define WIFI_SCAN_RESULT_TEMPLATE "{\"mac\":\"%s\",\"rss\":%d}"
#define LTE_MODEM_PARAMS_TEMPLATE                                                                  \
	"{\"mcc\":\"%s\",\"mnc\":\"%s\",\"cid\":%d,\"tac\":%d,\"rsrp\":%d}"

#define WIFI_SHELL_MGMT_EVENTS                                                                     \
	(NET_EVENT_WIFI_SCAN_RESULT | NET_EVENT_WIFI_SCAN_DONE | NET_EVENT_WIFI_CONNECT_RESULT |   \
	 NET_EVENT_WIFI_DISCONNECT_RESULT | NET_EVENT_WIFI_TWT | NET_EVENT_WIFI_RAW_SCAN_RESULT)

static struct golioth_client *_client;
struct modem_param_info modem_param;
static struct net_mgmt_event_callback wifi_scan_mgmt_cb;
static struct wifi_location_scan_result scan_results_arr[WIFI_LOCATION_MAX_SCAN_RESULTS];
static struct wifi_location_scan_results scan_results = {.results = scan_results_arr,
							 .results_len = 0};
static char json_buf[1024];

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
	int err = modem_info_params_get(&modem_param);
	if (err) {
		LOG_ERR("Getting modem info parameters failed: %d", err);
	}
}

static void print_network_location_info(void)
{
	const struct wifi_location_scan_result *result;

	printk("\nLTE Modem Parameters:\n");
	printk("Mobile Country Code (mcc): %s\n", modem_param.network.mcc.value_string);
	printk("Mobile Network Code (mnc): %s\n", modem_param.network.mnc.value_string);
	printk("Cell Identifier (cid): %d\n", (uint32_t)modem_param.network.cellid_dec);
	printk("Tracking Area Code (tac): %d\n", modem_param.network.area_code.value);
	printk("Reference Signal Received Power in dBm (rsrp): %d\n",
	       here_adjust_rsrp(modem_param.network.rsrp.value));
	printk("\nWiFi Scan Results:\n");
	for (int i = 0; i < scan_results.results_len; i++) {
		result = &scan_results.results[i];
		printk("(%d) MAC: %s, RSSI: %d\n", i + 1, result->mac, result->rssi);
	}
	printk("\n");
}

static void encode_network_location_info(void)
{
	const struct wifi_location_scan_result *result;
	char modem_params_buf[sizeof(
		"{\"mcc\":\"xxx\",\"mnc\":\"xxx\",\"cid\":268435455,\"tac\":65535,\"rsrp\":-"
		"156}")];
	char result_buf[sizeof("{\"mac\":\"xx:xx:xx:xx:xx:xx\",\"rss\":-xxx}")];

	json_buf[0] = '\0';

	strcat(json_buf, "{");

	/* Start LTE modem parameters object */
	strcat(json_buf, "\"lte\":[");

	snprintk(modem_params_buf, sizeof(modem_params_buf), LTE_MODEM_PARAMS_TEMPLATE,
		 modem_param.network.mcc.value_string, modem_param.network.mnc.value_string,
		 (uint32_t)modem_param.network.cellid_dec, modem_param.network.area_code.value,
		 here_adjust_rsrp(modem_param.network.rsrp.value));

	strcat(json_buf, modem_params_buf);

	strcat(json_buf, "]");
	/* End LTE modem parameters object */

	strcat(json_buf, ",");

	/* Start WiFi scan results */
	strcat(json_buf, "\"wlan\":[");

	for (int i = 0; i < scan_results.results_len; i++) {
		result = &scan_results.results[i];

		snprintk(result_buf, sizeof(result_buf), WIFI_SCAN_RESULT_TEMPLATE, result->mac,
			 result->rssi);

		strcat(json_buf, result_buf);

		if (i < (scan_results.results_len - 1)) {
			strcat(json_buf, ",");
		}
	}

	strcat(json_buf, "]");
	/* End WiFi scan results */

	strcat(json_buf, "}");
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

	err = golioth_stream_set_async(_client, "wifi_lte_loc_req", GOLIOTH_CONTENT_TYPE_JSON, json_buf,
				       strlen(json_buf), stream_network_location_info_cb, NULL);
	if (err) {
		LOG_ERR("Failed to enqueue network location info stream request: %d", err);
	}
}

static void handle_wifi_scan_result(struct net_mgmt_event_callback *cb)
{
	const struct wifi_scan_result *result = (const struct wifi_scan_result *)cb->info;

	if (result->mac_length) {
		if (scan_results.results_len < WIFI_LOCATION_MAX_SCAN_RESULTS) {
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
	struct net_if *wifi_iface = net_if_get_first_wifi();
	struct wifi_scan_params params = {0};

	scan_results.results_len = 0;

	int err = net_mgmt(NET_REQUEST_WIFI_SCAN, wifi_iface, &params, sizeof(params));
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
				     WIFI_SHELL_MGMT_EVENTS);
	net_mgmt_add_event_callback(&wifi_scan_mgmt_cb);

	return 0;
}
