#include "huffman_encoding.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include <time.h>

#define MAX_CODE_LENGTH 16 // In bytes

#define COMPRESSED_FLAG     0x80
#define NOT_COMPRESSED_FLAG 0x00

// =======================================================================================================================

#define INT_TO_BYTE(i) (((BYTE) (i)) & 0x000000FF)
#define BYTE_TO_INT(i) (( (int) (i)) & 0x000000FF)

#define CHECK_BIT(var,pos) (((var) & (1<<(pos))) ? 1 : 0)

#define ELAPSED_TIME(b, e) ((e.tv_sec - b.tv_sec) + (e.tv_nsec - b.tv_nsec) * 1e-9)

// Represents a byte
typedef char BYTE;
static_assert(sizeof(BYTE) == 1, "Must be 1 byte");

// Represents the internal buffer for working better with bits

typedef struct {
    BYTE* stream;       // bytes stream
    size_t streamIndex; // stream's index
    BYTE byte;          // tmp byte
    int byteIndex;      // tmp byte's index
} HuffBuffer_t;

typedef struct {
    const BYTE* stream; // bytes stream
    size_t streamIndex; // stream's index
    BYTE byte;          // tmp byte
    int byteIndex;      // tmp byte's index
} HuffReadOnlyBuffer_t;

// Structs for the internal algorithm
typedef struct { int freq; char chr; } HuffDictEntry_t;
typedef struct { long long val; int left, right; } HuffTreeEntry_t;
typedef struct { int size; BYTE code[MAX_CODE_LENGTH]; } HuffTableEntry_t;
typedef struct { int index; HuffTableEntry_t table; } HuffQueueEntry_t;

// =======================================================================================================================

// Required for qsort
int cmp_dict_entries(const void* a, const void* b);

// Utilities to read/write bit/bytes to buffer at any location
int read_bit(HuffReadOnlyBuffer_t* buffer);
BYTE read_byte(HuffReadOnlyBuffer_t* buffer);
void write_bit(HuffBuffer_t* buffer, int bitValue);
void write_byte(HuffBuffer_t* buffer, int byteValue);

// =======================================================================================================================

