#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "testdata.h"
#include "huffman.h"

/**
 * 
 * HUFFMAN CODING
 * 
 * sources:
 *    https://en.wikipedia.org/wiki/Huffman_coding
 * 
 */

int main() {
    const char* const data = TEXT_SHORT;
    
    size_t size0 = strlen(data), size1 = 0, size2 = 0;
    char* compressedData = compress(data, size0, &size1);
    char* decompressedData = decompress(compressedData, size1, &size2);

    if (size0 == size2 && (strncmp(decompressedData, data, size0) == 0)) printf("Success !!\n");
    else                                                                 printf("Failure !!\n");

    free(compressedData);
    free(decompressedData);

    return 0;
}