/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(wifi_positioning, LOG_LEVEL_DBG);

#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zcbor_encode.h>
#include <golioth/client.h>
#include <golioth/stream.h>
#include <modem/modem_info.h>

#include "wifi_positioning.h"

#define WIFI_SCAN_RESULTS_MAX 10
#define WIFI_SHELL_MGMT_EVENTS                                                                     \
	(NET_EVENT_WIFI_SCAN_RESULT | NET_EVENT_WIFI_SCAN_DONE | NET_EVENT_WIFI_CONNECT_RESULT |   \
	 NET_EVENT_WIFI_DISCONNECT_RESULT | NET_EVENT_WIFI_TWT | NET_EVENT_WIFI_RAW_SCAN_RESULT)

static struct golioth_client *_client;
static struct net_mgmt_event_callback wifi_scan_mgmt_cb;
static struct wifi_positioning_scan_result scan_results_arr[WIFI_POSITIONING_MAX_SCAN_RESULTS];
static struct wifi_positioning_scan_results scan_results = {.results = scan_results_arr,
							    .results_len = 0};

static void print_scan_results(void)
{
	const struct wifi_positioning_scan_result *result;

	printk("WiFi Scan Results:\n");
	for (int i = 0; i < scan_results.results_len; i++) {
		result = &scan_results.results[i];
		printk("MAC: %s, RSSI: %d\n", result->mac, result->rssi);
	}
}

static void stream_scan_results_cb(struct golioth_client *client,
				   const struct golioth_response *response, const char *path,
				   void *arg)
{
	if (response->status != GOLIOTH_OK) {
		LOG_WRN("Failed to stream scan results: %d", response->status);
		return;
	}

	LOG_DBG("Scan results successfully streamed to Golioth");

	return;
}

static void stream_scan_results(void)
{
	int err;
	const struct wifi_positioning_scan_result *result;
	uint8_t cbor_buf[1024];
	struct modem_param_info modem_param;

	LOG_DBG("Streaming scan results");

	/* Create zcbor state variable for encoding */
	ZCBOR_STATE_E(zse, 1, cbor_buf, sizeof(cbor_buf), 1);

	/* Encode the root CBOR map header */
	bool ok = zcbor_map_start_encode(zse, 1);
	if (!ok) {
		LOG_ERR("Failed to start encoding the root CBOR map");
		return;
	}

	/* Encode the map key "lte" as a text string */
	// ok = zcbor_tstr_put_lit(zse, "lte");
	// if (!ok) {
	// 	LOG_ERR("CBOR: Failed to encode lte map key");
	// 	return;
	// }

	/* Encode the lte value as an int */
	// ok = zcbor_int32_put(zse, 5);
	// if (!ok) {
	// 	LOG_ERR("CBOR: failed to encode lte map value");
	// 	return;
	// }

	/* Encode the map key "wlan" as a text string */
	ok = zcbor_tstr_put_lit(zse, "wlan");
	if (!ok) {
		LOG_ERR("CBOR: Failed to encode wlan map key");
		return;
	}

	/* Encode the wlan value as a list of scan results */
	ok = zcbor_list_start_encode(zse, scan_results.results_len);
	if (!ok) {
		LOG_ERR("CBOR: failed to encode wlan value list start");
		return;
	}

	for (int i = 0; i < scan_results.results_len; i++) {
		result = &scan_results.results[i];

		/* Encode the result CBOR map header */
		bool ok = zcbor_map_start_encode(zse, 2);
		if (!ok) {
			LOG_ERR("Failed to start encoding the result CBOR map");
			return;
		}

		/* Encode the map key "mac" as a text string */
		ok = zcbor_tstr_put_lit(zse, "mac");
		if (!ok) {
			LOG_ERR("CBOR: Failed to encode mac map key");
			return;
		}

		/* Encode the mac value as a text string */
		ok = zcbor_tstr_put_term(zse, result->mac);
		if (!ok) {
			LOG_ERR("CBOR: Failed to encode mac value");
			return;
		}

		/* Encode the map key "rss" as a text string */
		ok = zcbor_tstr_put_lit(zse, "rss");
		if (!ok) {
			LOG_ERR("CBOR: Failed to encode rss map key");
			return;
		}

		/* Encode the rss value as an int32_t */
		ok = zcbor_int32_put(zse, result->rssi);
		if (!ok) {
			LOG_ERR("CBOR: failed to encode rss value");
			return;
		}

		/* Close the result CBOR map */
		ok = zcbor_map_end_encode(zse, 2);
		if (!ok) {
			LOG_ERR("Failed to close the result CBOR map");
			return;
		}
	}

	/* Close the wlan value list */
	ok = zcbor_list_end_encode(zse, scan_results.results_len);
	if (!ok) {
		LOG_ERR("CBOR: failed to close wlan value list");
		return;
	}

	/* Close the root CBOR map */
	ok = zcbor_map_end_encode(zse, 1);
	if (!ok) {
		LOG_ERR("Failed to close the root CBOR map");
		return;
	}

	size_t payload_size = (intptr_t)zse->payload - (intptr_t)cbor_buf;

	LOG_HEXDUMP_DBG(cbor_buf, payload_size, "scan_results");

	err = golioth_stream_set_async(_client, "", GOLIOTH_CONTENT_TYPE_CBOR, cbor_buf,
				       payload_size, stream_scan_results_cb, NULL);
	if (err) {
		LOG_ERR("Failed to send sensor data to Golioth: %d", err);
	}

	LOG_DBG("Stream request enqueued");
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
	stream_scan_results();
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

int wifi_positioning_init(struct golioth_client *client)
{
	_client = client;

	net_mgmt_init_event_callback(&wifi_scan_mgmt_cb, wifi_mgmt_event_handler,
				     WIFI_SHELL_MGMT_EVENTS);
	net_mgmt_add_event_callback(&wifi_scan_mgmt_cb);

	return 0;
}
