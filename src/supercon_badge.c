#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(supercon_badge, LOG_LEVEL_DBG);

#include <libostentus.h>
#include <net/golioth/system_client.h>
#include <zcbor_common.h>
#include <zcbor_encode.h>
#include "supercon_badge.h"

#define DATA_POINT_THRESHOLD_BEFORE_NEWBLOCK 100

struct golioth_client *client = GOLIOTH_SYSTEM_CLIENT_GET();

struct cbor_block {
	uint8_t cbor_payload[1000];
	zcbor_state_t *encoding_state;
	uint16_t cur_uid;
	uint16_t cur_block;
	uint16_t data_idx;
} CborBlockCtx;

void reset_ctx(void) {
	CborBlockCtx.cur_uid = 0;
	CborBlockCtx.cur_block = 0;
	CborBlockCtx.data_idx = 0;
}

void cbor_begin_new_block(SuperPacket packet) {
	ZCBOR_STATE_E(new_state, 8, CborBlockCtx.cbor_payload, sizeof(CborBlockCtx.cbor_payload), 0);

	CborBlockCtx.encoding_state = new_state;

	zcbor_map_start_encode(CborBlockCtx.encoding_state, 3);
	zcbor_tstr_put_lit(CborBlockCtx.encoding_state, "uid");
	zcbor_int_encode(CborBlockCtx.encoding_state, &CborBlockCtx.cur_uid, 2);
	zcbor_tstr_put_lit(CborBlockCtx.encoding_state, "block_num");
	zcbor_int_encode(CborBlockCtx.encoding_state, &CborBlockCtx.cur_block, 2);
}

void cbor_begin_points_list(SuperPacket packet) {
	zcbor_tstr_put_lit(CborBlockCtx.encoding_state, "points");
	zcbor_list_start_encode(CborBlockCtx.encoding_state, DATA_POINT_THRESHOLD_BEFORE_NEWBLOCK + 50);
}

void cbor_end_points_close_map(SuperPacket packet) {
	zcbor_list_end_encode(CborBlockCtx.encoding_state, DATA_POINT_THRESHOLD_BEFORE_NEWBLOCK + 50);
	zcbor_map_end_encode(CborBlockCtx.encoding_state, 3);
}

void cbor_add_points(SuperPacket packet) {
	for (uint8_t i = 0; i < 16; i++) {
		zcbor_int_encode(CborBlockCtx.encoding_state, &packet.points[SCB_POINTS_INT + i], 2);
		CborBlockCtx.data_idx++;
	}
}

void push_cbor(SuperPacket packet) {
	size_t cbor_payload_len = CborBlockCtx.encoding_state->payload - CborBlockCtx.cbor_payload;

	char endp[6];
	snprintk(endp, sizeof(endp), "%d", CborBlockCtx.cur_uid);

	int err = golioth_stream_push(client,
					endp,
					GOLIOTH_CONTENT_FORMAT_APP_CBOR,
					CborBlockCtx.cbor_payload,
					cbor_payload_len);
	if (err) {
		LOG_ERR("Unable to stream block %d cbor: %d", CborBlockCtx.cur_block, err);
	} else {
		LOG_INF("Successfully pushed block %d", CborBlockCtx.cur_block);
	}
}

int process_packet(SuperPacket packet) {
	if (packet.points[SCB_BLOCKNUM] == 0) {
		reset_ctx();
		CborBlockCtx.cur_uid = packet.points[SCB_UID];
		LOG_INF("UID: %d", CborBlockCtx.cur_uid);
		LOG_INF("Block Number: %d", packet.points[SCB_BLOCKNUM]);
		LOG_INF("Interval: %d", packet.points[SCB_INTERVAL]);
		LOG_INF("Total Data Points: %d", packet.points[SCB_TOTALDATA]);
		LOG_INF("Name: %s", &packet.bytes[SCB_NAME_BYTES_INDEX]);

		cbor_begin_new_block(packet);

		/* Add metadata */
		zcbor_tstr_put_lit(CborBlockCtx.encoding_state, "interval");
		zcbor_int_encode(CborBlockCtx.encoding_state, &packet.points[SCB_INTERVAL], 2);
		zcbor_tstr_put_lit(CborBlockCtx.encoding_state, "point_cnt");
		zcbor_int_encode(CborBlockCtx.encoding_state, &packet.points[SCB_TOTALDATA], 2);
		zcbor_tstr_put_lit(CborBlockCtx.encoding_state, "name");
		zcbor_tstr_put_term(CborBlockCtx.encoding_state, &packet.bytes[SCB_NAME_BYTES_INDEX]);

		cbor_begin_points_list(packet);

	} else if (packet.points[SCB_BLOCKNUM] == 65535) {
		/* Finish processing the finaly block */
		cbor_add_points(packet);
		cbor_end_points_close_map(packet);
		push_cbor(packet);
		reset_ctx();
	} else {
		if (CborBlockCtx.data_idx > DATA_POINT_THRESHOLD_BEFORE_NEWBLOCK) {
			/* Close and send this block, then start a new one */
			cbor_end_points_close_map(packet);
			push_cbor(packet);

			++CborBlockCtx.cur_block;
			CborBlockCtx.data_idx = 0;

			cbor_begin_new_block(packet);
			cbor_begin_points_list(packet);
		}

		cbor_add_points(packet);
	}
	return 0;
}

#define SUPERCON_BADGE_STACK 4096

extern void supercon_badge_thread(void *d0, void *d1, void *d2)
{
	SuperPacket packet;
	while (true) {
		if (ostentus_i2c_readbyte(0xE0) == 1) {
			ostentus_i2c_readarray(0xE1, packet.bytes, 36);
			process_packet(packet);
		} else {
			k_sleep(K_SECONDS(1));
		}

		//app_work_sensor_read();

		//k_sleep(K_SECONDS(get_loop_delay_s()));
	}
}

K_THREAD_DEFINE(weather_sensor_tid, SUPERCON_BADGE_STACK, supercon_badge_thread, NULL, NULL, NULL,
		K_LOWEST_APPLICATION_THREAD_PRIO, 0, 0);
