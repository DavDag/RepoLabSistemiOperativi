#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "testdata.h"

typedef struct { unsigned int freq; unsigned char c; } DictEnty_t;
typedef struct { int v, f, l, r; } TreeEntry_t;
typedef struct { int size, b[4]; } TableEntry_t;
typedef struct { int index; TableEntry_t tbl; } QueueEntry_t;

static char* compress(char* buffer, size_t size, size_t* compressedSize) {
    if (size == 0) return size;

    struct timeval begin, end;
    gettimeofday(&begin, 0);

    // Intialize vars
    static DictEnty_t dict[256];
    for (int i = 0; i < 256; ++i)
        dict[i] = (DictEnty_t) { .freq = 0, .c = (unsigned char) i };

    // Count frequences
    for (int i = 0; i < size; ++i)
        dict[(unsigned int) buffer[i]].freq++;
    
    // Sort: Bubble-sort ...
    for (int i = 0; i < 256; ++i) {
        int minPos = i;
        // find min
        for (int j = i + 1; j < 256; ++j)
            if (dict[j].freq != 0 && (dict[minPos].freq == 0 || dict[j].freq < dict[minPos].freq))
                minPos = j;
        // swap
        DictEnty_t tmp = dict[i];
        dict[i] = dict[minPos];
        dict[minPos] = tmp;
    }

    // Find first 0
    int first0 = 0;
    for (int i = 0; i < 256; ++i)
        if (dict[i].freq == 0) {
            first0 = i;
            break;
        }

    // Vars
    int num_roots = first0 + 1, treeIndex = 0, tmpTreeIndexL = num_roots, tmpTreeIndexR = num_roots; 
    TreeEntry_t tree[num_roots * 2];
    for (int i = 0; i < num_roots; ++i)
        tree[i] = (TreeEntry_t) {
            .v = dict[i].freq,
            .f = -1,
            .l = -1,
            .r = -1
        };
    for (int i = 0; i < num_roots; ++i)
        tree[num_roots + i] = (TreeEntry_t) {
            .v =  0,
            .f = -1,
            .l = -1,
            .r = -1
        };

    // Merge trees
    while (treeIndex < num_roots || (tmpTreeIndexL < tmpTreeIndexR - 1)) {
        // 1. Find two lowest numbers (between two parts of the tree)
        int min1, min2;

        // Find first min
        if (treeIndex == num_roots)              // 1st part empty
            min1 = tmpTreeIndexL++;
        else if (tmpTreeIndexL == tmpTreeIndexR) // 2nd part empty
            min1 = treeIndex++;
        else
            min1 = (tree[treeIndex].v <= tree[tmpTreeIndexL].v) ? treeIndex++ : tmpTreeIndexL++;
        
        // Find second min
        if (treeIndex == num_roots)              // 1st part empty
            min2 = tmpTreeIndexL++;
        else if (tmpTreeIndexL == tmpTreeIndexR) // 2nd part empty
            min2 = treeIndex++;
        else
            min2 = (tree[treeIndex].v <= tree[tmpTreeIndexL].v) ? treeIndex++ : tmpTreeIndexL++;

        // 2. Create a node inside tmpTree that merge them
        tree[min1].f = tmpTreeIndexR;
        tree[min2].f = tmpTreeIndexR;
        tree[tmpTreeIndexR++] = (TreeEntry_t) {
            .v = tree[min1].v + tree[min2].v,
            .l = min1,
            .r = min2,
            .f = -1
        };
    }

    /*
    printf("==========TREE==========\n");
    int queue[256], qIndex = 0;
    queue[qIndex++] = tmpTreeIndexL;
    while (qIndex > 0) {
        int tI = queue[--qIndex];
        const TreeEntry_t node = tree[tI];
        printf("Im on %.2d : '%c' with val %.2d.%s\n", tI, (node.l == -1 && node.r == -1) ? dict[tI].c : ' ', node.v, (node.l == -1 && node.r == -1) ? " Im a leaf node." : "");
        if (node.l != -1) queue[qIndex++] = node.l;
        if (node.r != -1) queue[qIndex++] = node.r;
    }
    printf("========================\n");
    */

    // Encoding table
    static TableEntry_t table[256];
    static QueueEntry_t queue[256];
    memset(table, 0, sizeof(TableEntry_t) * 256);
    memset(queue, 0, sizeof(QueueEntry_t) * 256);
    int qIndex = 0;
    queue[qIndex++].index = tmpTreeIndexL;
    while (qIndex > 0) {
        const QueueEntry_t top = queue[--qIndex];
        const TreeEntry_t node = tree[top.index];

        if (node.r == -1 && node.l == -1)
            memcpy(&table[(unsigned int) dict[top.index].c], &top.tbl, sizeof(TableEntry_t));
        else {
            if (node.l != -1) {
                // Push left node into queue
                queue[qIndex].index = node.l;
                queue[qIndex].tbl = top.tbl;
                queue[qIndex].tbl.b[top.tbl.size / 32] |= (1 << (31 - (top.tbl.size % 32)));
                queue[qIndex].tbl.size++;
                qIndex++;
            }
            if (node.r != -1) {
                // Push right node into queue
                queue[qIndex].index = node.r;
                queue[qIndex].tbl = top.tbl;
                queue[qIndex].tbl.b[top.tbl.size / 32] |= (0 << (31 - (top.tbl.size % 32)));
                queue[qIndex].tbl.size++;
                qIndex++;
            }
        }
    }

    printf("=========TABLE=========\n");
    for (int i = 0; i < 256; ++i) {
        if (table[i].size == 0) continue;
        printf("%c: ", (unsigned char) i);
        for (int ci = 0; ci < table[i].size; ++ci) {
            const int byteInd = ci / 32;
            const int bitInd = 31 - (ci % 32);
            const int bitValue = (table[i].b[byteInd] & (1 << bitInd));
            printf("%c", (bitValue) ? '1' : '0');
        }
        printf("\n");
    }
    printf("=======================\n");

    // Size estimation
    size_t totalSizeInBits = 0;
    for (int i = 0; i < size; ++i)
        totalSizeInBits += table[(unsigned int) buffer[i]].size;
    size_t totalSize = (totalSizeInBits / 8) + (totalSizeInBits % 8 != 0);

    *compressedSize = totalSize;

    // Apply encoding
    // memset(bufferO, 0, totalSize);
    int index = 0;
    for (int i = 0; i < size; i += 4) {
        const char tmpBuffer[4] = { buffer[i + 0], buffer[i + 1], buffer[i + 2], buffer[i + 3] };
        buffer[i + 0] = 0; buffer[i + 1] = 0; buffer[i + 2] = 0; buffer[i + 3] = 0;
        for (int j = 0; j < 4; ++j) {
            const unsigned int tableIndex = (unsigned int) tmpBuffer[j];
            const int codeSize            = table[tableIndex].size;
            const unsigned int offset     = 7 - (index % 8);
            buffer[index / 8]            |= ((table[tableIndex].b[0] & 0xFF) >> offset);
            index                        += codeSize;

            printf("Adding %c offset %.2d: ", tmpBuffer[j], offset);
            for (int ci = 0; ci < codeSize; ++ci) {
                const int byteInd = ci / 32;
                const int bitInd = 31 - (ci % 32);
                const int bitValue = (table[tableIndex].b[byteInd] & (1 << bitInd));
                printf("%c", (bitValue) ? '1' : '0');
            }
            printf("\n");
        }
    }
    printf("buffer: ");
    for(int i = 0; i < (index / 8) + 1; ++i) {
        for (int ci = 0; ci < 8; ++ci) {
            const int bitInd = 7 - ci;
            const int bitValue = (buffer[i] & (1 << bitInd));
            printf("%c", (bitValue) ? '1' : '0');
        }
    }
    printf("\n");

    gettimeofday(&end, 0);
    long seconds = end.tv_sec - begin.tv_sec;
    long microseconds = end.tv_usec - begin.tv_usec;
    double elapsed = seconds + microseconds*1e-6;

    printf("Compression done !\n  time: %.6fs\n  size ratio: %.2f\n", elapsed, (float) totalSize / (float) size * 100.0f);
    return size;
}

