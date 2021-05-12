#include <stdio.h>
#include <stdlib.h>

#define TIMESTAMP
#define LOG_WITHOUT_COLORS
#include <logger.h>

int main(int argc, char **argv) {
    LOG_WARN("Hello world from server !");
    return EXIT_SUCCESS;
}