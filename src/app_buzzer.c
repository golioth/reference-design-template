/*
 * Copyright (c) 2024 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#if defined(CONFIG_BOARD_ALUDEL_ELIXIR_NRF9160_NS)

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app_buzzer, LOG_LEVEL_DBG);

#include <stdlib.h>
#include <zephyr/device.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>

#define FUNKYTOWN_NOTES 13
#define MARIO_NOTES	37
#define GOLIOTH_NOTES	21

#define BUZZER_MAX_FREQ 2500
#define BUZZER_MIN_FREQ 75

#define sixteenth 38
#define eigth	  75
#define quarter	  150
#define half	  300
#define whole	  600

#define C4  262
#define Db4 277
#define D4  294
#define Eb4 311
#define E4  330
#define F4  349
#define Gb4 370
#define G4  392
#define Ab4 415
#define A4  440
#define Bb4 466
#define B4  494
#define C5  523
#define Db5 554
#define D5  587
#define Eb5 622
#define E5  659
#define F5  698
#define Gb5 740
#define G5  784
#define Ab5 831
#define A5  880
#define Bb5 932
#define B5  988
#define C6  1046
#define Db6 1109
#define D6  1175
#define Eb6 1245
#define E6  1319
#define F6  1397
#define Gb6 1480
#define G6  1568
#define Ab6 1661
#define A6  1760
#define Bb6 1865
#define B6  1976

#define REST 1

static const struct pwm_dt_spec sBuzzer = PWM_DT_SPEC_GET(DT_ALIAS(buzzer_pwm));

enum song_choice {
	beep,
	funkytown,
	mario,
	golioth
};

enum song_choice song = 3;

struct note_duration {
	int note;     /* hz */
	int duration; /* msec */
};

static struct note_duration funkytown_song[FUNKYTOWN_NOTES] = {
	{.note = C5, .duration = quarter},
	{.note = REST, .duration = eigth},
	{.note = C5, .duration = quarter},
	{.note = Bb4, .duration = quarter},
	{.note = C5, .duration = quarter},
	{.note = REST, .duration = quarter},
	{.note = G4, .duration = quarter},
	{.note = REST, .duration = quarter},
	{.note = G4, .duration = quarter},
	{.note = C5, .duration = quarter},
	{.note = F5, .duration = quarter},
	{.note = E5, .duration = quarter},
	{.note = C5, .duration = quarter}};

static struct note_duration mario_song[MARIO_NOTES] = {
	{.note = E6, .duration = quarter},
	{.note = REST, .duration = eigth},
	{.note = E6, .duration = quarter},
	{.note = REST, .duration = quarter},
	{.note = E6, .duration = quarter},
	{.note = REST, .duration = quarter},
	{.note = C6, .duration = quarter},
	{.note = E6, .duration = half},
	{.note = G6, .duration = half},
	{.note = REST, .duration = quarter},
	{.note = G4, .duration = whole},
	{.note = REST, .duration = whole},
	/* break in sound */
	{.note = C6, .duration = half},
	{.note = REST, .duration = quarter},
	{.note = G5, .duration = half},
	{.note = REST, .duration = quarter},
	{.note = E5, .duration = half},
	{.note = REST, .duration = quarter},
	{.note = A5, .duration = quarter},
	{.note = REST, .duration = quarter},
	{.note = B5, .duration = quarter},
	{.note = REST, .duration = quarter},
	{.note = Bb5, .duration = quarter},
	{.note = A5, .duration = half},
	{.note = G5, .duration = quarter},
	{.note = E6, .duration = quarter},
	{.note = G6, .duration = quarter},
	{.note = A6, .duration = half},
	{.note = F6, .duration = quarter},
	{.note = G6, .duration = quarter},
	{.note = REST, .duration = quarter},
	{.note = E6, .duration = quarter},
	{.note = REST, .duration = quarter},
	{.note = C6, .duration = quarter},
	{.note = D6, .duration = quarter},
	{.note = B5, .duration = quarter}};

