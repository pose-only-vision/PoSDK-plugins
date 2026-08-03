/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "degensac_rng.h"

#include <stdint.h>

#if defined(_MSC_VER)
#define POSDK_THREAD_LOCAL __declspec(thread)
#else
#define POSDK_THREAD_LOCAL _Thread_local
#endif

/* A non-zero default also keeps the legacy unseeded entry point well-defined. */
static POSDK_THREAD_LOCAL uint64_t posdk_degensac_rng_state =
    UINT64_C(0x9e3779b97f4a7c15);
static POSDK_THREAD_LOCAL uint64_t posdk_degensac_rng_draws = 0;
static POSDK_THREAD_LOCAL uint64_t posdk_degensac_rng_trace =
    UINT64_C(0xcbf29ce484222325);

void posdk_degensac_srand(unsigned int seed)
{
    /* SplitMix's increment avoids xorshift's forbidden all-zero state. */
    posdk_degensac_rng_state =
        (uint64_t)seed + UINT64_C(0x9e3779b97f4a7c15);
}

void posdk_degensac_begin_run(unsigned int seed)
{
    posdk_degensac_rng_draws = 0;
    posdk_degensac_rng_trace =
        UINT64_C(0xcbf29ce484222325) ^ (uint64_t)seed;
    posdk_degensac_srand(seed);
}

int posdk_degensac_rand(void)
{
    uint64_t value = posdk_degensac_rng_state;
    value ^= value >> 12;
    value ^= value << 25;
    value ^= value >> 27;
    posdk_degensac_rng_state = value;
    value *= UINT64_C(0x2545f4914f6cdd1d);
    const int result = (int)((value >> 33) & UINT64_C(0x7fffffff));
    posdk_degensac_rng_trace ^= (uint64_t)(unsigned int)result;
    posdk_degensac_rng_trace *= UINT64_C(0x100000001b3);
    ++posdk_degensac_rng_draws;
    return result;
}

uint64_t posdk_degensac_rng_draw_count(void)
{
    return posdk_degensac_rng_draws;
}

uint64_t posdk_degensac_rng_fingerprint(void)
{
    return posdk_degensac_rng_trace;
}
