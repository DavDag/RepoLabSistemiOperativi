#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>

// REFERENCES
// https://en.wikipedia.org/wiki/Universally_unique_identifier#Version_4_(random)
// https://digitalbunker.dev/2020/09/30/understanding-how-uuids-are-generated/
// https://www.itu.int/en/ITU-T/asn1/Pages/UUID/uuids.aspx
// https://www.cryptosys.net/pki/uuid-rfc4122.html

// TESTED ONLINE ON:
// https://www.freecodeformat.com/validate-uuid-guid.php

typedef struct { int data[4]; } UUID_t;
_Static_assert(sizeof(UUID_t) == 16, "UUID must be 16 bytes long");

static inline UUID_t newUUID() {
    return (UUID_t) { .data = { rand(), ((rand() & 0xFFFF0FFF) | 0x00004000), ((rand() & 0x3FFFFFFF) | 0x80000000), rand() } };
}

static inline const char* toString(UUID_t uuid) {
    static char buffer[50]; // "0xAAAAAAAA-BBBB-BBBB-CCCC-CCCCDDDDDDDD"
    // snprintf(buffer, 50, "0x%08X 0x%08X 0x%08X 0x%08X", uuid.data[0], uuid.data[1], uuid.data[2], uuid.data[3]);
    snprintf(buffer, 50, "%08x-%04x-%04x-%04x-%04x%08x",
        (uuid.data[0] & 0xFFFFFFFF) >>  0,  // 8
        (uuid.data[1] & 0xFFFF0000) >> 16,  // 4
        (uuid.data[1] & 0x0000FFFF) >>  0,  // 4
        (uuid.data[2] & 0xFFFF0000) >> 16,  // 4
        (uuid.data[2] & 0x0000FFFF) >>  0,  // 4
        (uuid.data[3] & 0xFFFFFFFF) >>  0); // 8
    return buffer;
}

void old_generation() {
    int a[] = { rand(), rand(), rand(), rand() };
    // printf("UUID (v1): 0x%08x%08x%08x%08x\n", a[0], a[1], a[2], a[3]);

    char b[16];
    memcpy(b, a, 16);

    b[6] &= 0x0F; // Tengo solo la parte a dx
    b[6] |= 0x40; // Setto un bit a sx
    b[8] &= 0x3F; // Tengo solo la parte a dx piu 2 bit a sx
    b[8] |= 0x80; // Setto un bit a sx
    
    printf("%08x-%04x-%04x-%04x-%04x%08x\n",
        ((int*) b)[0],                              //  8 digits
        (((int*) b)[1] & 0xFFFF0000) >> 16,         //  4 digits
        ((int*) b)[1] & 0x0000FFFF,                 //  4 digits
        (((int*) b)[2] & 0xFFFF0000) >> 16,         //  4 digits
        ((int*) b)[2] & 0x0000FFFF, ((int*) b)[3]); // 12 digits
}

int main(int argc, char** argv) {
    srand(time(NULL));

    old_generation();
    old_generation();
    old_generation();

    UUID_t u = newUUID();
    printf("%s\n", toString(u));
    u = newUUID();
    printf("%s\n", toString(u));
    u = newUUID();
    printf("%s\n", toString(u));

    return EXIT_SUCCESS;
}