static struct note_duration golioth_song[] = {
	{.note = C6, .duration = quarter},
	{.note = REST, .duration = 100},
	{.note = G5, .duration = 100},
	{.note = A5, .duration = 100},
	{.note = Bb5, .duration = 100},
	{.note = REST, .duration = 100},
	{.note = Bb5, .duration = 100},
	{.note = REST, .duration = quarter},
	{.note = C5, .duration = half},
	{.note = REST, .duration = half},
	{.note = REST, .duration = quarter},
	{.note = C6, .duration = quarter}
};

/* Thread plays song on buzzer */

K_SEM_DEFINE(buzzer_initialized_sem, 0, 1); /* Wait until buzzer is ready */

#define BUZZER_STACK 1024

extern void buzzer_thread(void *d0, void *d1, void *d2)
{
	/* Block until buzzer is available */
	k_sem_take(&buzzer_initialized_sem, K_FOREVER);
	while (1) {
		switch (song) {
		case 0:
			LOG_DBG("beep");
			pwm_set_dt(&sBuzzer, PWM_HZ(1000), PWM_HZ(1000) / 2);
			k_msleep(100);
			break;
		case 1:
			LOG_DBG("funkytown");
			for (int i = 0; i < FUNKYTOWN_NOTES; i++) {
				if (funkytown_song[i].note < 10) {
					/* Low frequency notes represent a 'pause' */
					pwm_set_pulse_dt(&sBuzzer, 0);
					k_msleep(funkytown_song[i].duration);
				} else {
					pwm_set_dt(&sBuzzer, PWM_HZ(funkytown_song[i].note),
						   PWM_HZ((funkytown_song[i].note)) / 2);
					k_msleep(funkytown_song[i].duration);
				}
			}
			break;

		case 2:
			LOG_DBG("mario");
			for (int i = 0; i < MARIO_NOTES; i++) {
				if (mario_song[i].note < 10) {
					/* Low frequency notes represent a 'pause' */
					pwm_set_pulse_dt(&sBuzzer, 0);
					k_msleep(mario_song[i].duration);
				} else {
					pwm_set_dt(&sBuzzer, PWM_HZ(mario_song[i].note),
						   PWM_HZ((mario_song[i].note)) / 2);
					k_msleep(mario_song[i].duration);
				}
			}
			break;
		case 3:
			LOG_DBG("golioth");
			for (int i = 0; i < (sizeof(golioth_song) / sizeof(golioth_song[1])); i++) {
				if (golioth_song[i].note < 10) {
					/* Low frequency notes represent a 'pause' */
					pwm_set_pulse_dt(&sBuzzer, 0);
					k_msleep(golioth_song[i].duration);
				} else {
					pwm_set_dt(&sBuzzer, PWM_HZ(golioth_song[i].note),
						   PWM_HZ((golioth_song[i].note)) / 2);
					k_msleep(golioth_song[i].duration);
				}
			}
			break;
		default:
			LOG_WRN("invalid switch state");
			break;
		}

		/* turn buzzer off (pulse duty to 0) */
		pwm_set_pulse_dt(&sBuzzer, 0);

		/* Sleep thread until awoken externally */
		k_sleep(K_FOREVER);
	}
}

int app_buzzer_init(void)
{
	if (!device_is_ready(sBuzzer.dev)) {
		return -ENODEV;
	}
	k_sem_give(&buzzer_initialized_sem);
	return 0;
}

K_THREAD_DEFINE(buzzer_tid, BUZZER_STACK, buzzer_thread, NULL, NULL, NULL,
		K_LOWEST_APPLICATION_THREAD_PRIO, 0, 0);

void play_beep_once(void)
{
	song = beep;
	k_wakeup(buzzer_tid);
}

void play_funkytown_once(void)
{
	song = funkytown;
	k_wakeup(buzzer_tid);
}

void play_mario_once(void)
{
	song = mario;
	k_wakeup(buzzer_tid);
}

void play_golioth_once(void)
{
	song = golioth;
	k_wakeup(buzzer_tid);
}

#else

int app_buzzer_init(void)
{
	return 0;
}

void play_beep_once(void)
{
	return;
}

#endif /* CONFIG_BOARD_THINGY91_NRF9160_NS */
