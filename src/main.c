/*
 * Copyright (c) 2022-2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(golioth_rd_template, LOG_LEVEL_DBG);

#include <modem/lte_lc.h>
#include <net/golioth/system_client.h>
#include <samples/common/net_connect.h>
#include <zephyr/net/coap.h>
#include "app_rpc.h"
#include "app_settings.h"
#include "app_state.h"
#include "app_work.h"
#include "dfu/app_dfu.h"
#include "main.h"

#ifdef CONFIG_LIB_OSTENTUS
#include <libostentus.h>
#endif
#ifdef CONFIG_ALUDEL_BATTERY_MONITOR
#include "battery_monitor/battery.h"
#endif

#include <zephyr/drivers/gpio.h>

#ifdef CONFIG_MODEM_INFO
#include <modem/modem_info.h>
#endif

static struct golioth_client *client = GOLIOTH_SYSTEM_CLIENT_GET();

K_SEM_DEFINE(connected, 0, 1);
K_SEM_DEFINE(dfu_status_unreported, 1, 1);

static k_tid_t _system_thread = 0;

static const struct gpio_dt_spec golioth_led = GPIO_DT_SPEC_GET(DT_ALIAS(golioth_led), gpios);
static const struct gpio_dt_spec user_btn = GPIO_DT_SPEC_GET(DT_ALIAS(sw1), gpios);
static struct gpio_callback button_cb_data;

/* forward declarations */
void golioth_connection_led_set(uint8_t state);

void wake_system_thread(void)
{
	k_wakeup(_system_thread);
}

static void golioth_on_connect(struct golioth_client *client)
{
	k_sem_give(&connected);
	golioth_connection_led_set(1);

	LOG_INF("Registering observations with Golioth");
	app_dfu_observe();
	app_settings_observe();
	app_rpc_observe();
	app_state_observe();

	if (k_sem_take(&dfu_status_unreported, K_NO_WAIT) == 0) {
		/* Report firmware update status on first connect after power up */
		app_dfu_report_state_to_golioth();
	}
}

#ifdef CONFIG_SOC_NRF9160
static void process_lte_connected(void)
{
	/* Change the state of the Internet LED on Ostentus */
	IF_ENABLED(CONFIG_LIB_OSTENTUS, (led_internet_set(1);));

	golioth_system_client_start();
}

/**
 * @brief Perform actions based on LTE connection events
 *
 * This is copied from the Golioth samples/common/nrf91_lte_monitor.c to allow us to perform custom
 * actions (turn on LED and start Golioth client) when a network connection becomes available.
 *
 * Set `CONFIG_GOLIOTH_SAMPLE_NRF91_LTE_MONITOR=n` so that the common sample code doesn't collide.
 *
 * @param evt
 */
static void lte_handler(const struct lte_lc_evt *const evt)
{
	switch (evt->type) {
	case LTE_LC_EVT_NW_REG_STATUS:
		switch (evt->nw_reg_status) {
		case LTE_LC_NW_REG_NOT_REGISTERED:
			LOG_INF("Network: Not registered");
			break;
		case LTE_LC_NW_REG_REGISTERED_HOME:
			LOG_INF("Network: Registered (home)");
			process_lte_connected();
			break;
		case LTE_LC_NW_REG_SEARCHING:
			LOG_INF("Network: Searching");
			break;
		case LTE_LC_NW_REG_REGISTRATION_DENIED:
			LOG_INF("Network: Registration denied");
			break;
		case LTE_LC_NW_REG_UNKNOWN:
			LOG_INF("Network: Unknown");
			break;
		case LTE_LC_NW_REG_REGISTERED_ROAMING:
			LOG_INF("Network: Registered (roaming)");
			process_lte_connected();
			break;
		case LTE_LC_NW_REG_REGISTERED_EMERGENCY:
			LOG_INF("Network: Registered (emergency)");
			break;
		case LTE_LC_NW_REG_UICC_FAIL:
			LOG_INF("Network: UICC fail");
			break;
		}
		break;
	case LTE_LC_EVT_RRC_UPDATE:
		switch (evt->rrc_mode) {
		case LTE_LC_RRC_MODE_CONNECTED:
			LOG_DBG("RRC: Connected");
			break;
		case LTE_LC_RRC_MODE_IDLE:
			LOG_DBG("RRC: Idle");
			break;
		}
		break;
	default:
		break;
	}
}
#endif /* CONFIG_SOC_NRF9160 */

#ifdef CONFIG_MODEM_INFO
static void log_modem_firmware_version(void)
{
	char sbuf[128];

	/* Initialize modem info */
	int err = modem_info_init();

	if (err) {
		LOG_ERR("Failed to initialize modem info: %d", err);
	}

	/* Log modem firmware version */
	modem_info_string_get(MODEM_INFO_FW_VERSION, sbuf, sizeof(sbuf));
	LOG_INF("Modem firmware version: %s", sbuf);
}
#endif

void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	LOG_DBG("Button pressed at %d", k_cycle_get_32());
	/* This function is an Interrupt Service Routine. Do not call functions that
	 * use other threads, or perform long-running operations here
	 */
	k_wakeup(_system_thread);
}

