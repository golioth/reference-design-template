/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "zephyr/device.h"
#include "zephyr/devicetree.h"
#include "zephyr/kernel.h"
#include <string.h>
#include <sys/_stdint.h>
#include <zephyr/drivers/gpio.h>                                                                                                                                                     
#include <zephyr/drivers/spi.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app_rfid, LOG_LEVEL_DBG);
#include "rfid_click.h"

#define RFID_STACK 2048
extern void rfid_thread(void *d0, void *d1, void *d2);
K_THREAD_DEFINE(rfid_tid, RFID_STACK, rfid_thread, NULL, NULL, NULL,
		K_LOWEST_APPLICATION_THREAD_PRIO, 0, 0);

#define SPI_OP	SPI_OP_MODE_MASTER | SPI_WORD_SET(8) | SPI_LINES_SINGLE | SPI_FULL_DUPLEX
const struct spi_dt_spec spi_dev = SPI_DT_SPEC_GET(DT_NODELABEL(rfid_click), SPI_OP, 0);
static const struct gpio_dt_spec rfid_wakeup_pin = GPIO_DT_SPEC_GET(DT_ALIAS(rfid_wakeup_pin), gpios);
static const struct gpio_dt_spec rfid_data_ready_pin = GPIO_DT_SPEC_GET(DT_ALIAS(rfid_data_ready_pin), gpios);
static const struct gpio_dt_spec rfid_led = GPIO_DT_SPEC_GET(DT_ALIAS(rfid_led), gpios);

// ToDo: wait_for_data_ready is a blocking function, solve with IRQ
// ToDO: create a msgq for UUID exchange between rfid_thread and a consumer
// ToDo: split to drivers/CR95HN, and sensors/rfid_click
extern void rfid_thread(void *d0, void *d1, void *d2)
{
	if(!spi_is_ready_dt(&spi_dev)) {
		LOG_ERR("SPI master device not ready!\n");
	}

	/* Initialize Golioth logo LED */
	int err = gpio_pin_configure_dt(&rfid_led, GPIO_OUTPUT_INACTIVE);
	if (err) {
		LOG_ERR("Unable to configure RFID LED");
	}

	/* Initialize RFID CLick, SPI device and control pins */
	init_rfid_click(&spi_dev, &rfid_data_ready_pin, &rfid_wakeup_pin);
	k_sleep(K_MSEC(20));
	
	// wait until echo OK
	while(rfid_check_echo(&spi_dev)) {
		k_sleep(K_MSEC(500));
	}

	uint32_t uuid = 0;

	while(1) {
		while(!rfid_detect_tag(&spi_dev));
	
		gpio_pin_set_dt(&rfid_led, 1);
		
		rfid_get_tag_uid(&spi_dev, &uuid);
		LOG_INF("RFID UUID = 0x%x", uuid);
		k_sleep(K_SECONDS(1));
		
		gpio_pin_set_dt(&rfid_led, 0);
		k_sleep(K_SECONDS(1));
	}
}

