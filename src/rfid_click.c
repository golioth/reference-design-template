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
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(rfid_click, LOG_LEVEL_DBG);

#include <zephyr/drivers/gpio.h>                                                                                                                                                     
#include <zephyr/drivers/spi.h>

#include "rfid_click.h"

#define RFID_STACK 2048
#define BUFF_SIZE 20

static const struct gpio_dt_spec *wakeup_pin;
static const struct gpio_dt_spec *data_ready_pin;

static uint8_t tx_buffer[BUFF_SIZE];
static uint8_t rx_buffer[BUFF_SIZE];

static struct spi_buf tx_buf = {
	.buf = tx_buffer,
	.len = sizeof(tx_buffer)
};

static struct spi_buf_set tx = {
	.buffers = &tx_buf,
	.count = 1
};

static struct spi_buf rx_buf = {
	.buf = rx_buffer,
	.len = sizeof(rx_buffer),
};

static struct spi_buf_set rx = {
	.buffers = &rx_buf,
	.count = 1
};

static void rfid_wakeup(void)
{
	gpio_pin_set_dt(wakeup_pin, 1);
	k_sleep(K_USEC(200));
	gpio_pin_set_dt(wakeup_pin, 0);
}


/* Wait for CR95HF to finish processing */
static void wait_for_data_ready(void)
{
	while(!gpio_pin_get_dt(data_ready_pin)) {
		k_sleep(K_USEC(200));
	}
}

/* Read data from CR95HF */
static int rfid_read_data(const struct spi_dt_spec *spi_dev, uint8_t *rx_buff, uint8_t len)
{
	int ret = 0;
	
	tx_buf.len = len;	
	tx_buffer[0] = RFID_READ_CTRL;
	rx_buf.len = len;
	spi_transceive_dt(spi_dev, &tx, &rx);

	memcpy(rx_buff, rx_buffer, len);

	return ret;
}

/* Send command to CR95HF */
static int rfid_send_command(const struct spi_dt_spec *spi_dev, uint8_t cmd, uint8_t *tx_buff, uint8_t len)
{
	int ret = 0;

	tx_buffer[0] = RFID_SEND_CMD_CRTL;
	tx_buffer[1] = cmd;
	tx_buffer[2] = len;

	for(uint8_t idx=0; idx<len; idx++) {
		tx_buffer[idx + 3] = tx_buff[idx];
	}

	tx_buf.len = (len + 3);	
	spi_write_dt(spi_dev, &tx);

	return ret;
}

/* Select ISO 14443-A protocol */
static void rfid_select_protocol(const struct spi_dt_spec *spi_dev)
{
	uint8_t tx_buff[2];
	tx_buff[0] = RFID_ISO_14443A;
	tx_buff[1] = 0x00;
	
	rfid_send_command(spi_dev, RFID_PROT_SELECT, tx_buff, 2);

	wait_for_data_ready();

	uint8_t rx_buff[3];
	rfid_read_data(spi_dev, rx_buff, 3);
}

/* Send Reset command to CR95HF */
static void rfid_reset(const struct spi_dt_spec *spi_dev)
{
	tx_buf.len = 1;	
	tx_buffer[0] = RFID_RESET_CTRL;
	
	spi_write_dt(spi_dev, &tx);
}

void init_rfid_click(const struct spi_dt_spec *spi_dev,
		     const struct gpio_dt_spec *rfid_data_ready_pin,
		     const struct gpio_dt_spec *rfid_wakeup_pin)
{
	/* configure CS and RFID control pins */
	wakeup_pin = rfid_wakeup_pin;
	data_ready_pin = rfid_data_ready_pin;

	gpio_pin_configure_dt(wakeup_pin, GPIO_OUTPUT);
	gpio_pin_configure_dt(data_ready_pin, GPIO_INPUT);
	
	rfid_wakeup();
	rfid_reset(spi_dev);
	k_sleep(K_SECONDS(1));
	rfid_wakeup();
}

int rfid_check_echo(const struct spi_dt_spec *spi_dev)
{
	int ret = 0;

	tx_buf.len = 2;	
	tx_buffer[0] = RFID_SEND_CMD_CRTL;
	tx_buffer[1] = RFID_ECHO;

	spi_write_dt(spi_dev, &tx);
	k_sleep(K_USEC(30));
	
	wait_for_data_ready();
	
	tx_buf.len = 2;	
	rx_buf.len = 2;
	tx_buffer[0] = 0x02;
	
	spi_transceive_dt(spi_dev, &tx, &rx);

	if (rx_buffer[1] != RFID_ECHO) {
		ret = -1;
	}

	return ret;
}

int rfid_detect_tag(const struct spi_dt_spec *spi_dev)
{
	// IDLE Command parameters
	uint8_t tx_buff[14] = {0x0B, 0x21, 0x00, 0x78, 0x01, 0x18, 0x00, 0x20, 0x60, 0x60, 0x64, 0x74, 0x3F, 0x01};
	rfid_send_command(spi_dev, RFID_IDLE, tx_buff, 14);
	
	wait_for_data_ready();

	uint8_t rx_buff[4];
	rfid_read_data(spi_dev, rx_buff, 4);
	if(rx_buff[3] == RFID_TAG_DETECTED) {
		return -1;
	}

	return 0;

}

int rfid_get_tag_uid (const struct spi_dt_spec *spi_dev, uint32_t *uuid) 
{
	int ret = 0;

	rfid_select_protocol(spi_dev);
	k_sleep(K_MSEC(100));

	uint8_t tx_buff[3];
	tx_buff[0] = 0x26;  
	tx_buff[1] = 0x07;  
	rfid_send_command(spi_dev, RFID_SEND_RECV, tx_buff, 2);
	
	wait_for_data_ready();

	uint8_t rx_buff[11];
	ret = rfid_read_data(spi_dev, rx_buff, 7);
	if(rx_buff[1] != EFrameRecvOK) {
		return -1;
	}

	tx_buff[0] = 0x93;  
	tx_buff[1] = 0x20;
	tx_buff[2] = 0x08;
	rfid_send_command(spi_dev, RFID_SEND_RECV, tx_buff, 3);
	k_sleep(K_MSEC(100));
	
	wait_for_data_ready();
	
	rfid_read_data(spi_dev, rx_buff, 11);
	if(rx_buff[1] != EFrameRecvOK) {
		return -1;
	}

	*uuid = rx_buff[3] << 24 | rx_buff[4] << 16 | rx_buff[5] << 8  | rx_buff[6];
	
	return 0;

}