/* Set (unset) LED indicators for active Golioth connection */
void golioth_connection_led_set(uint8_t state)
{
	uint8_t pin_state = state ? 1 : 0;
	/* Turn on Golioth logo LED once connected */
	gpio_pin_set_dt(&golioth_led, pin_state);
	/* Change the state of the Golioth LED on Ostentus */
	IF_ENABLED(CONFIG_LIB_OSTENTUS, (led_golioth_set(pin_state);));
}

#include <zcbor_common.h>
#define DATA_POINT_THRESHOLD_BEFORE_NEWBLOCK 100

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

int main(void)
{
	int err;

	LOG_DBG("Start Reference Design Template sample");

	LOG_INF("Firmware version: %s", CONFIG_MCUBOOT_IMAGE_VERSION);
	IF_ENABLED(CONFIG_MODEM_INFO, (log_modem_firmware_version();));

	IF_ENABLED(CONFIG_LIB_OSTENTUS, (
		/* Clear Ostentus memory */
		clear_memory();
		/* Update Ostentus LEDS using bitmask (Power On and Battery) */
		led_bitmask(LED_POW | LED_BAT);
		/* Show Golioth Logo on Ostentus ePaper screen */
		show_splash();

	));

	/* Get system thread id so loop delay change event can wake main */
	_system_thread = k_current_get();

	/* Initialize Golioth logo LED */
	err = gpio_pin_configure_dt(&golioth_led, GPIO_OUTPUT_INACTIVE);
	if (err) {
		LOG_ERR("Unable to configure LED for Golioth Logo");
	}

	/* Initialize app state */
	app_state_init(client);

	/* Initialize app work */
	app_work_init(client);

	/* Initialize DFU components */
	app_dfu_init(client);

	/* Initialize app settings */
	app_settings_init(client);

	/* Initialize app RPC */
	app_rpc_init(client);

	/* Register Golioth on_connect callback */
	client->on_connect = golioth_on_connect;

	/* Start LTE asynchronously if the nRF9160 is used
	 * Golioth Client will start automatically when LTE connects
	 */
	IF_ENABLED(CONFIG_SOC_NRF9160, (LOG_INF("Connecting to LTE, this may take some time...");
					lte_lc_init_and_connect_async(lte_handler);));

	/* If nRF9160 is not used, start the Golioth Client and block until connected */
	if (!IS_ENABLED(CONFIG_SOC_NRF9160)) {
		/* Run WiFi/DHCP if necessary */
		if (IS_ENABLED(CONFIG_GOLIOTH_SAMPLES_COMMON)) {
			net_connect();
		}

		/* Start Golioth client */
		golioth_system_client_start();

		/* Block until connected to Golioth */
		k_sem_take(&connected, K_FOREVER);

		/* Turn on Golioth logo LED once connected */
		gpio_pin_set_dt(&golioth_led, 1);
	}

	/* Set up user button */
	err = gpio_pin_configure_dt(&user_btn, GPIO_INPUT);
	if (err) {
		LOG_ERR("Error %d: failed to configure %s pin %d", err, user_btn.port->name,
			user_btn.pin);
		return err;
	}

	err = gpio_pin_interrupt_configure_dt(&user_btn, GPIO_INT_EDGE_TO_ACTIVE);
	if (err) {
		LOG_ERR("Error %d: failed to configure interrupt on %s pin %d", err,
			user_btn.port->name, user_btn.pin);
		return err;
	}

	gpio_init_callback(&button_cb_data, button_pressed, BIT(user_btn.pin));
	gpio_add_callback(user_btn.port, &button_cb_data);

	IF_ENABLED(CONFIG_LIB_OSTENTUS,(
		/* Set up a slideshow on Ostentus
		 *  - add up to 256 slides
		 *  - use the enum in app_work.h to add new keys
		 *  - values are updated using these keys (see app_work.c)
		 */
		slide_add(UP_COUNTER, LABEL_UP_COUNTER, strlen(LABEL_UP_COUNTER));
		slide_add(DN_COUNTER, LABEL_DN_COUNTER, strlen(LABEL_DN_COUNTER));
		IF_ENABLED(CONFIG_ALUDEL_BATTERY_MONITOR, (
			slide_add(BATTERY_V, LABEL_BATTERY, strlen(LABEL_BATTERY));
			slide_add(BATTERY_LVL, LABEL_BATTERY, strlen(LABEL_BATTERY));
		));
		slide_add(FIRMWARE, LABEL_FIRMWARE, strlen(LABEL_FIRMWARE));

		/* Set the title ofthe Ostentus summary slide (optional) */
		summary_title(SUMMARY_TITLE, strlen(SUMMARY_TITLE));

		/* Update the Firmware slide with the firmware version */
		slide_set(FIRMWARE, CONFIG_MCUBOOT_IMAGE_VERSION, strlen(CONFIG_MCUBOOT_IMAGE_VERSION));

		/* Start Ostentus slideshow with 30 second delay between slides */
		slideshow(30000);
	));


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
