#ifndef BUNDLE7_CRC_H_INCLUDED
#define BUNDLE7_CRC_H_INCLUDED

/**
 * @brief CRC-16 (ANSI/IBM) checksum
 *
 * Polynom:         0x8005
 * Initial value:   0x0000
 * Final XOR value: 0x0000
 *
 * @param data  Pointer to byte array to perform CRC on
 * @param len   Number of bytes (8-bit) to CRC
 *
 * @return CRC checksum
 */
uint32_t bundle7_crc16(const void *data, size_t len);

/**
 * @brief CRC-32 (ANSI/Ethernet) checksum
 *
 * Polynom:         0x04c11db7
 * Initial value:   0xffffffff
 * Final XOR value: 0xffffffff
 *
 * @param data  Pointer to byte array to perform CRC on
 * @param len   Number of bytes (8-bit) to CRC
 *
 * @return CRC checksum
 */
uint32_t bundle7_crc32(const void *data, size_t len);


enum bundle7_crc_version {
	BUNDLE_V7_CRC16,  // CRC-16 ANSI/IBM
	BUNDLE_V7_CRC32,  // CRC-32 ANSI/Ethernet
};


struct bundle7_crc_stream {
	const uint32_t *table;
	union {
		uint32_t checksum;
		uint8_t bytes[4];
	};
};


void bundle7_crc_init(
	struct bundle7_crc_stream *crc,
	enum bundle7_crc_version version
);


void bundle7_crc_read(struct bundle7_crc_stream *crc, uint8_t byte);


void bundle7_crc_done(struct bundle7_crc_stream *crc);


void bundle7_crc_reset(struct bundle7_crc_stream *crc);

#endif /* BUNDLE7_CRC_H_INCLUDED */
