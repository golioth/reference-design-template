/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

void wake_system_thread(void);

#define LABEL_UP_COUNTER "Counter"
#define LABEL_DN_COUNTER "Anti-counter"
#define SUMMARY_TITLE	 "Counters:"

/**
 * Each Ostentus slide needs a unique key. You may add additional slides by
 * inserting elements with the name of your choice to this enum.
 */
typedef enum {
	UP_COUNTER,
	DN_COUNTER,
} slide_key;
