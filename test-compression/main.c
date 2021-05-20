#define _POSIX_C_SOURCE 200809L

#include "testdata.h"
#include "huffman_encoding.c"

void test(const char* name, const char* data, size_t size0) {
    printf("%-24s: ", name);

    HuffCodingResult_t res1 = compress_data(data, size0);
    HuffCodingResult_t res2 = decompress_data(res1.data, res1.size);

    printf("%9.7f ns | %9.7f ns : ", res1.time, res2.time);

    if (res2.size == size0) {
        if (memcmp(res2.data, data, res2.size) == 0) {
            printf("Success\n");
        } else {
            printf("Failed, different content\n");
        }
    } else {
        printf("Failed, different sizes %lu %lu\n", res2.size, size0);
    }

    free(res1.data);
    free(res2.data);
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

int main(int argc, char** argv) {
    test("text_short1", TEXT_SHORT1, strlen(TEXT_SHORT1));
    test("text_short2", TEXT_SHORT2, strlen(TEXT_SHORT2));
    test("text_short3", TEXT_SHORT3, strlen(TEXT_SHORT3));
    test("text empty", TEXT_EMPTY, strlen(TEXT_EMPTY));
    test("text best case", TEXT_BEST_CASE, strlen(TEXT_BEST_CASE));
    test("text 1", TEXT_1, strlen(TEXT_1));
    test("text 2", TEXT_2, strlen(TEXT_2));
    test("text 3", TEXT_3, strlen(TEXT_3));
    test("text 4", TEXT_4, strlen(TEXT_4));
    test("text 5", TEXT_5, strlen(TEXT_5));
    test("text 6", TEXT_6, strlen(TEXT_6));
    for (int i = 0; i < 32; ++i) {
        char buffer[50];
        snprintf(buffer, 50, "testdir/test_%d.txt", i);
        test_from_file(buffer);
    }
    return 1;
}