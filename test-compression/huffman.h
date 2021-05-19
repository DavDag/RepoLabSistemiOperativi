#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>

#define DEBUG_LOG

#define CHECK_BIT(var,pos) (((var) & (1<<(pos))) ? 1 : 0)

// =======================================================================================================================

typedef char BYTE;
typedef const char CBYTE;
typedef BYTE* BYTE_PTR;
typedef CBYTE* CBYTE_PTR;

static_assert(sizeof(BYTE) == 1, "Must be 1 byte");
static_assert(sizeof(CBYTE) == 1, "Must be 1 byte");

typedef struct { int freq; BYTE chr; } HuffDictEntry_t;
typedef struct { int val, left, right; } HuffTreeEntry_t;
typedef struct { int size; unsigned int code[4]; } HuffTableEntry_t;
typedef struct { int index; HuffTableEntry_t table; } HuffQueueEntry_t;

static_assert(sizeof(int) == 4, "Must be 4 byte_PTR");

// =======================================================================================================================

BYTE_PTR compress(CBYTE_PTR data, size_t dataSize, size_t* outSize);
BYTE_PTR decompress(CBYTE_PTR data, size_t dataSize, size_t* outSize);
int cmp_dict_entries(const void* a, const void* b);
void write_bit_into_buffer_using_tmp(BYTE_PTR buffer, int* bufferIndex, BYTE_PTR tmpBuffer, int* tmpBufferIndex, int bitValue);
void write_byte_into_buffer_using_tmp(BYTE_PTR buffer, int* bufferIndex, BYTE_PTR tmpBuffer, int* tmpBufferIndex, CBYTE byteValue);
int read_bit_from_buffer_using_tmp(CBYTE_PTR buffer, int* bufferIndex, BYTE_PTR tmpBuffer, int* tmpBufferIndex);
BYTE read_byte_from_buffer_using_tmp(CBYTE_PTR buffer, int* bufferIndex, BYTE_PTR tmpBuffer, int* tmpBufferIndex);
void print_huff_table_entry(const HuffTableEntry_t* entry);
void print_byte_as_binary(CBYTE byte);

// =======================================================================================================================

