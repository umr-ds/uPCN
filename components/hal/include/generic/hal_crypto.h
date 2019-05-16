/*
 * hal_crypto.h
 *
 * Description: contains the definitions of the hardware abstraction
 * layer interface for crypto-related functionality
 *
 */

#ifndef HAL_CRYPTO_H_INCLUDED
#define HAL_CRYPTO_H_INCLUDED

#include <stdint.h>
#include <stddef.h>


/* define the length that the generated hashes should have */
#define UPCN_HASH_LENGTH 32

/**
 * @brief hal_hash_init Initialization of underlying OS/HW for hash calculations
 */
void hal_hash_init(void);

/**
 * @brief hal_crc_init Initialization of underlying OS/HW for crc calculations
 */
void hal_crc_init(void);

/**
 * @brief hal_hash Provides a hash generation of a given message
 * @param message A pointer to the message that should be hashed
 * @param message_length The length of the given message
 * @param hash The digest???
 * @attention If some kind of initialisation is necessary, this has to be done
 *	      in the init-methods in hal_plattform.h
 */
void hal_hash(uint8_t *message, uint32_t message_length,
	  uint8_t hash[UPCN_HASH_LENGTH]);

/**
 * @brief hal_hash_hmac Provides a hmac generation of a given message
 * @param key A pointer to the key
 * @param key_length The length of the key
 * @param message A pointer to the message
 * @param message_length The length of the given message
 * @param hash The digest???
 * @attention If some kind of initialisation is necessary, this has to be done
 *	      in the init-methods in hal_plattform.h
 */
void hal_hash_hmac(uint8_t *key, uint32_t key_length, uint8_t *message,
	       uint32_t message_length,	uint8_t hash[UPCN_HASH_LENGTH]);

/**
 * @brief hal_crc32 Calculates the crc checksum of a given input
 * @param input A pointer to the input data
 * @param len The length of input data that should be taken for the
 *	      calculation (in 32-bit input values)
 * @return the calculated checksum
 * @attention If some kind of initialisation is necessary, this has to be done
 *	      in the init-methods in hal_plattform.h
 */
uint32_t hal_crc32(uint32_t *input, size_t len);

/**
 * @brief hal_crc32_8 Calculates the crc checksum of a given input
 * @param input A pointer to the input data
 * @param len The length of input data that should be taken for the
 *	      calculation (in 8-bit input values)
 * @return the calculated checksum
 * @attention If some kind of initialisation is necessary, this has to be done
 *	      in the init-methods in hal_plattform.h
 */
uint32_t hal_crc32_8(uint8_t *input, size_t len);



#endif /* HAL_CRYPTO_H_INCLUDED */
