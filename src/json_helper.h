/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __JSON_HELPER_H_
#define __JSON_HELPER_H_

#include <zephyr/data/json.h>

struct app_state {
	int32_t acc;
	char* last_run;
	float lat;
	float lng;
	uint32_t status;
};

static const struct json_obj_descr app_state_descr[] = {
	JSON_OBJ_DESCR_PRIM(struct app_state, acc, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct app_state, last_run, JSON_TOK_STRING),
	JSON_OBJ_DESCR_PRIM(struct app_state, lat, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct app_state, lng, JSON_TOK_NUMBER),
	JSON_OBJ_DESCR_PRIM(struct app_state, status, JSON_TOK_NUMBER)};

#endif
