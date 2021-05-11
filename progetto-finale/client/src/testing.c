#include "testing.h"

Result_t handle_test_argument_option(TestMode_t *mode, const char* value) {
    *mode = TEST_NONE;
    return RES_OK;
}