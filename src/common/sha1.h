/*
 *  sha1.h
 *
 *  Copyright (C) 1998, 2009
 *  Paul E. Jones <paulej@packetizer.com>
 *  All Rights Reserved
 *
 *****************************************************************************
 *  $Id: sha1.h 12 2009-06-22 19:34:25Z paulej $
 *****************************************************************************
 *
 *  Description:
 *      This class implements the Secure Hashing Standard as defined
 *      in FIPS PUB 180-1 published April 17, 1995.
 *
 *      Many of the variable names in the SHA1Context, especially the
 *      single character names, were used because those were the names
 *      used in the publication.
 *
 *      Please read the file sha1.c for more information.
 *
 *
 * Freeware Public License (FPL)
 *
 * This software is licensed as "freeware."  Permission to distribute
 * this software in source and binary forms, including incorporation
 * into other products, is hereby granted without a fee.  THIS SOFTWARE
 * IS PROVIDED 'AS IS' AND WITHOUT ANY EXPRESSED OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE.  THE AUTHOR SHALL NOT BE HELD
 * LIABLE FOR ANY DAMAGES RESULTING FROM THE USE OF THIS SOFTWARE, EITHER
 * DIRECTLY OR INDIRECTLY, INCLUDING, BUT NOT LIMITED TO, LOSS OF DATA
 * OR DATA BEING RENDERED INACCURATE.
 */

#pragma once

#include "../shared/ufotypes.h"

/**
 * This structure will hold context information for the hashing operation
 */
typedef struct SHA1Context {
	unsigned Message_Digest[5]; /* Message Digest (output) */

	unsigned Length_Low; /* Message length in bits */
	unsigned Length_High; /* Message length in bits */

	unsigned char Message_Block[64]; /* 512-bit message blocks */
	int Message_Block_Index; /* Index into message block array */

	int Computed; /* Is the digest computed? */
	int Corrupted; /* Is the message digest corrupted? */
} SHA1Context;

/*
 *  Function Prototypes
 */
void Com_SHA1Reset (SHA1Context *);
bool Com_SHA1Result (SHA1Context *);
void Com_SHA1Input (SHA1Context *, const unsigned char *, unsigned);

bool Com_SHA1File (const char *filename, char digest[41]);
bool Com_SHA1Buffer (const unsigned char *buf, unsigned int len, char digest[41]);
