#include <aos_jrpc_peer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <test_macros.h>
#include <test_mock_server.h>
#include <unity.h>
#include <unity_test_runner.h>

static aos_jrpc_client_t *_client = NULL;
static bool _simulateOutputFail = false;

unsigned int test_client_on_output(const char *data) {
  printf("Client output: %s\n", data);
  return _simulateOutputFail
             ? 1
             : 0; // This function is for the client, if it returns a non-zero
                  // value it means that the output could not succeed. In that
                  // case, the timeout count is not even started and the send
                  // future is resoled immediately.
}

unsigned int test_client_on_output_tomockserver(const char *data) {
  printf("Client output: %s\n", data);
  if (_simulateOutputFail)
    return 1;
  test_mock_server_read(data);
  return 0;
}

void test_client_read(const char *data) {
  if (!_client) {
    return;
  }
  printf("Client input: %s\n", data);
  aos_jrpc_client_read(_client, data);
}

TEST_CASE("Send request without params (json)", "[client]") {
  TEST_HEAP_START

  // Init mock server
  test_mock_server_init(test_client_read);

  // Init client
  // TODO: Test cases with too many requests for both client and server
  aos_jrpc_client_config_t client_config = {
      .maxinputlen = 1000,
      .maxrequests = 10,
      .on_output = test_client_on_output_tomockserver};
  _client = aos_jrpc_client_alloc(&client_config);
  TEST_ASSERT_NOT_NULL(_client);

  // Send request
  aos_future_t *future =
      AOS_AWAITABLE_ALLOC_T(aos_jrpc_client_request_send_json)(NULL, 0);
  TEST_ASSERT_NOT_NULL(future);
  aos_jrpc_client_request_send_json(_client, 100, "testHandler0", NULL, future);

  // Await response, get results
  TEST_ASSERT_TRUE(aos_isresolved(aos_await(future)));
  AOS_ARGS_T(aos_jrpc_client_request_send_json) *args = aos_args_get(future);
  printf("Response (err:%d hasResult:%u)\n", args->out_err,
         args->out_result != NULL);
  TEST_ASSERT_EQUAL(AOS_JRPC_CLIENT_ERR_OK, args->out_err);
  TEST_ASSERT_NOT_NULL(args->out_result);
  cJSON_Delete(args->out_result);
  aos_awaitable_free(future);

  // Give the timeout time to expire, as it will clean its stuff and allow us to
  // deinit the client
  vTaskDelay(pdMS_TO_TICKS(150));

  // Deinit client
  TEST_ASSERT_EQUAL(0, aos_jrpc_client_free(_client));
  _client = NULL;

  // Deinit mock server
  test_mock_server_deinit();

  TEST_HEAP_STOP
}

TEST_CASE("Send request with params (json)", "[client]") {
  TEST_HEAP_START

  // Init mock server
  test_mock_server_init(test_client_read);

  // Init client
  // TODO: Test cases with too many requests for both client and server
  aos_jrpc_client_config_t client_config = {
      .maxinputlen = 1000,
      .maxrequests = 10,
      .on_output = test_client_on_output_tomockserver};
  _client = aos_jrpc_client_alloc(&client_config);
  TEST_ASSERT_NOT_NULL(_client);

  // Create parameters
  cJSON *params = cJSON_Parse("[1]");
  TEST_ASSERT_NOT_NULL(params);

  // Send request, cleanup temporary values
  aos_future_t *future =
      AOS_AWAITABLE_ALLOC_T(aos_jrpc_client_request_send_json)(NULL, 0);
  TEST_ASSERT_NOT_NULL(future);
  aos_jrpc_client_request_send_json(_client, 100, "testHandler1", params,
                                    future);
  cJSON_Delete(params);

  // Await response, get results
  TEST_ASSERT_TRUE(aos_isresolved(aos_await(future)));
  AOS_ARGS_T(aos_jrpc_client_request_send_json) *args = aos_args_get(future);
  printf("Response (err:%d hasResult:%u)\n", args->out_err,
         args->out_result != NULL);
  TEST_ASSERT_EQUAL(AOS_JRPC_CLIENT_ERR_OK, args->out_err);
  TEST_ASSERT_NOT_NULL(args->out_result);
  cJSON_Delete(args->out_result);
  aos_awaitable_free(future);

  // Give the timeout time to expire, as it will clean its stuff and allow us to
  // deinit the client
  vTaskDelay(pdMS_TO_TICKS(150));

  // Deinit client
  TEST_ASSERT_EQUAL(0, aos_jrpc_client_free(_client));
  _client = NULL;

  // Deinit mock server
  test_mock_server_deinit();

  TEST_HEAP_STOP
}

TEST_CASE("Send request with no params (json,timeout)", "[client]") {
  TEST_HEAP_START

  // Init mock server
  test_mock_server_init(test_client_read);

  // Init client
  // TODO: Test cases with too many requests for both client and server
  aos_jrpc_client_config_t client_config = {
      .maxinputlen = 1000,
      .maxrequests = 10,
      .on_output = test_client_on_output_tomockserver};
  _client = aos_jrpc_client_alloc(&client_config);
  TEST_ASSERT_NOT_NULL(_client);

  // Send request
  aos_future_t *future =
      AOS_AWAITABLE_ALLOC_T(aos_jrpc_client_request_send_json)(NULL, 0);
  TEST_ASSERT_NOT_NULL(future);
  aos_jrpc_client_request_send_json(_client, 100, "testHandlerDelayed", NULL,
                                    future);

  // Await response, get results
  TEST_ASSERT_TRUE(aos_isresolved(aos_await(future)));
  AOS_ARGS_T(aos_jrpc_client_request_send_json) *args = aos_args_get(future);
  printf("Response (err:%d hasResult:%u)\n", args->out_err,
         args->out_result != NULL);
  TEST_ASSERT_EQUAL(AOS_JRPC_CLIENT_ERR_TIMEOUT, args->out_err);
  TEST_ASSERT_NULL(args->out_result);
  cJSON_Delete(args->out_result);
  aos_awaitable_free(future);

  // Deinit client
  TEST_ASSERT_EQUAL(0, aos_jrpc_client_free(_client));
  _client = NULL;

  // Wait for server to complete response before deinitializing
  vTaskDelay(pdMS_TO_TICKS(500));

  // Deinit mock server
  test_mock_server_deinit();

  TEST_HEAP_STOP
}

