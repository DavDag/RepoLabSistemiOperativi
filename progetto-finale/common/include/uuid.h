#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/** 
 * " The probability to find a duplicate within 103 trillion version-4 UUIDs is one in a billion. "
 * 
 * COLLISION PERCENTAGE:
 *    AFTER 103'000'000'000'000 uuids (v4) = 0.00000001 %
 * 
 * Canonical textual representation:
 *    xxxxxxxx-xxxx-Mxxx-Nxxx-xxxxxxxxxxxx  (hex)
 * 
 * M) 4 bits for version
 * N) 1 to 3 bits for variant
 * 
 * source:
 *   https://en.wikipedia.org/wiki/Universally_unique_identifier#Version_4_(random)
 */

// Total of 128 bits.
// Using version 4 with variant 1
typedef struct { union { int i; char b[4]; } data[4]; } UUID_t;

// Ensure correct size
static_assert(sizeof(UUID_t) == 16, "Size of UUID must be 128 bits or 16 bytes");

// It uses rand() so make sure to set seed.
// Create a new UUID
static inline UUID_t UUID_new() {
    UUID_t uuid = {
        .data = {                  // Bytes #
            [0] = { .i = rand() }, // [ 0.. 3]
            [1] = { .i = rand() }, // [ 4.. 7]
            [2] = { .i = rand() }, // [ 8..11]
            [3] = { .i = rand() }, // [12..15]
        }
    };
    // Setting version (byte #6)
    uuid.data[1].b[2] &= 0x0F;
    uuid.data[1].b[2] |= 0x40;
    // Setting variant (byte #8)
    uuid.data[2].b[1] &= 0x3F;
    uuid.data[2].b[1] |= 0x80;
    return uuid;
}

/**
 * NOT THREAD-SAFE
 * 
 * Returns a ptr to a static zero-terminated-string buffer that contains the canonical
 * textual representation of the UUID.
 * 
 * xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
 */
static inline const char* UUID_to_String(UUID_t u) {
    static char _inn_buffer[37];
    // 00000000-1111-1111-2222-222233333333
    snprintf(_inn_buffer, 37, "%08x-%04x-%04x-%04x-%04x%08x",
        (u.data[0].i & 0xFFFFFFFF) >>  0, // Bytes [ 0.. 3]
        (u.data[1].i & 0xFFFF0000) >> 16, // Bytes [ 4.. 5]
        (u.data[1].i & 0x0000FFFF) >>  0, // Bytes [ 6.. 7]
        (u.data[2].i & 0xFFFF0000) >> 16, // Bytes [ 8.. 9]
        (u.data[2].i & 0x0000FFFF) >>  0, // Bytes [10..11]
        (u.data[3].i & 0xFFFFFFFF) >>  0  // Bytes [12..15]
    );
    return _inn_buffer;
}
