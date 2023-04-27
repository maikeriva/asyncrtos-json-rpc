#include <aos_jrpc_server.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <test_handlers.h>
#include <test_macros.h>
#include <unity.h>
#include <unity_test_runner.h>

static void test_mock_server_read_cb(aos_future_t *future);
AOS_DECLARE(test_mock_server_call_spawnable, aos_jrpc_server_t *in_server,
            char *in_request, char *out_response, unsigned int out_err)
static void test_mock_server_call_spawnable(aos_future_t *future);

static aos_jrpc_server_t *_server = NULL;
static void (*_on_output)(const char *) = NULL;

void test_mock_server_init(void (*on_output)(const char *data)) {
  aos_jrpc_server_config_t config = {
      .parallel = true, .maxinputlen = 1000, .maxrequests = 10};
  _server = aos_jrpc_server_alloc(&config);
  TEST_ASSERT_NOT_NULL(_server);
  TEST_ASSERT_EQUAL(
      0, aos_jrpc_server_handler_set(_server, test_handler0, "testHandler0"));
  TEST_ASSERT_EQUAL(
      0, aos_jrpc_server_handler_set(_server, test_handler1, "testHandler1"));
  TEST_ASSERT_EQUAL(0,
                    aos_jrpc_server_handler_set(_server, test_handler_delayed,
                                                "testHandlerDelayed"));
  _on_output = on_output;
}

void test_mock_server_deinit() {
  vTaskDelay(pdMS_TO_TICKS(
      500)); // Wait for an eventual incomplete testHandlerDelayed.
  aos_jrpc_server_free(_server);
  _server = NULL;
}

/**
 * @brief Mock server input function
 *
 * @param data Input string
 */
void test_mock_server_read(const char *data) {
  // Spawn the server call in a new task to simulate network effects
  // This also allows to simulate delays via vTaskDelay without affecting the
  // client task
  char *data_dup = strdup(data);
  TEST_ASSERT_NOT_NULL(data_dup);
  aos_future_config_t future_conf = {.cb = test_mock_server_read_cb};
  aos_future_t *future = AOS_FUTURE_ALLOC_T(test_mock_server_call_spawnable)(
      &future_conf, _server, data_dup, NULL, 0);
  TEST_ASSERT_NOT_NULL(future);
  aos_spawn_config_t config = {.stacksize = 4096};
  aos_spawn(&config, test_mock_server_call_spawnable, future);
}

static void test_mock_server_read_cb(aos_future_t *future) {
  AOS_ARGS_T(test_mock_server_call_spawnable) *args = aos_args_get(future);
  free(args->in_request);
  unsigned int out_err = args->out_err;
  char *out_response = args->out_response;
  aos_future_free(future);

  if (out_err) {
    printf("Mock server error (%u)\n", args->out_err);
  } else if (out_response) {
    printf("Mock server output: %s\n", out_response);
    _on_output(out_response);
  } else {
    printf("Mock server output: empty (notification)\n");
  }
  free(out_response);
}

/**
 * @brief Wrapper to spawn aos_jrpc_server_call in another task, simulating a
 * separate process We need this for the delayed response
 */
AOS_DEFINE(test_mock_server_call_spawnable, aos_jrpc_server_t *, char *, char *,
           unsigned int)
static void test_mock_server_call_spawnable(aos_future_t *future) {
  AOS_ARGS_T(test_mock_server_call_spawnable) *args = aos_args_get(future);

  aos_future_t *new_future =
      AOS_AWAITABLE_ALLOC_T(aos_jrpc_server_call)(NULL, 0);
  TEST_ASSERT_NOT_NULL(new_future);
  aos_jrpc_server_call(args->in_server, args->in_request, new_future);

  TEST_ASSERT_TRUE(aos_isresolved(aos_await(new_future)));
  AOS_ARGS_T(aos_jrpc_server_call) *new_args = aos_args_get(new_future);

  args->out_err = new_args->out_err;
  args->out_response = new_args->out_data;

  aos_awaitable_free(new_future);

  aos_resolve(future);
}