HuffCodingResult_t compress_data(const BYTE* data, size_t dataSize) {
    // Result var
    HuffCodingResult_t result = {
        .data = NULL,
        .size = 0UL,
        .time = 0.0
    };
    struct timespec begin, end;

    // Check for empty stream
    if (data == NULL || dataSize == 0) return result;

    // TIMER START
    clock_gettime(CLOCK_MONOTONIC, &begin);

    // 1. Create characters dictionary
    HuffDictEntry_t dictionary[256];
    for (int i = 0; i < 256; ++i) {
        dictionary[i].chr  = INT_TO_BYTE(i);
        dictionary[i].freq = 0;
    }

    // 2. Read data stream and update dictionary frequences
    for (int i = 0; i < dataSize; ++i) {
        dictionary[BYTE_TO_INT(data[i])].freq++;
    }

    // 3. Sort dictionary on frequences
    qsort(dictionary, 256, sizeof(HuffDictEntry_t), cmp_dict_entries);

    // 4. Find index of first character not used in data
    int first0 = 256;
    for (int i = 0; i < 256; ++i) {
        if (dictionary[i].freq == 0) {
            first0 = i + 1;
            break;
        }
    }

    // 5. Create trees
    const int treeSize = first0 * 2 - 1;
    HuffTreeEntry_t tree[treeSize];
    for (int i = 0; i < treeSize; ++i) {
        tree[i].val   = (i <= first0) ? dictionary[i].freq : 0;
        tree[i].left  = -1;
        tree[i].right = -1;
    }

    // 6. Merge trees into one tree
    int treeRoot = -1;
    {
        int treeIndex = 0, tmpTreeIndexL = first0, tmpTreeIndexR = first0;
        while (treeIndex < first0 || (tmpTreeIndexL < tmpTreeIndexR - 1)) {
            // 6.1 Find two lowest roots
            int min1Index, min2Index;

            // (first)
            if (treeIndex == first0)                 min1Index = tmpTreeIndexL++;
            else if (tmpTreeIndexL == tmpTreeIndexR) min1Index = treeIndex++;
            else                                     min1Index = (tree[treeIndex].val <= tree[tmpTreeIndexL].val) ? treeIndex++ : tmpTreeIndexL++;

            // (second)
            if (treeIndex == first0)                 min2Index = tmpTreeIndexL++;
            else if (tmpTreeIndexL == tmpTreeIndexR) min2Index = treeIndex++;
            else                                     min2Index = (tree[treeIndex].val <= tree[tmpTreeIndexL].val) ? treeIndex++ : tmpTreeIndexL++;

            // 6.2 Merge roots into a single one
            tree[tmpTreeIndexR].val   = tree[min1Index].val + tree[min2Index].val;
            tree[tmpTreeIndexR].left  = min1Index;
            tree[tmpTreeIndexR].right = min2Index;
            tmpTreeIndexR++;
        }

        // 6.3 Save root
        treeRoot = tmpTreeIndexL;
    }

    // 7. Create encoding table
    HuffTableEntry_t table[256];
    memset(table, 0, 256 * sizeof(HuffTableEntry_t));

    // 8. Traverse tree and fill table
    {
        // 8.1 Create queue
        HuffQueueEntry_t queue[256];
        memset(queue, 0, 256 * sizeof(HuffQueueEntry_t));

        // 8.2 Start exploring
        HuffTableEntry_t currTable;
        int qSize = 0;
        queue[qSize++].index = treeRoot;
        while (qSize > 0) {
            // 8.2.1 vars
            qSize--;
            const int nodeInd      = queue[qSize].index;
            const int leftNodeInd  = tree[nodeInd].left;
            const int rightNodeInd = tree[nodeInd].right;
            memcpy(&currTable, &queue[qSize].table, sizeof(HuffTableEntry_t));

            // 8.2.2 Check if current node is a leaf
            if (leftNodeInd == -1 && rightNodeInd == -1) {
                // 8.2.2.1 Save code
                const int charValue = BYTE_TO_INT(dictionary[nodeInd].chr);
                memcpy(&table[charValue], &currTable, sizeof(HuffTableEntry_t));
            } else {
                // 8.2.3 Push left node into queue
                if (leftNodeInd != -1) {
                    queue[qSize].index = leftNodeInd;
                    memcpy(&queue[qSize].table, &currTable, sizeof(HuffTableEntry_t));
                    queue[qSize].table.code[queue[qSize].table.size / 8] |= (1 << (7 - (queue[qSize].table.size % 8)));
                    queue[qSize].table.size++;
                    qSize++;
                }

                // 8.2.4 Push right node into queue
                if (rightNodeInd != -1) {
                    queue[qSize].index = rightNodeInd;
                    memcpy(&queue[qSize].table, &currTable, sizeof(HuffTableEntry_t));
                    queue[qSize].table.code[queue[qSize].table.size / 8] |= (0 << (7 - (queue[qSize].table.size % 8)));
                    queue[qSize].table.size++;
                    qSize++;
                }
            }
        }
    }

    // 9. Estimate final compressed size
    size_t compressedSize = (1 + 1 + sizeof(int)) * 8 + (10 * first0 - 1);
    for (int i = 0; i < dataSize; ++i) {
        compressedSize += table[BYTE_TO_INT(data[i])].size;
    }
    const int padding = compressedSize % 8;
    compressedSize    = (compressedSize / 8) + (padding != 0);

    // Is it convenient to compress data ?
    if (compressedSize > dataSize) {
        // TIMER END
        clock_gettime(CLOCK_MONOTONIC, &end);
    
        // No, it is not.
        result.data    = (BYTE*) calloc(dataSize + 1, sizeof(BYTE));
        result.data[0] = NOT_COMPRESSED_FLAG;
        memcpy(result.data + 1, data, dataSize * sizeof(BYTE));
        result.size    = dataSize + 1;
        result.time    = ELAPSED_TIME(begin, end);
        return result;
    }
    // Yes, it is.

    // 10. Create output buffer
    HuffBuffer_t out;
    out.stream      = (BYTE*) calloc(compressedSize, sizeof(BYTE));
    out.streamIndex = 0;
    out.byte        = 0x00;
    out.byteIndex   = 0;

    // 11. Write header
    write_byte(&out, INT_TO_BYTE((COMPRESSED_FLAG | (padding))));
    write_byte(&out, (first0 - 1));
    write_byte(&out, (dataSize >> 24));
    write_byte(&out, (dataSize >> 16));
    write_byte(&out, (dataSize >>  8));
    write_byte(&out, (dataSize >>  0));

    // 12. Write tree
    {
        int queue[256], qSize = 0;
        queue[qSize++] = treeRoot;
        while (qSize > 0) {
            // 12.1 vars
            qSize--;
            const int nodeInd      = queue[qSize];
            const int leftNodeInd  = tree[nodeInd].left;
            const int rightNodeInd = tree[nodeInd].right;

            // 12.2 Check if current node is a leaf
            if (leftNodeInd == -1 && rightNodeInd == -1) {
                BYTE charValue = dictionary[nodeInd].chr;
                // 12.2.1 Write flag for leaf nodes
                write_bit(&out, 1);
                // 12.2.2 Write ascii byte representing leaf node
                write_byte(&out, charValue);
            } else {
                // 12.3 Write flag for intermediate nodes
                write_bit(&out, 0);
                // 12.4 Push right node into queue
                if (rightNodeInd != -1) {
                    queue[qSize++] = rightNodeInd;
                }
                // 12.5 Push right node into queue
                if (leftNodeInd != -1) {
                    queue[qSize++] = leftNodeInd;
                }
            }
        }
    }

    // 13. Encode content
    for (int i = 0; i < dataSize; ++i) {
        const int charToEncode = BYTE_TO_INT(data[i]);
        const int codeSize     = table[charToEncode].size;
        // TODO: Transform using bytes
        for (int j = 0; j < codeSize; ++j) {
            const int byteInd  = j / 8;
            const int bitInd   =  7 - (j % 8);
            const int bitValue = CHECK_BIT(table[charToEncode].code[byteInd], bitInd);
            write_bit(&out, bitValue);
        }
    }
    // Write last byte if needed
    if (out.byteIndex > 0) {
        out.stream[out.streamIndex++] = out.byte;
    }

    // TIMER END
    clock_gettime(CLOCK_MONOTONIC, &end);

    // 14. Set values and return
    result.data = out.stream;
    result.size = out.streamIndex;
    result.time = ELAPSED_TIME(begin, end);
    return result;
}

