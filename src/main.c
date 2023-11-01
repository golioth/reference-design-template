/*
 * Copyright (c) 2022-2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(golioth_rd_template, LOG_LEVEL_DBG);

#include "app_rpc.h"
#include "app_settings.h"
#include "app_state.h"
#include "app_work.h"
#include "golioth.h"
#include <modem/lte_lc.h>
#include <samples/common/net_connect.h>
#include <samples/common/sample_credentials.h>
#include <zephyr/kernel.h>

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

/* Current firmware version; update in prj.conf or via build argument */
static const char *_current_version = CONFIG_GOLIOTH_SAMPLE_FW_VERSION;

static golioth_client_t client;
K_SEM_DEFINE(connected, 0, 1);

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

static void on_client_event(golioth_client_t client, golioth_client_event_t event, void *arg)
{
	bool is_connected = (event == GOLIOTH_CLIENT_EVENT_CONNECTED);

	if (is_connected) {
		k_sem_give(&connected);
		golioth_connection_led_set(1);
	}
	LOG_INF("Golioth client %s", is_connected ? "connected" : "disconnected");
}

static void start_golioth_client(void)
{
	if (client == NULL) {
		/* Get the client configuration from auto-loaded settings */
		const golioth_client_config_t *client_config = golioth_sample_credentials_get();

		/* Create and start a Golioth Client */
		client = golioth_client_create(client_config);

		/* Register Golioth on_connect callback */
		golioth_client_register_event_callback(client, on_client_event, NULL);

		/* Initialize app state */
		app_state_init(client);

		/* Initialize app work */
		app_work_init(client);

		/* Initialize DFU components */
		golioth_fw_update_init(client, _current_version);

		/* Initialize app settings */
		app_settings_init(client);

		/* Initialize app RPC */
		app_rpc_init(client);
	}
}

#ifdef CONFIG_SOC_NRF9160
static void process_lte_connected(void)
{
	/* Change the state of the Internet LED on Ostentus */
	IF_ENABLED(CONFIG_LIB_OSTENTUS, (led_internet_set(1);));

	/* Create and start a Golioth Client */
	start_golioth_client();
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


	/* Start LTE asynchronously if the nRF9160 is used
	 * Golioth Client will start automatically when LTE connects
	 */
	IF_ENABLED(CONFIG_SOC_NRF9160, (LOG_INF("Connecting to LTE, this may take some time...");
					lte_lc_init_and_connect_async(lte_handler);));

	/* If nRF9160 is not used, start the Golioth Client and block until connected */
	if (!IS_ENABLED(CONFIG_SOC_NRF9160)) {
		/* Run WiFi/DHCP if necessary */
		if (IS_ENABLED(CONFIG_GOLIOTH_SAMPLE_COMMON)) {
			net_connect();
		}

		/* Start Golioth client */
		start_golioth_client();

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

	while (true) {
		app_work_sensor_read();

		k_sleep(K_SECONDS(get_loop_delay_s()));
	}
}
