// https://github.com/ThrowTheSwitch/Unity/blob/master/docs/UnityAssertionsReference.md
#include <aos.h>
#include <aos_jrpc_server.h>
#include <test_functions.h>
#include <unity.h>

void test_handler0(cJSON *params, aos_future_t *future);

AOS_DECLARE(test_handler1, int arg1)
void test_handler1(cJSON *params, aos_future_t *future);

AOS_DECLARE(test_handler_delayed)
void test_handler_delayed(cJSON *params, aos_future_t *future);

AOS_DECLARE(test_handler_async)
void test_handler_async(cJSON *params, aos_future_t *future);