static size_t decompress(char* buffer, size_t size) {
    struct timeval begin, end;
    gettimeofday(&begin, 0);

    // TODO:

    gettimeofday(&end, 0);
    long seconds = end.tv_sec - begin.tv_sec;
    long microseconds = end.tv_usec - begin.tv_usec;
    double elapsed = seconds + microseconds*1e-6;

    printf("Decompression done !\n  time: %.6fs\n  size ratio: %.2f\n", elapsed, (float) size / (float) size * 100.0f);
    return size;
}

int main(int argc, char** argv) {
    int testsSize = 2;
    const char* const testsData[] = {
        //TEXT_EMPTY,
        TEXT_SHORT,
        TEXT_BEST_CASE,
        //TEXT_1,
        //TEXT_2,
        //TEXT_3,
    };

    for (int i = 0; i < testsSize; ++i) {
        printf("test #%02d\n", i + 1);
        memset(buffer, 0, BUFFER_SIZE);

        size_t testsDataSize = strlen(testsData[i]);
        memcpy(buffer, testsData[i], testsDataSize);

        size_t compressedSize;
        size_t decompressedSize;
        const char* compressedData = compress(buffer, testsDataSize, &compressedSize);
        const char* decompressedData = decompress(compressedData, compressedSize, &decompressedSize);

        int result = (decompressedSize == testsDataSize) && ((memcmp(testsData[i], buffer, testsDataSize)) == 0);
        result++;
        printf("\n");
    }

    return EXIT_SUCCESS;
}