BYTE_PTR compress(CBYTE_PTR data, size_t dataSize, size_t* outSize) {
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
    int treeRoot = -1;
    {
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

        // Save root
        treeRoot = tmpTreeIndexL;
    }

    // 7. Create encoding table from final tree
    HuffTableEntry_t table[256];
    memset(table, 0, sizeof(HuffTableEntry_t) * 256);
    
    { // 8. Create queue to explore tree
        HuffQueueEntry_t queue[256];
        memset(queue, 0, sizeof(HuffQueueEntry_t) * 256);

        // 9. Start exploring from root
        int qIndex = 0;
        queue[qIndex++].index = treeRoot;
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
    const int treeSize = (10 * first0 - 1);
    size_t compressedSize = 48 + treeSize; // HEADER
    for (int i = 0; i < dataSize; ++i) {
        compressedSize += table[(int) data[i]].size;
    }
    const int padding = compressedSize % 8;
    compressedSize    = (compressedSize / 8) + (padding != 0);

    // 11. Create output buffer
    int outBufferIndex = 0;
    BYTE_PTR outBuffer = (BYTE_PTR) calloc(compressedSize, sizeof(BYTE));
    int tmpBufferIndex = 0;
    BYTE tmpBuffer     = 0x00;

    // 12. Write header + tree
    outBuffer[outBufferIndex++] = 0x80 | (padding); // Header: compressed flag, padding
    outBuffer[outBufferIndex++] = (first0);         // Header: charCount
    outBuffer[outBufferIndex++] = dataSize >>  0;   // Header: decompressed size p.1
    outBuffer[outBufferIndex++] = dataSize >>  8;   // Header: decompressed size p.2
    outBuffer[outBufferIndex++] = dataSize >> 16;   // Header: decompressed size p.3
    outBuffer[outBufferIndex++] = dataSize >> 24;   // Header: decompressed size p.4

#ifdef DEBUG_LOG
    printf("Header content:\n  padding: %d\n  charCount: %d\n  treeSize: %d\n  origSize: %d\n", padding, first0, treeSize, (int) dataSize);
#endif

    { // Header: Tree
        int queue[256], qIndex = 0;
        queue[qIndex++] = treeRoot;
        while (qIndex > 0) {
            // vars
            qIndex--;
            const int nodeIndex      = queue[qIndex];
            const int leftNodeIndex  = tree[nodeIndex].left;
            const int rightNodeIndex = tree[nodeIndex].right;
            CBYTE charValue          = dict[nodeIndex].chr;

            // 1. Check for leaf node
            if (leftNodeIndex == -1 && rightNodeIndex == -1) {
                // 1.1 Write flag for node leaf
                write_bit_into_buffer_using_tmp(outBuffer, &outBufferIndex, &tmpBuffer, &tmpBufferIndex, 1);
                // 1.2 Write character
                write_byte_into_buffer_using_tmp(outBuffer, &outBufferIndex, &tmpBuffer, &tmpBufferIndex, charValue);
            } else {
                // 2.1 Write flag for intermediate node
                write_bit_into_buffer_using_tmp(outBuffer, &outBufferIndex, &tmpBuffer, &tmpBufferIndex, 0);
                // 2.2 Push right node into queue
                if (rightNodeIndex != -1) {
                    queue[qIndex] = rightNodeIndex;
                    qIndex++;
                }
                // 2.3 Push left node into queue
                if (leftNodeIndex != -1) {
                    queue[qIndex] = leftNodeIndex;
                    qIndex++;
                }
            }
        }
    }

#ifdef DEBUG_LOG
    printf("Encoding content...\n");
#endif

    { // 13. Encode content
        for (int i = 0; i < dataSize; ++i) {
            const int charToEncode = data[i];
            const int codeSize     = table[charToEncode].size;
            for (int j = 0; j < codeSize; ++j) {
                const int byteInd  = (j / 32);
                const int bitInd   = 31 - (j % 32);
                const int bitValue = CHECK_BIT(table[charToEncode].code[byteInd], bitInd);
                write_bit_into_buffer_using_tmp(outBuffer, &outBufferIndex, &tmpBuffer, &tmpBufferIndex, bitValue);
            }
        }
        // Write last byte if needed
        if (tmpBufferIndex > 0) outBuffer[outBufferIndex++] = tmpBuffer;
    }

#ifdef DEBUG_LOG
    printf("Encoded content:\n");
    print_byte_as_binary(outBuffer[0]);
    printf(" | ");
    print_byte_as_binary(outBuffer[1]);
    printf(" | ");
    print_byte_as_binary(outBuffer[2]);
    print_byte_as_binary(outBuffer[3]);
    print_byte_as_binary(outBuffer[4]);
    print_byte_as_binary(outBuffer[5]);
    printf(" | ");
    for (int i = 2; i < compressedSize; ++i) {
        print_byte_as_binary(outBuffer[i]);
    }
    printf("\n");
#endif

    // 12. Save values and return
    *outSize = compressedSize;
    return outBuffer;
}

BYTE_PTR decompress(CBYTE_PTR data, size_t dataSize, size_t* outSize) {
    if (dataSize == 0) return NULL;

    // 1. Read 1st byte of header
    int dataIndex = 1, tmpByteIndex = 0;
    BYTE tmpByte  = data[0];
    CBYTE header1 = read_byte_from_buffer_using_tmp(data, &dataIndex, &tmpByte, &tmpByteIndex);
    { // 2. Check if data is compressed
        if (!(header1 & (0x80))) {
            // Data uncompressed
#ifdef DEBUG_LOG
            printf("Found uncompressed data\n");
#endif

            BYTE_PTR buffer = (BYTE_PTR) calloc(dataSize - 1, sizeof(BYTE));
            memcpy(buffer, data, dataSize - 1);
            *outSize = dataSize - 1;
            return buffer;
        }
    }

    // 3. Finish reading header
    const int padding = (header1 & 0x7F);
    CBYTE header2 = read_byte_from_buffer_using_tmp(data, &dataIndex, &tmpByte, &tmpByteIndex);
    int originalSize = 
        read_byte_from_buffer_using_tmp(data, &dataIndex, &tmpByte, &tmpByteIndex) <<  0 |
        read_byte_from_buffer_using_tmp(data, &dataIndex, &tmpByte, &tmpByteIndex) <<  8 |
        read_byte_from_buffer_using_tmp(data, &dataIndex, &tmpByte, &tmpByteIndex) << 16 |
        read_byte_from_buffer_using_tmp(data, &dataIndex, &tmpByte, &tmpByteIndex) << 24 ;
    const int charCount = header2;
    const int bitCountForTree = 10 * (charCount) - 1; // Reverse from compress

#ifdef DEBUG_LOG
    printf("Header content:\n  padding: %d\n  charCount: %d\n  treeSize: %d\n  origSize: %d\n", padding, charCount, bitCountForTree, originalSize);
#endif

    // 4. Explore data to fill tree
    HuffTreeEntry_t tree[charCount * 2];
    for (int i = 0; i < charCount * 2 - 1; ++i) {
        tree[i].val   = -1;
        tree[i].left  = -1;
        tree[i].right = -1;
    }
    {
        int fatherQueue[256], qIndex = 0;
        fatherQueue[qIndex] = dataIndex; // tree root
        for (int treeIndex = 0; treeIndex < (charCount * 2 - 1) && qIndex >= 0; ++treeIndex) {
            // 0. vars
            const int bitValue = read_bit_from_buffer_using_tmp(data, &dataIndex, &tmpByte, &tmpByteIndex);

            // 1. Check if a leaf node appears
            if (bitValue) {
                // 1.1 Fill the tree data
                BYTE byte = read_byte_from_buffer_using_tmp(data, &dataIndex, &tmpByte, &tmpByteIndex);
                tree[treeIndex].val   = byte;
                tree[treeIndex].left  = -1;
                tree[treeIndex].right = -1;

                // 1.2 Update father's data
                int updated = 0, tmpInd = treeIndex;
                do {
                    const int currFather = fatherQueue[qIndex];
                    if (tree[currFather].left == -1) {
                        tree[currFather].left = tmpInd;
                        updated = 1;
                    } else if (tree[currFather].right == -1) {
                        tree[currFather].right = tmpInd;
                        tmpInd = currFather;
                        qIndex--;
                    }
                } while(!updated && qIndex > 0);
            } else {
                // 2. Push node into fatherQueue
                fatherQueue[++qIndex] = treeIndex;
            }
        }
    }

#ifdef DEBUG_LOG
    {
        printf("Reconstructed tree:\n");
        int qIndex = 0;
        int queue[charCount * 2];
        queue[qIndex++] = 0;
        while (qIndex > 0) {
            qIndex--;
            const int treeIndex = queue[qIndex];
            printf("Exploring %d\n", treeIndex);
            if (tree[treeIndex].val == -1) {
                queue[qIndex++] = tree[treeIndex].left;
                queue[qIndex++] = tree[treeIndex].right;
            } else {
                printf("#%.2d ['%c']\n", treeIndex, tree[treeIndex].val);
            }
        }
        printf("\n==================\n");
        for (int i = 0; i < charCount * 2; ++i) {
            printf("#%.2d ['%c'] > L: %.2d , R: %.2d\n", i, (tree[i].val == -1) ? '?' : tree[i].val, tree[i].left, tree[i].right);
        }
        printf("\n");
    }
#endif

    // 5. Decode
    int bufferIndex = 0;
    BYTE_PTR buffer = (BYTE_PTR) calloc(originalSize, sizeof(BYTE));
    {
        int treeIndex = 0;
        while (dataIndex < dataSize) {
            // Read byte
            CBYTE byte = read_byte_from_buffer_using_tmp(data, &dataIndex, &tmpByte, &tmpByteIndex);
            for (int i = 7; i >= 0; --i) {
                const int bitValue = CHECK_BIT(byte, i);
                treeIndex = (bitValue) ? tree[treeIndex].left : tree[treeIndex].right;
                if (tree[treeIndex].val != -1) {
                    buffer[bufferIndex++] = tree[treeIndex].val;
                    treeIndex = 0;
                }
            }
        }
        if (padding) {
            // Padding handling
            CBYTE byte = read_byte_from_buffer_using_tmp(data, &dataIndex, &tmpByte, &tmpByteIndex);
            for (int i = 7; i >= (7 - padding); --i) {
                const int bitValue = CHECK_BIT(byte, i);
                treeIndex = (bitValue) ? tree[treeIndex].left : tree[treeIndex].right;
                if (tree[treeIndex].val != -1) {
                    buffer[bufferIndex++] = tree[treeIndex].val;
                    treeIndex = 0;
                }
            }
        }
    }

#ifdef DEBUG_LOG
    printf("Decoded text:\n");
    for (int i = 0; i < bufferIndex; ++i) {
        printf("| %c", buffer[i]);
    }
    printf("\n");
#endif

    // 12. Save values and return
    *outSize = originalSize;
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

void write_bit_into_buffer_using_tmp(BYTE_PTR buffer, int* bufferIndex, BYTE_PTR tmpBuffer, int* tmpBufferIndex, int bitValue) {
    // Write bit
    (*tmpBuffer) |= bitValue << (7 - (*tmpBufferIndex));
    (*tmpBufferIndex)++;

    // Write byte
    if ((*tmpBufferIndex) == 8) {
        buffer[(*bufferIndex)++] = *tmpBuffer;
        *tmpBufferIndex          = 0;
        *tmpBuffer               = 0x00;
    }
}

int read_bit_from_buffer_using_tmp(CBYTE_PTR buffer, int* bufferIndex, BYTE_PTR tmpBuffer, int* tmpBufferIndex) {
    // Read bit
    const int bitValue = CHECK_BIT((*tmpBuffer), (7 - (*tmpBufferIndex)));
    (*tmpBufferIndex)++;

    // Read byte
    if ((*tmpBufferIndex) == 8) {
        *tmpBufferIndex = 0;
        *tmpBuffer      = buffer[(*bufferIndex)++];
    }
    return bitValue;
}

void write_byte_into_buffer_using_tmp(BYTE_PTR buffer, int* bufferIndex, BYTE_PTR tmpBuffer, int* tmpBufferIndex, CBYTE byteValue) {
    for (int i = 7; i >= 0; --i) {
        const int bitValue = CHECK_BIT(byteValue, i);
        write_bit_into_buffer_using_tmp(buffer, bufferIndex, tmpBuffer, tmpBufferIndex, bitValue);
    }
}

BYTE read_byte_from_buffer_using_tmp(CBYTE_PTR buffer, int* bufferIndex, BYTE_PTR tmpBuffer, int* tmpBufferIndex) {
    BYTE byte = 0x00;
    for (int i = 7; i >= 0; --i) {
        const int bitValue = read_bit_from_buffer_using_tmp(buffer, bufferIndex, tmpBuffer, tmpBufferIndex);
        byte              |= (bitValue << i);
    }
    return byte;
}

void print_huff_table_entry(const HuffTableEntry_t* entry) {
    for (int ci = 0; ci < entry->size; ++ci) {
        const int byteInd  = ci / 32;
        const int bitInd   = 31 - (ci % 32);
        const int bitValue = CHECK_BIT(entry->code[byteInd], bitInd);
        printf("%c", (bitValue) ? '1' : '0');
    }
}

void print_byte_as_binary(CBYTE byte) {
    for (int j = 7; j >= 0; --j) {
        const int bitValue = CHECK_BIT(byte, j);
        printf("%c", (bitValue) ? '1' : '0');
    }
}
