/*
 * Copyright (c) 2018-2019 Peter Bigot Consulting, LLC
 * Copyright (c) 2019-2020 Nordic Semiconductor ASA
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/logging/log.h>
#include <golioth/client.h>
#include <golioth/stream.h>

#include "battery_monitor/battery.h"
#include "../app_sensors.h"

LOG_MODULE_REGISTER(battery, LOG_LEVEL_DBG);

#define VBATT	    DT_PATH(vbatt)
#define ZEPHYR_USER DT_PATH(zephyr_user)

/* Formatting string for sending battery JSON to Golioth */
#define JSON_FMT "{\"batt_v\":%d.%03d,\"batt_lvl\":%d.%02d}"

#define LABEL_BATTERY "Battery"

#ifdef CONFIG_BOARD_THINGY52_NRF52832
/* This board uses a divider that reduces max voltage to
 * reference voltage (600 mV).
 */
#define BATTERY_ADC_GAIN ADC_GAIN_1
#else
/* Other boards may use dividers that only reduce battery voltage to
 * the maximum supported by the hardware (3.6 V)
 */
#define BATTERY_ADC_GAIN ADC_GAIN_1_6
#endif

char stream_endpoint[] = "battery";

char _batt_v_str[8] = "0.0 V";
char _batt_lvl_str[5] = "none";

/* Battery values specific to the Aludel-mini */
static const struct battery_level_point batt_levels[] = {
	/* "Curve" here eyeballed from captured data for the [Adafruit
	 * 3.7v 2000 mAh](https://www.adafruit.com/product/2011) LIPO
	 * under full load that started with a charge of 3.96 V and
	 * dropped about linearly to 3.58 V over 15 hours.  It then
	 * dropped rapidly to 3.10 V over one hour, at which point it
	 * stopped transmitting.
	 *
	 * Based on eyeball comparisons we'll say that 15/16 of life
	 * goes between 3.95 and 3.55 V, and 1/16 goes between 3.55 V
	 * and 3.1 V.
	 */

	{10000, 3950},
	{625, 3550},
	{0, 3100},
};

struct io_channel_config {
	uint8_t channel;
};

struct divider_config {
	struct io_channel_config io_channel;
	struct gpio_dt_spec power_gpios;
	/* output_ohm is used as a flag value: if it is nonzero then
	 * the battery is measured through a voltage divider;
	 * otherwise it is assumed to be directly connected to Vdd.
	 */
	uint32_t output_ohm;
	uint32_t full_ohm;
};

static const struct divider_config divider_config = {
#if DT_NODE_HAS_STATUS(VBATT, okay)
	/* clang-format off */
	.io_channel = {
		DT_IO_CHANNELS_INPUT(VBATT),
	}, /* clang-format on */
	.power_gpios = GPIO_DT_SPEC_GET_OR(VBATT, power_gpios, {}),
	.output_ohm = DT_PROP(VBATT, output_ohms),
	.full_ohm = DT_PROP(VBATT, full_ohms),
#else  /* /vbatt exists */
	/* clang-format off */
	.io_channel = {
		DT_IO_CHANNELS_INPUT(ZEPHYR_USER),
	}, /* clang-format on */
#endif /* /vbatt exists */
};

struct divider_data {
	const struct device *adc;
	struct adc_channel_cfg adc_cfg;
	struct adc_sequence adc_seq;
	int16_t raw;
};
static struct divider_data divider_data = {
#if DT_NODE_HAS_STATUS(VBATT, okay)
	.adc = DEVICE_DT_GET(DT_IO_CHANNELS_CTLR(VBATT)),
#else
	.adc = DEVICE_DT_GET(DT_IO_CHANNELS_CTLR(ZEPHYR_USER)),
#endif
};