HuffCodingResult_t decompress_data(const BYTE* data, size_t dataSize) {
    // Result var
    HuffCodingResult_t result = {
        .data = NULL,
        .size = 0UL,
        .time = 0.0
    };
    struct timespec begin, end;

    // Check for empty stream
    if (data == NULL || dataSize == 0) return result;

    // TIMER START
    clock_gettime(CLOCK_MONOTONIC, &begin);

    // 1. Create buffer
    HuffReadOnlyBuffer_t in;
    in.streamIndex = 1;
    in.stream      = data;
    in.byteIndex   = 0;
    in.byte        = data[0];

    // 2. Read first byte of stream (header)s
    BYTE header1 = read_byte(&in);
    if (!(header1 & (COMPRESSED_FLAG))) {
        // Data is uncompressed
        result.data = (BYTE*) calloc(dataSize - 1, sizeof(BYTE));
        memcpy(result.data, data + 1, (dataSize - 1) * sizeof(BYTE));
        result.size = dataSize - 1;
        result.time = 0.0;
        return result;
    }

    // 3. Data compressed. Finish reading header
    int padding      = header1 & (0x3F);
    int charCount    = read_byte(&in) + 1;
    int originalSize =
        BYTE_TO_INT(read_byte(&in)) << 24 |
        BYTE_TO_INT(read_byte(&in)) << 16 |
        BYTE_TO_INT(read_byte(&in)) <<  8 |
        BYTE_TO_INT(read_byte(&in)) <<  0;

    // 4. Create empty tree
    int treeSize = charCount * 2;
    HuffTreeEntry_t tree[treeSize];
    for (int i = 0; i < treeSize; ++i) {
        tree[i].val   = -1;
        tree[i].left  = -1;
        tree[i].right = -1;
    }

    // 5. Read data and fill tree
    {
        int fathers[256], qSize = 0;
        fathers[qSize] = 0;
        for (int treeInd = 0; treeInd < (treeSize - 1); ++treeInd) {
            // 5.0 vars
            const int bitValue = read_bit(&in);

            // 5.1 Check if current node is a leaf node
            if (bitValue) {
                // 5.1.1 Fill tree data
                tree[treeInd].val = read_byte(&in);

                // 5.1.2 Update fathers
                int updated = 0, tmpInd = treeInd;
                do {
                    int father = fathers[qSize];
                    if (tree[father].left == -1) {
                        tree[father].left = tmpInd;
                        updated = 1;
                    } else if (tree[father].right == -1) {
                        tree[father].right = tmpInd;
                        tmpInd = father;
                        qSize--;
                    }
                } while (!updated && qSize > 0);
            } else {
                // 5.2 Push node into fathers queue
                fathers[++qSize] = treeInd;
            }
        }
    }

    HuffBuffer_t out;
    out.streamIndex = 0;
    out.stream      = (BYTE*) calloc(originalSize, sizeof(BYTE));
    out.byteIndex   = 0;
    out.byte        = 0x00;

    // 6. Decode data using tree
    {
        int treeInd = 0, extraBits = (8 - in.byteIndex);
        // 6.1 Extra bits to align at byte
        for (int i = 0; i < extraBits; ++i) {
            const int bitValue = read_bit(&in);
            treeInd = (bitValue) ? tree[treeInd].left : tree[treeInd].right;
            if (tree[treeInd].val != -1) {
                write_byte(&out, INT_TO_BYTE(tree[treeInd].val));
                treeInd = 0;
            }
        }
        // 6.2 Start decoding aligned bytes
        while (in.streamIndex < originalSize && (in.streamIndex < dataSize || (in.streamIndex == dataSize && padding == 0))) {
            const int bitValue = read_bit(&in);
            treeInd = (bitValue) ? tree[treeInd].left : tree[treeInd].right;
            if (tree[treeInd].val != -1) {
                write_byte(&out, INT_TO_BYTE(tree[treeInd].val));
                treeInd = 0;
            }
        }
        // 6.3 Handle remaining padding
        for (int i = 0; i < padding; ++i) {
            const int bitValue = read_bit(&in);
            treeInd = (bitValue) ? tree[treeInd].left : tree[treeInd].right;
            if (tree[treeInd].val != -1) {
                write_byte(&out, INT_TO_BYTE(tree[treeInd].val));
                treeInd = 0;
            }
        }
    }

    // TIMER END
    clock_gettime(CLOCK_MONOTONIC, &end);

    // 14. Set values and return
    result.data = out.stream;
    result.size = out.streamIndex;
    result.time = ELAPSED_TIME(begin, end);
    return result;
}

