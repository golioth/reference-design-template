/*
 * Copyright (c) 2022 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app_work, LOG_LEVEL_DBG);

#include <net/golioth/system_client.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <drivers/spi.h>

#include "app_work.h"
#include "app_state.h"

#define SPI_OP	SPI_OP_MODE_MASTER | SPI_MODE_CPOL | SPI_MODE_CPHA | SPI_WORD_SET(8) | SPI_LINES_SINGLE
const struct spi_dt_spec mcp3201_ch0 = SPI_DT_SPEC_GET(DT_NODELABEL(mcp3201_ch0), SPI_OP, 0);
const struct spi_dt_spec mcp3201_ch1 = SPI_DT_SPEC_GET(DT_NODELABEL(mcp3201_ch1), SPI_OP, 0);

#include "app_work.h"
#include "libostentus/libostentus.h"

static struct golioth_client *client;

static uint64_t _ontime_ch0 = 0;
static uint64_t _ontime_ch1 = 0;
static int64_t _laston_ch0 = -1;
static int64_t _laston_ch1 = -1;

/* Store two values for each ADC reading */
struct mcp3201_data {
	uint16_t val1;
	uint16_t val2;
};

/* Formatting string for sending sensor JSON to Golioth */
#define JSON_FMT	"{\"ch0\":%d,\"ch1\":%d}"
#define ADC_ENDP	"sensor"

void get_ontime(struct ontime *ot) {
	ot->ch0 = _ontime_ch0;
	ot->ch1 = _ontime_ch1;
}

/* Callback for LightDB Stream */
static int async_error_handler(struct golioth_req_rsp *rsp) {
	if (rsp->err) {
		LOG_ERR("Async task failed: %d", rsp->err);
		return rsp->err;
	}
	return 0;
}

/*
 * Validate data received from MCP3201
 */
static int process_adc_reading(uint8_t buf_data[4], struct mcp3201_data *adc_data) {
	if (buf_data[0] & 1<<5) { return -ENOTSUP; }	/* Missing NULL bit */
	uint16_t data_msb = 0;
	uint16_t data_lsb = 0;
	data_msb = buf_data[0] & 0x1F;
	data_msb |= (data_msb<<7) | (buf_data[1]>>1);
	for (uint8_t i=0; i<12; i++) {
		bool bit_set = false;
		if (i < 2) {
			if (buf_data[1] & (1<<(1-i))) { bit_set = true; }
		}
		else if (i < 10) {
			if (buf_data[2] & (1<<(2+7-i))) { bit_set = true; }
		}
		else {
			if (buf_data[3] & (1<<(10+7-i))) { bit_set = true; }
		}

		if (bit_set) { data_lsb |= (1<<i); }
	}

	adc_data->val1 = data_msb;
	adc_data->val2 = data_lsb;
	return 0;
}

static int get_adc_reading(const struct spi_dt_spec* adc_channel, struct mcp3201_data *adc_data) {
	int err;
	static uint8_t my_buffer[4] = {0};
	struct spi_buf my_spi_buffer[1];
	my_spi_buffer[0].buf = my_buffer;
	my_spi_buffer[0].len = 4;
	const struct spi_buf_set rx_buff = { my_spi_buffer, 1 };

	err = spi_read_dt(adc_channel, &rx_buff);
	if (err) {
		LOG_INF("spi_read status: %d", err);
		return err;
	}
	LOG_DBG("Received 4 bytes: %d %d %d %d",
			my_buffer[0],my_buffer[1],my_buffer[2], my_buffer[3]);

	err = process_adc_reading(my_buffer, adc_data);
	if (err == 0) {
		LOG_INF("mcp3201_ch%d received two ADC readings: 0x%04x\t0x%04x",
				adc_channel == &mcp3201_ch0 ? 0 : 1,
				adc_data->val1, adc_data->val2);
		return err;
	}

	return 0;
}

static int push_adc_to_golioth(uint16_t ch0_data, uint16_t ch1_data) {
	int err;
	char json_buf[30];

	snprintk(json_buf, sizeof(json_buf), JSON_FMT, ch0_data, ch1_data);

	err = golioth_stream_push_cb(client, ADC_ENDP,
			GOLIOTH_CONTENT_FORMAT_APP_JSON, json_buf, strlen(json_buf),
			async_error_handler, NULL);

	if (err) {
		LOG_ERR("Failed to send sensor data to Golioth: %d", err);
		return err;
	}

	return 0;
}

static void update_ontime(uint16_t adc_value, uint64_t *ontime, int64_t *laston) {
	if (adc_value == 0) {
		*ontime = 0;
		*laston = -1;
	}
	else {
		int64_t ts = k_uptime_get();
		*ontime += (*laston < 0) ? 1 : (ts - *laston);
		*laston = ts;
	}
}

/* This will be called by the main() loop */
/* Do all of your work here! */
void app_work_sensor_read(void) {
	int err;
	struct mcp3201_data ch0_data, ch1_data;

	get_adc_reading(&mcp3201_ch0, &ch0_data);
	get_adc_reading(&mcp3201_ch1, &ch1_data);

	/* Calculate the "On" time if readings are not zero */
	update_ontime(ch0_data.val1, &_ontime_ch0, &_laston_ch0);
	update_ontime(ch1_data.val1, &_ontime_ch1, &_laston_ch1);
	LOG_DBG("Ontime:\t(ch0): %lld\t(ch1): %lld", _ontime_ch0, _ontime_ch1);

	/* Send sensor data to Golioth */

	/* Two values were read for each sensor but we'll record only on form each
	 * channel as it's unlikely the two readings will be substantially
	 * different.
	 */
	push_adc_to_golioth(ch0_data.val1, ch1_data.val1);
	app_state_update_actual();
}

void app_work_init(struct golioth_client* work_client) {
	client = work_client;

	LOG_DBG("Setting up current clamp ADCs...");
	LOG_DBG("mcp3201_ch0.bus = %p", mcp3201_ch0.bus);
	LOG_DBG("mcp3201_ch0.config.cs->gpio.port = %p", mcp3201_ch0.config.cs->gpio.port);
	LOG_DBG("mcp3201_ch0.config.cs->gpio.pin = %u", mcp3201_ch0.config.cs->gpio.pin);
	LOG_DBG("mcp3201_ch1.bus = %p", mcp3201_ch1.bus);
	LOG_DBG("mcp3201_ch1.config.cs->gpio.port = %p", mcp3201_ch1.config.cs->gpio.port);
	LOG_DBG("mcp3201_ch1.config.cs->gpio.pin = %u", mcp3201_ch1.config.cs->gpio.pin);

}

