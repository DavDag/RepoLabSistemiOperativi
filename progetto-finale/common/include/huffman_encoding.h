#pragma once

#ifndef HUFFMAN_ENCODING_H
#define HUFFMAN_ENCODING_H

// https://en.wikipedia.org/wiki/Huffman_coding

#include <stdlib.h>

// Result type of encoding / decoding
typedef struct {
    size_t size; // Length of data
    char* data;  // Compressed / Decompressed data
    double time; // Time elapsed
} HuffCodingResult_t;

/*
 * Function that returns a newly allocated buffer which contains compressed data.
 */
HuffCodingResult_t compress_data(const char* data, size_t size);

/*
 * Function that returns a newly allocated buffer which contains decompressed data.
 */
HuffCodingResult_t decompress_data(const char* data, size_t size);

#endif // HUFFMAN_ENCODING_H
