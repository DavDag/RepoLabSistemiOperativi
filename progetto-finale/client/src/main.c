#include <stdio.h>
#include <stdlib.h>

#include <serverapi.h>

int main(int argc, char **argv) {
    printf("Hello world from client !\n");
    hello_from_lib();
    return EXIT_SUCCESS;
}