static int divider_setup(void)
{
	const struct divider_config *cfg = &divider_config;
	const struct io_channel_config *iocp = &cfg->io_channel;
	const struct gpio_dt_spec *gcp = &cfg->power_gpios;
	struct divider_data *ddp = &divider_data;
	struct adc_sequence *asp = &ddp->adc_seq;
	struct adc_channel_cfg *accp = &ddp->adc_cfg;
	int rc;

	if (!device_is_ready(ddp->adc)) {
		LOG_ERR("ADC device is not ready %s", ddp->adc->name);
		return -ENOENT;
	}

	if (gcp->port) {
		if (!device_is_ready(gcp->port)) {
			LOG_ERR("%s: device not ready", gcp->port->name);
			return -ENOENT;
		}
		rc = gpio_pin_configure_dt(gcp, GPIO_OUTPUT_INACTIVE);
		if (rc != 0) {
			LOG_ERR("Failed to control feed %s.%u: %d", gcp->port->name, gcp->pin, rc);
			return rc;
		}
	}

	*asp = (struct adc_sequence){
		.channels = BIT(0),
		.buffer = &ddp->raw,
		.buffer_size = sizeof(ddp->raw),
		.oversampling = 4,
		.calibrate = true,
	};

#ifdef CONFIG_ADC_NRFX_SAADC
	*accp = (struct adc_channel_cfg){
		.gain = BATTERY_ADC_GAIN,
		.reference = ADC_REF_INTERNAL,
		.acquisition_time = ADC_ACQ_TIME(ADC_ACQ_TIME_MICROSECONDS, 40),
	};

	if (cfg->output_ohm != 0) {
		accp->input_positive = SAADC_CH_PSELP_PSELP_AnalogInput0 + iocp->channel;
	} else {
		accp->input_positive = SAADC_CH_PSELP_PSELP_VDD;
	}

	asp->resolution = 14;
#else /* CONFIG_ADC_var */
#error Unsupported ADC
#endif /* CONFIG_ADC_var */

	rc = adc_channel_setup(ddp->adc, accp);
	if (rc) {
		LOG_ERR("Failed to setup ADC for AIN%u: %d", iocp->channel, rc);
	} else {
		LOG_DBG("ADC setup for AIN%u complete", iocp->channel);
	}

	return rc;
}

static bool battery_ok;

static int battery_setup(void)
{
	LOG_INF("Initializing battery measurement");

	int rc = divider_setup();

	battery_ok = (rc == 0);
	if (rc) {
		LOG_ERR("Battery measurement setup failed: %d", rc);
	}

	return rc;
}

