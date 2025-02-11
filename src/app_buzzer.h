/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __APP_BUZZER_H__
#define __APP_BUZZER_H__

int app_buzzer_init(void);
void play_beep_once(void);

#if defined(CONFIG_BOARD_THINGY91_NRF9160_NS) || defined(CONFIG_BOARD_ALUDEL_ELIXIR_NRF9160_NS)
void play_funkytown_once(void);
void play_mario_once(void);
void play_golioth_once(void);
#endif /* CONFIG_BOARD_THINGY91_NRF9160_NS */

#endif /* __APP_BUZZER__H__ */
