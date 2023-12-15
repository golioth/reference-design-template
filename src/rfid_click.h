/*
 * Copyright (c) 2023 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __RFID_CLICK_H__
#define __RFID_CLICK_H__

#include <stdint.h>
#include <zephyr/drivers/gpio.h>                                                                                                                                                     
#include <zephyr/drivers/spi.h>

/**
 * @brief RFID control bytes.
 * @details Specified register for description of RFID Click driver.
 */
#define RFID_SEND_CMD_CRTL  0x00
#define RFID_RESET_CTRL     0x01
#define RFID_READ_CTRL      0x02
#define RFID_POLL_CTRL      0x03

/**
 * @brief RFID commands.
 * @details Specified setting for description of RFID Click driver.
 */
#define RFID_IDN            0x01
#define RFID_PROT_SELECT    0x02
#define RFID_SEND_RECV      0x04
#define RFID_IDLE           0x07
#define RFID_RD_WAKEUP_REG  0x08
#define RFID_WR_WAKEUP_REG  0x09
#define RFID_SET_BAUD       0x0A
#define RFID_ECHO           0x55

/**
 * @brief RFID protocols.
 * @details RFID protocol settings.
 */
#define RFID_FIELD_OFF      0x00
#define RFID_ISO_15693      0x01
#define RFID_ISO_14443A     0x02    
#define RFID_ISO_14443B     0x03
#define RFID_ISO_18092NFC   0x04

/**
 * @brief RFID responses.
 * @details Expected RFID responses to commands.
 */
#define RFID_DATA_NOT_READY 0x00
#define RFID_TAG_DETECTED   0x02
#define RFID_DATA_READY     0x08

/**
 * @brief Error codes.
 * @details Possible error codes and their meaning.
 */

#define EEmdSOFerror23		0x63
#define EEmdSOFerror10		0x65
#define EEmdEgt			0x66
#define ETr1TooBigToolong	0x67
#define ETr1ToosmallToosmall	0x68
#define EFrameRecvOK		0x80
#define EinternalError		0x71
#define EUserStop		0x85
#define ECommError		0x86
#define EFrameWaitTOut		0x87
#define EInvalidSof		0x88
#define EBufOverflow		0x89
#define EFramingError		0x8A
#define EEgtError		0x8B
#define EInvalidLen		0x8C
#define ECrcError		0x8D
#define ERecvLost		0x8E
#define ENoField		0x8F
#define EUnintByte		0x90

void init_rfid_click(const struct spi_dt_spec *spi_dev, const struct gpio_dt_spec *rfid_data_ready_pin, const struct gpio_dt_spec *rfid_wakeup_pin);
int rfid_check_echo(const struct spi_dt_spec *spi_dev);
int rfid_detect_tag(const struct spi_dt_spec *spi_dev);
int rfid_get_tag_uid(const struct spi_dt_spec *spi_dev, uint32_t *uuid);

#endif
