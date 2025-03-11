/*
 * Copyright (c) 2025 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Common API calls that every voltage measurement source file should implement */

#ifndef __BATTERY_MEASUREMENT_API_H_
#define __BATTERY_MEASUREMENT_API_H_

int battery_measurement_setup(void);
int battery_read_voltage(struct battery_data *batt_data);

#endif /* __BATTERY_MEASUREMENT_API_H_ */
