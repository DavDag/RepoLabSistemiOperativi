#pragma once

#ifndef MURMUR_HASH_2_H
#define MURMUR_HASH_2_H

#define DEFAULT_MM2_HASH_SEED 0

#include <assert.h>

typedef unsigned int HashValue;
static_assert(sizeof(int) == 4, "Must be 4 bytes");

// code based on
// https://github.com/aappleby/smhasher/blob/master/src/MurmurHash2.cpp

static inline HashValue hash_string(const char * string, int len)
{
    const unsigned int m = 0x5bd1e995;
    const int r = 24;
    unsigned int h = DEFAULT_MM2_HASH_SEED ^ len;

    while(len >= 4) {
        unsigned int k = * ((unsigned int*) string);

        k *= m;
        k ^= k >> r;
        k *= m;

        h *= m;
        h ^= k;

        string += 4;
        len -= 4;
    }

    switch(len) {
        case 3: h ^= string[2] << 16;
        case 2: h ^= string[1] << 8;
        case 1: h ^= string[0];
        h *= m;
    }

    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;

    return h;
}

#endif // MURMUR_HASH_2_H
