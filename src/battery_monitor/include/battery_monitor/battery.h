/*
 * Copyright (c) 2018-2019 Peter Bigot Consulting, LLC
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef APPLICATION_BATTERY_H_
#define APPLICATION_BATTERY_H_

#include <stdint.h>
#include <stdbool.h>
#include <golioth/client.h>

/** Enable or disable measurement of the battery voltage.
 *
 * @param enable true to enable, false to disable
 *
 * @return zero on success, or a negative error code.
 */
int battery_measure_enable(bool enable);

/** Measure the battery voltage.
 *
 * @return the battery voltage in millivolts, or a negative error
 * code.
 */
int battery_sample(void);

/** A point in a battery discharge curve sequence.
 *
 * A discharge curve is defined as a sequence of these points, where
 * the first point has #lvl_pptt set to 10000 and the last point has
 * #lvl_pptt set to zero.  Both #lvl_pptt and #lvl_mV should be
 * monotonic decreasing within the sequence.
 */
struct battery_level_point {
	/** Remaining life at #lvl_mV. */
	uint16_t lvl_pptt;

	/** Battery voltage at #lvl_pptt remaining life. */
	uint16_t lvl_mV;
};

/** Calculate the estimated battery level based on a measured voltage.
 *
 * @param batt_mV a measured battery voltage level.
 *
 * @param curve the discharge curve for the type of battery installed
 * on the system.
 *
 * @return the estimated remaining capacity in parts per ten
 * thousand.
 */
unsigned int battery_level_pptt(unsigned int batt_mV, const struct battery_level_point *curve);

/** A battery voltage and level measurement.
 *
 * Battery voltage is in mV.
 * Battery level is in parts per ten thousand.
 */
struct battery_data {
	int battery_voltage_mv;
	unsigned int battery_level_pptt;
};

/**
 * @brief Get pointer to a string representation of the last read battery
 * voltage.
 *
 * This string is generated each time read_and_report_battery() is called.
 *
 * @return Pointer to character array
 */
char *get_batt_v_str(void);

/**
 * @brief Get pointer to a string representation of the last read percentage
 * level. If a level has not yet been read, this value will be `none`.
 *
 * This string is generated each time read_and_report_battery() is called.
 *
 * @return Pointer to character array
 */
char *get_batt_lvl_str(void);

/**
 * @brief Read the battery voltage and estimated level.
 *
 * @param battery_data pointer to a struct to read the battery data into.
 *
 * @return Error number or zero if successful.
 */
int read_battery_data(struct battery_data *batt_data);

/**
 * @brief Log the battery voltage and estimated level.
 *
 * @param battery_data battery data to log.
 *
 */
void log_battery_data(void);

/**
 * @brief Stream battery data to Golioth.
 *
 * @param client Golioth client to use for the Stream API call
 * @param battery_data battery data to stream to Golioth.
 *
 * @return Error number or zero if successful
 */
int stream_battery_data(struct golioth_client *client, struct battery_data *batt_data);

/**
 * @brief Read, log, stream, and display a battery measurement.
 *
 * @param client Golioth client to use for the Stream API call
 *
 * @return Error number or zero if successful
 */
int read_and_report_battery(struct golioth_client *client);

#endif /* APPLICATION_BATTERY_H_ */
