/*
 * Copyright (c) 2007, Cameron Rich
 * 
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, 
 *   this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice, 
 *   this list of conditions and the following disclaimer in the documentation 
 *   and/or other materials provided with the distribution.
 * * Neither the name of the axTLS project nor the names of its contributors 
 *   may be used to endorse or promote products derived from this software 
 *   without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * Some misc. routines to help things out
 */

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>

#include "crypto.h"

#include "FreeRTOS.h"
#include "task.h"

#define ENTROPY_POOL_SIZE 32
static uint8_t entropy_pool[ENTROPY_POOL_SIZE];

/**
 * If no /dev/urandom, then initialise the RNG with something interesting.
 */
void RNG_custom_init(const uint8_t *seed_buf, int size)
{
    int i;

    for (i = 0; i < ENTROPY_POOL_SIZE && i < size; i++)
        entropy_pool[i] ^= seed_buf[i];
}

/**
 * Set a series of bytes with a random number. Individual bytes can be 0
 */
void get_random(int num_rand_bytes, uint8_t *rand_data)
{   
 /* nothing else to use, so use a custom RNG */
    /* The method we use when we've got nothing better. Use RC4, time 
       and a couple of random seeds to generate a random sequence */
    RC4_CTX rng_ctx;
    uint32_t ticks;
    SHA1_CTX rng_digest_ctx;
    uint8_t digest[SHA1_SIZE];
    uint64_t *ep;
    int i;

    /* A proper implementation would use counters etc for entropy */
    ticks = xTaskGetTickCount();
    ep = (uint64_t *)entropy_pool;
    ep[0] ^= rand();
    ep[1] ^= ticks;

    /* use a digested version of the entropy pool as a key */
    SHA1_Init(&rng_digest_ctx);
    SHA1_Update(&rng_digest_ctx, entropy_pool, ENTROPY_POOL_SIZE);
    SHA1_Final(digest, &rng_digest_ctx);

    /* come up with the random sequence */
    RC4_setup(&rng_ctx, digest, SHA1_SIZE); /* use as a key */
    memcpy(rand_data, entropy_pool, num_rand_bytes < ENTROPY_POOL_SIZE ?
				num_rand_bytes : ENTROPY_POOL_SIZE);
    RC4_crypt(&rng_ctx, rand_data, rand_data, num_rand_bytes);

    /* move things along */
    for (i = ENTROPY_POOL_SIZE-1; i >= SHA1_SIZE ; i--)
        entropy_pool[i] = entropy_pool[i-SHA1_SIZE];

    /* insert the digest at the start of the entropy pool */
    memcpy(entropy_pool, digest, SHA1_SIZE);

}

/**
 * Set a series of bytes with a random number. Individual bytes are not zero.
 */
void get_random_NZ(int num_rand_bytes, uint8_t *rand_data)
{
    int i;
    get_random(num_rand_bytes, rand_data);

    for (i = 0; i < num_rand_bytes; i++)
    {
        while (rand_data[i] == 0)  /* can't be 0 */
            rand_data[i] = (uint8_t)(rand());
    }
}


