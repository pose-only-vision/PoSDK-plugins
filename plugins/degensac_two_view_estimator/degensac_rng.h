/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Seed the DEGENSAC sampler owned by the current thread.
 *
 * The vendored implementation historically used the process-wide C random
 * state. Keeping the state thread-local makes one estimator invocation
 * reproducible without changing another concurrently executing view pair.
 */
void posdk_degensac_srand(unsigned int seed);

/** Start one independently traceable seeded backend invocation. */
void posdk_degensac_begin_run(unsigned int seed);

/** Return the next deterministic value in the inclusive range [0, 2^31 - 1]. */
int posdk_degensac_rand(void);

/** Number of random values consumed by the current thread's active run. */
uint64_t posdk_degensac_rng_draw_count(void);

/** Stable fingerprint of the random values consumed by the active run. */
uint64_t posdk_degensac_rng_fingerprint(void);

#ifdef __cplusplus
}
#endif
