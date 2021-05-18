#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>

#define DEBUG_LOG

// =======================================================================================================================

typedef char BYTE;
typedef const char CBYTE;
typedef BYTE* BYTES;
typedef CBYTE* CBYTES;

static_assert(sizeof(BYTE) == 1, "Must be 1 byte");
static_assert(sizeof(CBYTE) == 1, "Must be 1 byte");

typedef struct { int freq; BYTE chr; } HuffDictEntry_t;
typedef struct { int val, left, right; } HuffTreeEntry_t;
typedef struct { int size; unsigned int code[4]; } HuffTableEntry_t;
typedef struct { int index; HuffTableEntry_t table; } HuffQueueEntry_t;

static_assert(sizeof(int) == 4, "Must be 4 bytes");

// =======================================================================================================================

BYTE* compress(CBYTES data, size_t dataSize, size_t* outSize);
BYTE* decompress(CBYTES data, size_t dataSize, size_t* outSize);
int cmp_dict_entries(const void* a, const void* b);
void print_huff_table_entry(const HuffTableEntry_t* entry);
void print_byte_as_binary(CBYTE byte);

// =======================================================================================================================

BYTE* compress(CBYTES data, size_t dataSize, size_t* outSize) {
    if (dataSize == 0) return NULL;

    // 1. Create dictionary
    HuffDictEntry_t dict[256];
    for (int i = 0; i < 256; ++i) {
        dict[i].chr  = (BYTE) i;
        dict[i].freq = 0;
    }

    // 2. Fill dictionary
    for (int i = 0; i < dataSize; ++i) {
        dict[(int) data[i]].freq++;
    }

    // 3. Sort dictionary on freq
    qsort(dict, 256, sizeof(HuffDictEntry_t), cmp_dict_entries);

#ifdef DEBUG_LOG
    for (int i = 0; i < 256; ++i) if (dict[i].freq != 0) printf("%.3d '%c': %.4d\n", i, dict[i].chr, dict[i].freq);
#endif

    // 4. Find first value set to 0
    int first0 = 0;
    for (int i = 0; i < 256; ++i) {
        if (dict[i].freq == 0) {
            first0 = i + 1;
            break;
        }
    }

    // 5. Create trees
    HuffTreeEntry_t tree[first0 * 2];
    for (int i = 0; i < first0 * 2; ++i) {
        tree[i].val   = (i < first0) ? dict[i].freq : 0;
        tree[i].left  = -1;
        tree[i].right = -1;
    }

    // 6. Apply algorithm (merge trees)
    int treeIndex = 0, tmpTreeIndexL = first0, tmpTreeIndexR = first0;
    while (treeIndex < first0 || (tmpTreeIndexL < tmpTreeIndexR - 1)) {
        // 1. Find two lowest numbers
        int min1, min2;

        // 1.1 Find first min
        if (treeIndex == first0)                 min1 = tmpTreeIndexL++;
        else if (tmpTreeIndexL == tmpTreeIndexR) min1 = treeIndex++;
        else                                     min1 = (tree[treeIndex].val <= tree[tmpTreeIndexL].val) ? treeIndex++ : tmpTreeIndexL++;
        // 1.2 Find second min
        if (treeIndex == first0)                 min2 = tmpTreeIndexL++;
        else if (tmpTreeIndexL == tmpTreeIndexR) min2 = treeIndex++;
        else                                     min2 = (tree[treeIndex].val <= tree[tmpTreeIndexL].val) ? treeIndex++ : tmpTreeIndexL++;

#ifdef DEBUG_LOG
        printf("Merging %.3d %.3d\n", min1, min2);
#endif

        // 2. Create a new tree by merging the two values
        tree[tmpTreeIndexR++] = (HuffTreeEntry_t) {
            .val = tree[min1].val + tree[min2].val,
            .left = min1,
            .right = min2,
        };
    }

    // 7. Create encoding table from final tree
    HuffTableEntry_t table[256];
    memset(table, 0, sizeof(HuffTableEntry_t) * 256);
    
    // 8. Create queue to explore tree
    HuffQueueEntry_t queue[256];
    memset(queue, 0, sizeof(HuffQueueEntry_t) * 256);

    // 9. Start exploring from root
    int qIndex = 0;
    queue[qIndex++].index = tmpTreeIndexL;
    while (qIndex > 0) {
        // 0. vars
        qIndex--;
        const int nodeIndex              = queue[qIndex].index;
        const HuffTableEntry_t currTable = queue[qIndex].table;
        const int charValue              = dict[nodeIndex].chr;
        const int leftNodeIndex          = tree[nodeIndex].left;
        const int rightNodeIndex         = tree[nodeIndex].right;

#ifdef DEBUG_LOG
        printf("Exploring %d\n", nodeIndex);
#endif

        // 1. Check if node is a leaf
        if (leftNodeIndex == -1 && rightNodeIndex == -1) {
            // 1.1 Save code
            memcpy(&table[charValue], &currTable, sizeof(HuffTableEntry_t));
        } else {
            // 2. Push left node into queue
            if (leftNodeIndex != -1) {
                queue[qIndex].index = leftNodeIndex;
                queue[qIndex].table = currTable;
                queue[qIndex].table.code[currTable.size / 32] |= (1 << (31 - (currTable.size % 32)));
                queue[qIndex].table.size++;
                qIndex++;
            }
            // 3. Push right node into queue
            if (rightNodeIndex != -1) {
                queue[qIndex].index = rightNodeIndex;
                queue[qIndex].table = currTable;
                queue[qIndex].table.code[currTable.size / 32] |= (0 << (31 - (currTable.size % 32)));
                queue[qIndex].table.size++;
                qIndex++;
            }
        }
    }

#ifdef DEBUG_LOG
    printf("=========TABLE=========\n");
    for (int i = 0; i < 256; ++i) {
        if (table[i].size == 0) continue;
        printf("%.3d %c: ", i, (unsigned char) i);
        print_huff_table_entry(&table[i]);
        printf("\n");
    }
    printf("=======================\n");
#endif

    // 10. Estimate compressed size
    size_t compressedSize = 1 + (7) + (first0 * 2); // HEADER
    for (int i = 0; i < dataSize; ++i) {
        compressedSize += table[(int) data[i]].size;
    }
    compressedSize = (compressedSize / 8) + (compressedSize % 8 != 0);

    // 11. Write header
    int outBufferIndex = 0;
    BYTES outBuffer    = (BYTES) calloc(compressedSize, sizeof(BYTE));
    outBuffer[outBufferIndex++] = 0x80 | (first0);
    for (int i = 0; i < first0 * 2; ++i) {
        // TODO: Write tree
    }

    // 12. Encode content
    int tmpBufferIndex = 0;
    BYTE tmpBuffer = 0x00;
    for (int i = 0; i < dataSize; ++i) {
        const int charToEncode = data[i];
        const int codeSize     = table[charToEncode].size;
        for (int j = 0; j < codeSize; ++j) {
            const int byteInd  = (j / 32);
            const int bitInd   = 31 - (j % 32);
            const int bitValue = (table[charToEncode].code[byteInd] >> bitInd);
            tmpBuffer         |= bitValue << (7 - tmpBufferIndex);
            tmpBufferIndex++;
            // Write byte
            if (tmpBufferIndex == 8) {
                printf("Adding: ");
                print_byte_as_binary(tmpBuffer);
                printf("\n");
                outBuffer[outBufferIndex++] = tmpBuffer;
                tmpBufferIndex              = 0;
                tmpBuffer                   = 0x00;
            }
        }
    }
    // Write last byte if needed
    if (tmpBufferIndex > 0) outBuffer[outBufferIndex++] = tmpBuffer;

#ifdef DEBUG_LOG
    printf("Encoded content:\n");
    print_byte_as_binary(outBuffer[0]);
    printf(" -- ");
    for (int i = 1; i < compressedSize; ++i) {
        print_byte_as_binary(outBuffer[i]);
    }
    printf("\n");
#endif

    // 12. Save values and return
    *outSize = compressedSize;
    return outBuffer;
}

BYTE* decompress(CBYTES data, size_t dataSize, size_t* outSize) {
    if (dataSize == 0) return NULL;

    *outSize = 0;
    BYTES buffer = (BYTES) calloc(*outSize, sizeof(BYTE));



    return buffer;
}

// =======================================================================================================================

int cmp_dict_entries(const void* a, const void* b) {
    int freqA = ((HuffDictEntry_t*) a)->freq;
    int freqB = ((HuffDictEntry_t*) b)->freq;
    if (!freqA) freqA = INT_MAX;
    if (!freqB) freqB = INT_MAX;
    return freqA - freqB;
}

void print_huff_table_entry(const HuffTableEntry_t* entry) {
    for (int ci = 0; ci < entry->size; ++ci) {
        const int byteInd  = ci / 32;
        const int bitInd   = 31 - (ci % 32);
        const int bitValue = (entry->code[byteInd] & (1 << bitInd));
        printf("%c", (bitValue) ? '1' : '0');
    }
}

void print_byte_as_binary(CBYTE byte) {
    for (int j = 7; j >= 0; --j) {
        const int bitValue = (byte & (1 << j));
        printf("%c", (bitValue) ? '1' : '0');
    }
}
