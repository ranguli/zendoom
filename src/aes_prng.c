//
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
//
// This implements a cryptographically secure pseudorandom number
// generator for implementing secure demos. The approach taken is to
// use the AES (Rijndael) stream cipher in "counter" mode, encrypting
// an incrementing counter. The cipher key acts as the random seed.
// Cryptanalysis of AES used in this way has shown it to be an
// effective PRNG (see: Empirical Evidence concerning AES, Hellekalek
// & Wegenkittl, 2003).
//
// AES implementation is taken from the Linux kernel's AES
// implementation, found in crypto/aes_generic.c. It has been hacked
// up to work independently.
//

#include <stdint.h>

#include "aes_prng.h"
#include "doomtype.h"
#include "i_swap.h"

/*
 * Cryptographic API.
 *
 * AES Cipher Algorithm.
 *
 * Based on Brian Gladman's code.
 *
 * Linux developers:
 *  Alexander Kjeldaas <astor@fast.no>
 *  Herbert Valerio Riedel <hvr@hvrlab.org>
 *  Kyle McMartin <kyle@debian.org>
 *  Adam J. Richter <adam@yggdrasil.com> (conversion to 2.5 API).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * ---------------------------------------------------------------------------
 * Copyright (c) 2002, Dr Brian Gladman <brg@gladman.me.uk>, Worcester, UK.
 * All rights reserved.
 *
 * LICENSE TERMS
 *
 * The free distribution and use of this software in both source and binary
 * form is allowed (with or without changes) provided that:
 *
 *   1. distributions of this source code include the above copyright
 *      notice, this list of conditions and the following disclaimer;
 *
 *   2. distributions in binary form include the above copyright
 *      notice, this list of conditions and the following disclaimer
 *      in the documentation and/or other associated materials;
 *
 *   3. the copyright holder's name is not used to endorse products
 *      built using this software without specific written permission.
 *
 * ALTERNATIVELY, provided that this notice is retained in full, this product
 * may be distributed under the terms of the GNU General Public License (GPL),
 * in which case the provisions of the GPL apply INSTEAD OF those given above.
 *
 * DISCLAIMER
 *
 * This software is provided 'as is' with no explicit or implied warranties
 * in respect of its properties, including, but not limited to, correctness
 * and/or fitness for purpose.
 * ---------------------------------------------------------------------------
 */

#define AES_MIN_KEY_SIZE        16
#define AES_MAX_KEY_SIZE        32
#define AES_KEYSIZE_128         16
#define AES_KEYSIZE_192         24
#define AES_KEYSIZE_256         32
#define AES_BLOCK_SIZE          16
#define AES_MAX_KEYLENGTH       (15 * 16)
#define AES_MAX_KEYLENGTH_U32   (AES_MAX_KEYLENGTH / sizeof(uint32_t))

/*
 * Please ensure that the first two fields are 16-byte aligned
 * relative to the start of the structure, i.e., don't move them!
 */
typedef struct
{
    uint32_t key_enc[AES_MAX_KEYLENGTH_U32];
    uint32_t key_dec[AES_MAX_KEYLENGTH_U32];
    uint32_t key_length;
} aes_context_t;
