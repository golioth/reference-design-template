#ifndef __SUPERCON_BADGE_H__
#define __SUPERCON_BADGE_H__

#include <stdint.h>

/* Sizing 36 bytes i2c packets:
 *
 * Header packet
 *    2 unique id
 *    2 blocknum (0x00, 0x00 indicates header packet)
 *    2 bytes interval between data points (in us)
 *    2 bytes total number of sample bytes (# of samples * 2 bytes)
 *    25 bytes for name (including null terminaor)
 *    3 bytes reserved for future use
 *
 * Data packets
 *    2 unique id
 *    2 blocknum (max == final block)
 *    32 bytes of data (16 stucts of 8-bit x and 8-bit y pairs)
 */

#define SCB_UID 0
#define SCB_BLOCKNUM 1
#define SCB_INTERVAL 2
#define SCB_TOTALDATA 3
#define SCB_NAME_BYTES_INDEX 8

#define SCB_POINTS_INT 2

#define SUPERCON_I2C_PACKET_SIZE 36
union superpacket {
	uint8_t bytes[SUPERCON_I2C_PACKET_SIZE];
	uint16_t points[SUPERCON_I2C_PACKET_SIZE / 2];
} typedef SuperPacket;

int process_packet(SuperPacket packet);

#endif