// =======================================================================================================================

int cmp_dict_entries(const void* a, const void* b) {
    int freqA = ((HuffDictEntry_t*) a)->freq;
    int freqB = ((HuffDictEntry_t*) b)->freq;
    if (!freqA) freqA = INT_MAX;
    if (!freqB) freqB = INT_MAX;
    return freqA - freqB;
}

int read_bit(HuffReadOnlyBuffer_t* buffer) {
    const int bitValue = CHECK_BIT(buffer->byte, (7 - buffer->byteIndex));
    ++buffer->byteIndex;
    if (buffer->byteIndex == 8) {
        buffer->byteIndex = 0;
        buffer->byte      = buffer->stream[buffer->streamIndex++];
    }
    return bitValue;
}

BYTE read_byte(HuffReadOnlyBuffer_t* buffer) {
    BYTE byte = 0x00;
    for (int i = 7; i >= 0; --i) {
        byte |= read_bit(buffer) << i;
    }
    return byte;
}

void write_bit(HuffBuffer_t* buffer, int bitValue) {
    buffer->byte |= bitValue << (7 - buffer->byteIndex);
    ++(buffer->byteIndex);
    if (buffer->byteIndex == 8) {
        buffer->stream[(buffer->streamIndex)++] = buffer->byte;
        buffer->byte                            = 0x00;
        buffer->byteIndex                       = 0;
    }
}

void write_byte(HuffBuffer_t* buffer, int byteValue) {
    for (int i = 7; i >= 0; --i) {
        const int bitValue = CHECK_BIT(byteValue, i);
        write_bit(buffer, bitValue);
    }
}
