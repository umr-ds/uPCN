/*
 * hal_crypto.c
 *
 * Description: contains the POSIX implementation of the hardware
 * abstraction layer interface for crypto-related functionality
 * (i.e. hashing, CRC, SHA)
 *
 */

#include "hal_crypto.h"
#include "drv/sha2.h"
#include "drv/hmac_sha2.h"
#include "drv/crc32.h"

void hal_hash(uint8_t *message, uint32_t message_length, uint8_t hash[])
{
	sha256(message, message_length, hash);
}


void hal_hash_hmac(uint8_t *key, uint32_t key_length, uint8_t *message,
		   uint32_t message_length, uint8_t hash[])
{
	hmac_sha256(key, key_length, message, message_length,
		hash, UPCN_HASH_LENGTH);
}


uint32_t hal_crc32(uint32_t *input, size_t len)
{
	uint32_t buffer;

	return crc32(*input, &buffer, 32);
}


uint32_t hal_crc32_8(uint8_t *input, size_t len)
{
	uint32_t buffer;

	return crc32(*input, &buffer, 8);
}


void hal_hash_init(void)
{
	/* in this posix implementation, no initialisation is required */
}


void hal_crc_init(void)
{
	/* in this posix implementation, no initialisation is required */
}
