/*
 * Copyright (c) 2025 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

#include "battery_monitor/battery.h"

LOG_MODULE_DECLARE(battery, LOG_LEVEL_DBG);

static const struct device *const fuel_gauge = DEVICE_DT_GET_ONE(maxim_max17262);

static bool _battery_ok;

int battery_measurement_setup(void)
{
	_battery_ok = false;

	if (0 == device_is_ready(fuel_gauge)) {
		LOG_ERR("Unable to initialize MAX17262 (fuel-gauge IC)");
		return -ENODEV;
	}

	_battery_ok = true;
	return 0;
}
int battery_read_voltage(struct battery_data *batt_data)
{
	if (false == _battery_ok) {
		return -ENODEV;
	}

	struct sensor_value voltage_v, full_uah, remaining_uah;

	sensor_sample_fetch(fuel_gauge);
	sensor_channel_get(fuel_gauge, SENSOR_CHAN_GAUGE_VOLTAGE, &voltage_v);
	sensor_channel_get(fuel_gauge, SENSOR_CHAN_GAUGE_FULL_CHARGE_CAPACITY, &full_uah);
	sensor_channel_get(fuel_gauge, SENSOR_CHAN_GAUGE_REMAINING_CHARGE_CAPACITY, &remaining_uah);

	unsigned int full_mah = (unsigned int) sensor_value_to_milli(&full_uah);
	unsigned int remaining_mah = (unsigned int) sensor_value_to_milli(&remaining_uah);

	batt_data->battery_voltage_mv = (unsigned int) sensor_value_to_milli(&voltage_v);
	batt_data->battery_level_pptt =
		(remaining_mah >= full_mah) ? 10000 : (remaining_mah * 10000) / full_mah;

	return 0;
}
