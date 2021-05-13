#include <stdio.h>
#include <stdlib.h>

#define LOG_TIMESTAMP
#define LOG_WITHOUT_COLORS
#include <logger.h>
#include <common.h>

int main(int argc, char** argv) {
    LOG_WARN("Hello world from server !");
    return EXIT_SUCCESS;
}