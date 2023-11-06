/*
 * Copyright (c) 2022-2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "zephyr/sys/util_macro.h"
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

#ifdef CONFIG_NET_L2_OPENTHREAD
#include <zephyr/net/openthread.h>
#include <openthread/thread.h>
#include <zephyr/net/coap.h>
#include <zephyr/net/socket.h>

static struct k_work on_connect_work;
static struct k_work on_disconnect_work;

static void on_ot_connect(struct k_work *item)
{
	ARG_UNUSED(item);

	LOG_INF("OpenThread on connect");
}

static void on_ot_disconnect(struct k_work *item)
{
	ARG_UNUSED(item);

	LOG_INF("OpenThread on disconnect");
}

static void on_thread_state_changed(otChangedFlags flags, struct openthread_context *ot_context,
				    void *user_data)
{
	if (flags & OT_CHANGED_THREAD_ROLE) {
		switch (otThreadGetDeviceRole(ot_context->instance)) {
		case OT_DEVICE_ROLE_CHILD:
		case OT_DEVICE_ROLE_ROUTER:
		case OT_DEVICE_ROLE_LEADER:
			k_work_submit(&on_connect_work);
			break;

		case OT_DEVICE_ROLE_DISABLED:
		case OT_DEVICE_ROLE_DETACHED:
		default:
			k_work_submit(&on_disconnect_work);
			break;
		}
	}
}

static struct openthread_state_changed_cb ot_state_chaged_cb = {
	.state_changed_cb = on_thread_state_changed
};
#endif

static struct golioth_client *client = GOLIOTH_SYSTEM_CLIENT_GET();

K_SEM_DEFINE(connected, 0, 1);
K_SEM_DEFINE(dfu_status_unreported, 1, 1);

static k_tid_t _system_thread = 0;

static const struct gpio_dt_spec golioth_led = GPIO_DT_SPEC_GET(DT_ALIAS(golioth_led), gpios);
static const struct gpio_dt_spec user_btn = GPIO_DT_SPEC_GET(DT_ALIAS(user_btn), gpios);

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


int main(void)
{
	int err = 0;

	LOG_DBG("Start Reference Design Template sample");

	IF_ENABLED(CONFIG_NET_L2_OPENTHREAD, (
		k_work_init(&on_connect_work, on_ot_connect);
		k_work_init(&on_disconnect_work, on_ot_disconnect);

		openthread_state_changed_cb_register(openthread_get_default_context(), &ot_state_chaged_cb);
		openthread_start(openthread_get_default_context());
	));

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

	while (true) {
		app_work_sensor_read();

		k_sleep(K_SECONDS(get_loop_delay_s()));
	}
}
