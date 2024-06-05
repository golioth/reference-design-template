/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __NETWORK_LOCATION_H__
#define __NETWORK_LOCATION_H__

#include <zephyr/net/wifi.h>
#include <zephyr/data/json.h>

#define WIFI_LOCATION_MAX_SCAN_RESULTS 10

struct lte_location_params {
	char *mcc;
	char *mnc;
	uint32_t cid;
	int16_t tac;
	int32_t rsrp;
};

struct wifi_location_scan_result {
	int32_t rssi;
	char *mac;
};

struct network_location_info {
	struct lte_location_params lte_results;
	struct wifi_location_scan_result wlan_results[WIFI_LOCATION_MAX_SCAN_RESULTS];
	int32_t wlan_results_len;
};

static const struct json_obj_descr lte_location_params_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct lte_location_params, mcc, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct lte_location_params, mnc, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct lte_location_params, cid, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct lte_location_params, tac, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct lte_location_params, rsrp, JSON_TOK_NUMBER)};

static const struct json_obj_descr wifi_location_scan_result_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct wifi_location_scan_result, rssi, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct wifi_location_scan_result, mac, JSON_TOK_STRING)};

static const struct json_obj_descr network_location_info_descr[] = {
	JSON_OBJ_DESCR_OBJECT_NAMED(struct network_location_info, "lte", lte_results,
				    lte_location_params_descr),
	JSON_OBJ_DESCR_OBJ_ARRAY_NAMED(struct network_location_info, "wlan", wlan_results,
				       WIFI_LOCATION_MAX_SCAN_RESULTS, wlan_results_len,
				       wifi_location_scan_result_descr,
				       ARRAY_SIZE(wifi_location_scan_result_descr))};

int network_location_execute(void);
int network_location_init(struct golioth_client *client);

#endif /* __NETWORK_LOCATION_H__ */
