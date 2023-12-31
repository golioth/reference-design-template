/*
 * Copyright (c) 2022-2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(golioth_rd_template, LOG_LEVEL_DBG);

#include "main.h"

#ifdef CONFIG_LIB_OSTENTUS
#include <libostentus.h>
#endif

#include <zephyr/drivers/gpio.h>

static k_tid_t _system_thread = 0;
uint8_t counter;
bool button_reset_pressed;

static const struct gpio_dt_spec golioth_led = GPIO_DT_SPEC_GET(DT_ALIAS(golioth_led), gpios);
static const struct gpio_dt_spec user_btn = GPIO_DT_SPEC_GET(DT_ALIAS(sw1), gpios);
static struct gpio_callback button_cb_data;

/* forward declarations */
void golioth_connection_led_set(uint8_t state);

void wake_system_thread(void)
{
	k_wakeup(_system_thread);
}


void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
	LOG_DBG("Button pressed at %d", k_cycle_get_32());
	/* This function is an Interrupt Service Routine. Do not call functions that
	 * use other threads, or perform long-running operations here
	 */
	button_reset_pressed = true;
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

void init_ostentus(void)
{
	/* Clear Ostentus memory */
	clear_memory();
	/* Update Ostentus LEDS using bitmask (Power On and Battery) */
	led_bitmask(LED_POW | LED_BAT);
	/* Show Golioth Logo on Ostentus ePaper screen */
	show_splash();
}

void init_slideshow(void)
{
	/* Set up a slideshow on Ostentus
	 *  - add up to 256 slides
	 *  - use the enum in app_work.h to add new keys
	 *  - values are updated using these keys (see app_work.c)
	 */
	slide_add(UP_COUNTER, LABEL_UP_COUNTER, strlen(LABEL_UP_COUNTER));
	slide_add(DN_COUNTER, LABEL_DN_COUNTER, strlen(LABEL_DN_COUNTER));

	/* Set the title ofthe Ostentus summary slide (optional) */
	summary_title(SUMMARY_TITLE, strlen(SUMMARY_TITLE));

	/* Start Ostentus slideshow with 30 second delay between slides */
	slideshow(30000);
}

void test_fifo_and_reset(void)
{
	/* Test filling up the FIFO */
	uint8_t slots = 1;

	while (slots) {
		show_splash();
		ostentus_fifo_ready(&slots);
		LOG_INF("FIFO slots remaining: %d", slots);
	}

	/* Tests finished, reset now */
	counter = 0;
	button_reset_pressed = false;
	ostentus_reset();

	/* Wait for Ostentus to come back up */
	while (ostentus_fifo_ready(&slots)) {
		k_msleep(100);
	}

	/* Reinit */
	init_ostentus();
	init_slideshow();
}

int main(void)
{
	int err;

	LOG_DBG("Start Ostentus test");

	init_ostentus();

	/* Get system thread id so loop delay change event can wake main */
	_system_thread = k_current_get();

	/* Initialize Golioth logo LED */
	err = gpio_pin_configure_dt(&golioth_led, GPIO_OUTPUT_INACTIVE);
	if (err) {
		LOG_ERR("Unable to configure LED for Golioth Logo");
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

	init_slideshow();

	char json_buf[32];

	ostentus_version_get(json_buf, sizeof(json_buf));
	LOG_INF("Ostentus Firmware Version: %s", json_buf);

	while (true) {
		/* Reset when button is pressed or all lights but Golioth are on */
		if ((button_reset_pressed) || (counter > 0x1D)) {
			test_fifo_and_reset();
		}

		led_bitmask(counter);
		snprintk(json_buf, sizeof(json_buf), "%d", counter);
		slide_set(UP_COUNTER, json_buf, strlen(json_buf));
		snprintk(json_buf, sizeof(json_buf), "%d", 255 - counter);
		slide_set(DN_COUNTER, json_buf, strlen(json_buf));
		counter++;

		k_sleep(K_SECONDS(1));
	}
}
