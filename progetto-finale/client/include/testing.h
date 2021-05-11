#pragma once

#include <common.h>

typedef enum {
    TEST_NONE = 0,
    TEST_ARGUMENT_PARSING = 1,
} TestMode_t;

Result_t handle_test_argument_option(TestMode_t *, const char*);