TEST_CASE("Send request without params (text)", "[client]") {
  TEST_HEAP_START

  // Init mock server
  test_mock_server_init(test_client_read);

  // Init client
  // TODO: Test cases with too many requests for both client and server
  aos_jrpc_client_config_t client_config = {
      .maxinputlen = 1000,
      .maxrequests = 10,
      .on_output = test_client_on_output_tomockserver};
  _client = aos_jrpc_client_alloc(&client_config);
  TEST_ASSERT_NOT_NULL(_client);

  // Send request
  aos_future_t *future =
      AOS_AWAITABLE_ALLOC_T(aos_jrpc_client_request_send)(NULL, 0);
  TEST_ASSERT_NOT_NULL(future);
  aos_jrpc_client_request_send(_client, 100, "testHandler0", NULL, future);

  // Await response, get results
  TEST_ASSERT_TRUE(aos_isresolved(aos_await(future)));
  AOS_ARGS_T(aos_jrpc_client_request_send) *args = aos_args_get(future);
  printf("Response (err:%d): %s\n", args->out_err,
         args->out_result ? args->out_result : "none");
  TEST_ASSERT_EQUAL(AOS_JRPC_CLIENT_ERR_OK, args->out_err);
  TEST_ASSERT_NOT_NULL(args->out_result);
  free(args->out_result);
  aos_awaitable_free(future);

  // Give the timeout time to expire, as it will clean its stuff and allow us to
  // deinit the client
  vTaskDelay(pdMS_TO_TICKS(150));

  // Deinit client
  TEST_ASSERT_EQUAL(0, aos_jrpc_client_free(_client));
  _client = NULL;

  // Deinit mock server
  test_mock_server_deinit();

  TEST_HEAP_STOP
}

TEST_CASE("Send request with params (text)", "[client]") {
  TEST_HEAP_START

  // Init mock server
  test_mock_server_init(test_client_read);

  // Init client
  // TODO: Test cases with too many requests for both client and server
  aos_jrpc_client_config_t client_config = {
      .maxinputlen = 1000,
      .maxrequests = 10,
      .on_output = test_client_on_output_tomockserver};
  _client = aos_jrpc_client_alloc(&client_config);
  TEST_ASSERT_NOT_NULL(_client);

  // Send request
  aos_future_t *future =
      AOS_AWAITABLE_ALLOC_T(aos_jrpc_client_request_send)(NULL, 0);
  TEST_ASSERT_NOT_NULL(future);
  aos_jrpc_client_request_send(_client, 100, "testHandler1", "[1]", future);

  // Await response, get results
  TEST_ASSERT_TRUE(aos_isresolved(aos_await(future)));
  AOS_ARGS_T(aos_jrpc_client_request_send) *args = aos_args_get(future);
  printf("Response (err:%d): %s\n", args->out_err,
         args->out_result ? args->out_result : "none");
  TEST_ASSERT_EQUAL(AOS_JRPC_CLIENT_ERR_OK, args->out_err);
  TEST_ASSERT_NOT_NULL(args->out_result);
  free(args->out_result);
  aos_awaitable_free(future);

  // Give the timeout time to expire, as it will clean its stuff and allow us to
  // deinit the client
  vTaskDelay(pdMS_TO_TICKS(150));

  // Deinit client
  TEST_ASSERT_EQUAL(0, aos_jrpc_client_free(_client));
  _client = NULL;

  // Deinit mock server
  test_mock_server_deinit();

  TEST_HEAP_STOP
}

TEST_CASE("Send request with no params (text,timeout)", "[client]") {
  TEST_HEAP_START

  // Init mock server
  test_mock_server_init(test_client_read);

  // Init client
  // TODO: Test cases with too many requests for both client and server
  aos_jrpc_client_config_t client_config = {
      .maxinputlen = 1000,
      .maxrequests = 10,
      .on_output = test_client_on_output_tomockserver};
  _client = aos_jrpc_client_alloc(&client_config);
  TEST_ASSERT_NOT_NULL(_client);

  // Send request
  aos_future_t *future =
      AOS_AWAITABLE_ALLOC_T(aos_jrpc_client_request_send)(NULL, 0);
  TEST_ASSERT_NOT_NULL(future);
  aos_jrpc_client_request_send(_client, 100, "testHandlerDelayed", NULL,
                               future);

  // Await response, get results
  TEST_ASSERT_TRUE(aos_isresolved(aos_await(future)));
  AOS_ARGS_T(aos_jrpc_client_request_send) *args = aos_args_get(future);
  printf("Response (err:%d): %s\n", args->out_err,
         args->out_result ? args->out_result : "none");
  TEST_ASSERT_EQUAL(AOS_JRPC_CLIENT_ERR_TIMEOUT, args->out_err);
  TEST_ASSERT_NULL(args->out_result);
  free(args->out_result);
  aos_awaitable_free(future);

  // Deinit client
  TEST_ASSERT_EQUAL(0, aos_jrpc_client_free(_client));
  _client = NULL;

  // Deinit mock server
  test_mock_server_deinit();

  TEST_HEAP_STOP
}
