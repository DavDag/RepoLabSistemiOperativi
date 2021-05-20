#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "testdata.h"

// #define DEBUG_LOG
// #define DEBUG_LOG_EXT
#define DEBUG_LOG_CONTENT
#include "huffman.h"

/**
 * 
 * HUFFMAN CODING
 * 
 * sources:
 *    https://en.wikipedia.org/wiki/Huffman_coding
 * 
 */

double calc_time_delta(struct timeval begin, struct timeval end) {
    long seconds = end.tv_sec - begin.tv_sec;
    long microseconds = end.tv_usec - begin.tv_usec;
    double elapsed = seconds + microseconds*1e-6;
    return elapsed;
}

const char* size_t_to_string(size_t size) {
    static const size_t B = 1, KB = 1024UL, MB = 1024UL * 1024UL, GB = 1024UL * 1024UL * 1024UL;
    static char buffer[16];
    if (size > GB)      snprintf(buffer, 16, "%.2f GB", ((float) size / (float) GB));
    else if (size > MB) snprintf(buffer, 16, "%.2f MB", ((float) size / (float) MB));
    else if (size > KB) snprintf(buffer, 16, "%.2f KB", ((float) size / (float) KB));
    else                snprintf(buffer, 16, "%.2f B",  ((float) size / (float)  B));
    return buffer;
}

void test(const char* testname, const char* data, size_t size0) {
    // vars
    struct timeval begin, end;
    size_t size1 = 0, size2 = 0;
    // Compression
    gettimeofday(&begin, 0);
    char* compressedData = compress(data, size0, &size1);
    gettimeofday(&end, 0);
    double compressionTime = calc_time_delta(begin, end);
    int isCompressed = (compressedData && (compressedData[0] & 0x80));
    // Decompression
    gettimeofday(&begin, 0);
    char* decompressedData = decompress(compressedData, size1, &size2);
    gettimeofday(&end, 0);
    double decompressionTime = calc_time_delta(begin, end);
    // Output result
    printf("\n================================\n");
    printf("| %28s |\n", testname);
    printf("--------------------------------\n");
    printf("| Compressing...     ");
    printf("%2.6fs |\n", compressionTime);
    printf("| Decompressing...   ");
    printf("%2.6fs |\n", decompressionTime);
    printf("| Original size:  %12s |\n", size_t_to_string(size0));
    if (isCompressed) printf("| Size reduction:     %7.4f%% |\n", ((size0 - size1)/(float)size0) * 100.0f);
    else              printf("| Not compressed.              |\n");
    printf("--------------------------------\n");
    if (size0 == size2) {
        if (memcmp(decompressedData, data, size0) == 0) printf("| Success.                     |\n");
        else                                            printf("| Failure, different content.  |\n");
    }
    else                                                printf("| Failure, different sizes.    |\n");
    free(compressedData);
    free(decompressedData);
    printf("================================\n\n");
}

void test_from_file(const char* filename) {
    FILE *f = fopen(filename, "rb");
    fseek(f, 0, SEEK_END);
    size_t fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *string = calloc(fsize, sizeof(char));
    size_t read = fread(string, 1, fsize, f);
    fclose(f);
    test(filename, string, read);
    free(string);
}

int main() {
    test("text short 1", TEXT_SHORT1, strlen(TEXT_SHORT1));
    // test("text short 2", TEXT_SHORT2, strlen(TEXT_SHORT2));
    // test("text short 3", TEXT_SHORT3, strlen(TEXT_SHORT3));
    // test("text empty", TEXT_EMPTY, strlen(TEXT_EMPTY));
    // test("text best case", TEXT_BEST_CASE, strlen(TEXT_BEST_CASE));
    // test("text 1", TEXT_1, strlen(TEXT_1));
    // test("text 2", TEXT_2, strlen(TEXT_2));
    // test("text 3", TEXT_3, strlen(TEXT_3));
    // test("text 4", TEXT_4, strlen(TEXT_4));
    // test("text 5", TEXT_5, strlen(TEXT_5));
    // test("text 6", TEXT_6, strlen(TEXT_6));
    // test_from_file("test.txt");
    // test_from_file("test2.txt");
    // test_from_file("test4.txt");
    // 
    // for (int i = 0; i < 32; ++i) {
    //     char buffer[50];
    //     snprintf(buffer, 50, "testdir/test_%d.txt", i);
    //     test_from_file(buffer);
    // }
    return 0;
}