SYS_INIT(battery_setup, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

int battery_measure_enable(bool enable)
{
	int rc = -ENOENT;

	if (battery_ok) {
		const struct gpio_dt_spec *gcp = &divider_config.power_gpios;

		rc = 0;
		if (gcp->port) {
			rc = gpio_pin_set_dt(gcp, enable);
		}
	}
	return rc;
}

int battery_sample(void)
{
	int rc = -ENOENT;

	if (battery_ok) {
		struct divider_data *ddp = &divider_data;
		const struct divider_config *dcp = &divider_config;
		struct adc_sequence *sp = &ddp->adc_seq;

		rc = adc_read(ddp->adc, sp);
		sp->calibrate = false;
		if (rc == 0) {
			int32_t val = ddp->raw;

			adc_raw_to_millivolts(adc_ref_internal(ddp->adc), ddp->adc_cfg.gain,
					      sp->resolution, &val);

			if (dcp->output_ohm != 0) {
				rc = val * (uint64_t)dcp->full_ohm / dcp->output_ohm;
				LOG_DBG("raw %u ~ %u mV => %d mV", ddp->raw, val, rc);
			} else {
				rc = val;
				LOG_DBG("raw %u ~ %u mV", ddp->raw, val);
			}
		}
	}

	return rc;
}

unsigned int battery_level_pptt(unsigned int batt_mV, const struct battery_level_point *curve)
{
	const struct battery_level_point *pb = curve;

	if (batt_mV >= pb->lvl_mV) {
		/* Measured voltage above highest point, cap at maximum. */
		return pb->lvl_pptt;
	}
	/* Go down to the last point at or below the measured voltage. */
	while ((pb->lvl_pptt > 0) && (batt_mV < pb->lvl_mV)) {
		++pb;
	}
	if (batt_mV < pb->lvl_mV) {
		/* Below lowest point, cap at minimum */
		return pb->lvl_pptt;
	}

	/* Linear interpolation between below and above points. */
	const struct battery_level_point *pa = pb - 1;

	return pb->lvl_pptt +
	       ((pa->lvl_pptt - pb->lvl_pptt) * (batt_mV - pb->lvl_mV) / (pa->lvl_mV - pb->lvl_mV));
}

int read_battery_data(struct battery_data *batt_data)
{

	/* Turn on the voltage divider circuit */
	int err = battery_measure_enable(true);

	if (err) {
		LOG_ERR("Failed to enable battery measurement power: %d", err);
		return err;
	}

	/* Read the battery voltage */
	int batt_mv = battery_sample();

	if (batt_mv < 0) {
		LOG_ERR("Failed to read battery voltage: %d", batt_mv);
		return batt_mv;
	}

	/* Turn off the voltage divider circuit */
	err = battery_measure_enable(false);
	if (err) {
		LOG_ERR("Failed to disable battery measurement power: %d", err);
		return err;
	}

	batt_data->battery_voltage_mv = batt_mv;
	batt_data->battery_level_pptt = battery_level_pptt(batt_mv, batt_levels);

	return 0;
}

char *get_batt_v_str(void)
{
	return _batt_v_str;
}

char *get_batt_lvl_str(void)
{
	return _batt_lvl_str;
}

void log_battery_data(void)
{
	LOG_INF("Battery measurement: voltage=%s, level=%s", get_batt_v_str(), get_batt_lvl_str());
}

static void async_error_handler(struct golioth_client *client,
				const struct golioth_response *response,
				const char *path,
				void *arg)
{
	if (response->status != GOLIOTH_OK) {
		LOG_ERR("Failed to stream battery data: %d", response->status);
		return;
	}
}

int stream_battery_data(struct golioth_client *client, struct battery_data *batt_data)
{
	int err;
	/* {"batt_v":X.XXX,"batt_lvl":XXX.XX} */
	char json_buf[35];

	/* Send battery data to Golioth */
	snprintk(json_buf, sizeof(json_buf), JSON_FMT, batt_data->battery_voltage_mv / 1000,
		 batt_data->battery_voltage_mv % 1000, batt_data->battery_level_pptt / 100,
		 batt_data->battery_level_pptt % 100);
	LOG_DBG("%s", json_buf);

	err = golioth_stream_set_async(client,
				       stream_endpoint,
				       GOLIOTH_CONTENT_TYPE_JSON,
				       json_buf,
				       strlen(json_buf),
				       async_error_handler,
				       NULL);
	if (err) {
		LOG_ERR("Failed to send battery data to Golioth: %d", err);
	}

	return 0;
}

int read_and_report_battery(struct golioth_client *client)
{
	int err;
	struct battery_data batt_data;

	err = read_battery_data(&batt_data);
	if (err) {
		LOG_ERR("Error reading battery data");
		return err;
	}

	/* Format as global string for easy access */
	snprintk(_batt_v_str, sizeof(_batt_v_str), "%d.%03d V", batt_data.battery_voltage_mv / 1000,
		 batt_data.battery_voltage_mv % 1000);
	snprintk(_batt_lvl_str, sizeof(_batt_lvl_str), "%d%%", batt_data.battery_level_pptt / 100);

	log_battery_data();

	if (golioth_client_is_connected(client)) {
		err = stream_battery_data(client, &batt_data);
		if (err) {
			LOG_ERR("Error streaming battery info");
			return err;
		}
	}

	return 0;
}
