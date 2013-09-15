#pragma once

#include "../shared/ufotypes.h"

typedef struct {
	uint64_t total[2];
	uint64_t state[8];
	byte buffer[64];
} sha2_context;

/**
 * Core SHA-256 functions
 */
void Com_SHA2Starts (sha2_context *ctx);
void Com_SHA2Update (sha2_context *ctx, const byte* input, uint32_t length);
void Com_SHA2Finish (sha2_context *ctx, byte digest[32]);

/**
 * @brief Output SHA-256(file contents)
 * @return @c true if successful
 */
bool Com_SHA2File (const char* filename, byte digest[32]);

/**
 * @brief Output SHA-256(buf)
 */
void Com_SHA2Csum (const byte* buf, uint32_t buflen, byte digest[32]);

/**
 * @brief Output HMAC-SHA-256(buf,key)
 */
void Com_SHA2Hmac (const byte* buf, uint32_t buflen, const byte* key, uint32_t keylen, byte digest[32]);

void Com_SHA2ToHex (const byte digest[32], char final[65]);
