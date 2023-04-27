#include <aos_jrpc_server.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <test_functions.h>
#include <unity.h>

void test_handler0(cJSON *params, aos_future_t *future) {
  AOS_ARGS_T(aos_jrpc_server_handler) *args = aos_args_get(future);
  printf("test_handler0\n");
  args->out_result = cJSON_Parse("0");
  aos_resolve(future);
}

AOS_DEFINE(test_handler1, int)
void test_handler1(cJSON *params, aos_future_t *future) {
  AOS_ARGS_T(aos_jrpc_server_handler) *args = aos_args_get(future);
  int arg1 = 0;
  if (aos_jrpc_server_param_int32_get(params, 0, "arg1", &arg1) != 0) {
    args->out_err = AOS_JRPC_SERVER_ERR_INVALIDPARAMS;
    aos_resolve(future);
    return;
  }

  printf("test_handler1: %d\n", arg1);
  args->out_result = cJSON_Parse("0");
  aos_resolve(future);
}

AOS_DEFINE(test_handler_delayed)
void test_handler_delayed(cJSON *params, aos_future_t *future) {
  AOS_ARGS_T(aos_jrpc_server_handler) *args = aos_args_get(future);

  vTaskDelay(pdMS_TO_TICKS(500));
  args->out_result = cJSON_Parse("0");
  aos_resolve(future);
}

AOS_DEFINE(test_handler_async)
static void test_handler_async_cb(aos_future_t *future);
void test_handler_async(cJSON *params, aos_future_t *future) {
  // AOS_ARGS_T(aos_jrpc_server_handler) *args = aos_args_get(future);
  aos_future_config_t config = {.cb = test_handler_async_cb, .ctx = future};
  aos_future_t *new_future = AOS_FUTURE_ALLOC_T(test_fn_resolve_0)(&config);
  if (!new_future) {
    aos_resolve(future);
    return;
  }
  test_fn_resolve_0(future);
}

static void test_handler_async_cb(aos_future_t *future) {
  aos_future_t *handler_future = aos_future_free(future);
  AOS_ARGS_T(aos_jrpc_server_handler) *handler_args =
      aos_args_get(handler_future);
  handler_args->out_result = cJSON_Parse("0");
  aos_resolve(future);
}
