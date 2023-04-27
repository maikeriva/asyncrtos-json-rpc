/**
 * TODO:
 * - Test request limiter, and counter reeentrancy
 */
#include <aos_jrpc_server.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <test_handlers.h>
#include <test_macros.h>
#include <unity.h>
#include <unity_test_runner.h>

#define STRING_REQUEST_HANDLER0_VALID0                                         \
  "{\"jsonrpc\": \"2.0\", \"method\":\"testHandler0\", \"id\":5}"
#define STRING_REQUEST_HANDLER0_VALID1                                         \
  "{\"jsonrpc\": \"2.0\", \"method\":\"testHandler0\", \"id\":\"abcdef\"}"
#define STRING_REQUEST_HANDLER0_VALID2                                         \
  "{\"jsonrpc\": \"2.0\", \"method\":\"testHandler0\", \"id\":null}"
#define STRING_REQUEST_HANDLER0_VALID3                                         \
  "{\"jsonrpc\": \"2.0\", \"method\":\"testHandler0\"}"
#define STRING_REQUEST_HANDLER0_VALID4                                         \
  "{\"jsonrpc\": \"2.0\", \"method\":\"testHandler0\", \"params\":[1], "       \
  "\"id\":5}"
#define STRING_REQUEST_HANDLER0_INVALID0                                       \
  "{\"jsonrpc\": \"2.0\", \"method\":\"testHandler0\", \"id\":[]}"
#define STRING_REQUEST_HANDLER0_INVALID1                                       \
  "{\"jsonrpc\": \"2.0\", \"method\":\"testHandler0\", \"id\":{}}"
#define STRING_REQUEST_HANDLER0_INVALID2                                       \
  "{\"jsonrpc\": \"2.0\", \"method\":\"testHandler0\", \"id\":abcdef}"
#define STRING_REQUEST_HANDLER0_INVALID3                                       \
  "{\"jsonrpc\": \"2.1\", \"method\":\"testHandler0\", \"id\":3}"
#define STRING_REQUEST_HANDLER0_INVALID4                                       \
  "{\"jsonrpc\": \"2.1\", \"method\":\"testHandler0\"}"

#define STRING_REQUEST_HANDLER1_VALID0                                         \
  "{\"jsonrpc\": \"2.0\", \"method\":\"testHandler1\", \"params\":[1], "       \
  "\"id\":5}"
#define STRING_REQUEST_HANDLER1_VALID1                                         \
  "{\"jsonrpc\": \"2.0\", \"method\":\"testHandler1\", \"params\":[1], "       \
  "\"id\":\"abcdef\"}"
#define STRING_REQUEST_HANDLER1_VALID2                                         \
  "{\"jsonrpc\": \"2.0\", \"method\":\"testHandler1\", \"params\":[1], "       \
  "\"id\":null}"
#define STRING_REQUEST_HANDLER1_VALID3                                         \
  "{\"jsonrpc\": \"2.0\", \"method\":\"testHandler1\", \"params\":[1]}"
#define STRING_REQUEST_HANDLER1_VALID4                                         \
  "{\"jsonrpc\": \"2.0\", \"method\":\"testHandler1\", \"params\":[1,2]}"
#define STRING_REQUEST_HANDLER1_VALID5                                         \
  "{\"jsonrpc\": \"2.0\", \"method\":\"testHandler1\", "                       \
  "\"params\":{\"arg1\":1}}"
#define STRING_REQUEST_HANDLER1_VALID6                                         \
  "{\"jsonrpc\": \"2.0\", \"method\":\"testHandler1\", "                       \
  "\"params\":{\"arg1\":1,\"arg2\":2}}"
#define STRING_REQUEST_HANDLER1_INVALID0                                       \
  "{\"jsonrpc\": \"2.0\", \"method\":\"testHandler1\", \"params\":[1], "       \
  "\"id\":[]}"
#define STRING_REQUEST_HANDLER1_INVALID1                                       \
  "{\"jsonrpc\": \"2.0\", \"method\":\"testHandler1\", \"params\":[1], "       \
  "\"id\":{}}"
#define STRING_REQUEST_HANDLER1_INVALID2                                       \
  "{\"jsonrpc\": \"2.0\", \"method\":\"testHandler1\", \"params\":[1], "       \
  "\"id\":abcdef}"
#define STRING_REQUEST_HANDLER1_INVALID3                                       \
  "{\"jsonrpc\": \"2.1\", \"method\":\"testHandler1\", \"params\":[1], "       \
  "\"id\":3}"
#define STRING_REQUEST_HANDLER1_INVALID4                                       \
  "{\"jsonrpc\": \"2.1\", \"method\":\"testHandler1\", \"params\":[1]}"
#define STRING_REQUEST_HANDLER1_INVALID5                                       \
  "{\"jsonrpc\": \"2.1\", \"method\":\"testHandler1\", \"params\":{}}"
#define STRING_REQUEST_HANDLER1_INVALID6                                       \
  "{\"jsonrpc\": \"2.1\", \"method\":\"testHandler1\", \"params\":[]}"
#define STRING_REQUEST_HANDLER1_INVALID7                                       \
  "{\"jsonrpc\": \"2.1\", \"method\":\"testHandler1\"}"
#define STRING_REQUEST_HANDLER1_INVALID8                                       \
  "{\"jsonrpc\": \"2.0\", \"method\":\"testHandler1\", "                       \
  "\"params\":{\"arg2\":2}}"

// Should return both responses
#define STRING_BATCH_VALID0                                                    \
  "[" STRING_REQUEST_HANDLER0_VALID0 "," STRING_REQUEST_HANDLER1_VALID0        \
  "]" // NOTE: This should fail once we have implemented id checking among
      // active requests
// Should return both responses
#define STRING_BATCH_VALID1                                                    \
  "[" STRING_REQUEST_HANDLER0_VALID0 "," STRING_REQUEST_HANDLER0_VALID0        \
  "]" // NOTE: This should fail once we have implemented id checking among
      // active requests
// Should return one valid response and an error response
#define STRING_BATCH_VALID2                                                    \
  "[" STRING_REQUEST_HANDLER0_VALID0                                           \
  ",{\"jsonrpc\": \"2.0\", \"method\":\"unavailable\"}]"
// Should return one valid response and an error response
#define STRING_BATCH_VALID3                                                    \
  "[" STRING_REQUEST_HANDLER0_VALID0                                           \
  ",{\"jsonrpc\": \"2.0\", \"method\":\"unavailable\", \"id\":3}]"
// Should return no data
#define STRING_BATCH_VALID4                                                    \
  "[" STRING_REQUEST_HANDLER0_VALID3 "," STRING_REQUEST_HANDLER1_VALID3 "]"
// Should return a single error response
#define STRING_BATCH_INVALID0 "[]"
// Should return three error responses in an array
#define STRING_BATCH_INVALID1 "[1,2,3]"

static void test_call(aos_jrpc_server_t *server, const char *data) {
  printf("Request: %s\n", data);

  aos_future_t *future = AOS_AWAITABLE_ALLOC_T(aos_jrpc_server_call)(NULL, 0);
  TEST_ASSERT_NOT_NULL(future);

  aos_jrpc_server_call(server, data, future);

  TEST_ASSERT_TRUE(aos_isresolved(aos_await(future)));
  AOS_ARGS_T(aos_jrpc_server_call) *args = aos_args_get(future);
  if (args->out_err) {
    printf("Server error: %u\n", args->out_err);
  } else if (args->out_data) {
    printf("Response: %s\n", args->out_data);
  } else {
    printf("Notification\n");
  }

  free(args->out_data);
  aos_awaitable_free(future);
  return;
}

static void test_call_json(aos_jrpc_server_t *server, const char *data) {
  printf("Request: %s\n", data);

  cJSON *json = cJSON_Parse(data);
  if (!json) {
    printf("Parse error, not testing\n");
    return;
  }

  aos_future_t *future =
      AOS_AWAITABLE_ALLOC_T(aos_jrpc_server_call_json)(NULL, 0);
  TEST_ASSERT_NOT_NULL(future);
  aos_jrpc_server_call_json(server, json, future);
  cJSON_Delete(json);

  TEST_ASSERT_TRUE(aos_isresolved(aos_await(future)));
  AOS_ARGS_T(aos_jrpc_server_call_json) *args = aos_args_get(future);
  if (args->out_err) {
    printf("Server error: %u\n", args->out_err);
  } else if (args->out_response) {
    char *response_data = cJSON_PrintUnformatted(args->out_response);
    printf("Response: %s\n", response_data);
    free(response_data);
  } else {
    printf("Notification\n");
  }

  cJSON_Delete(args->out_response);
  aos_awaitable_free(future);
  return;
}

TEST_CASE("Alloc/Dealloc", "[server]") {
  TEST_HEAP_START

  // TODO: Test cases with too many requests for both client and server
  aos_jrpc_server_config_t config = {.maxrequests = 10, .maxinputlen = 500};
  aos_jrpc_server_t *server = aos_jrpc_server_alloc(&config);

  aos_jrpc_server_free(server);

  TEST_HEAP_STOP
}

TEST_CASE("Set/unset handlers", "[server]") {
  TEST_HEAP_START

  // TODO: Test cases with too many requests for both client and server
  aos_jrpc_server_config_t config = {.maxrequests = 10, .maxinputlen = 500};
  aos_jrpc_server_t *server = aos_jrpc_server_alloc(&config);
  TEST_ASSERT_EQUAL(
      0, aos_jrpc_server_handler_set(server, test_handler0, "testHandler0"));
  TEST_ASSERT_EQUAL(
      0, aos_jrpc_server_handler_set(server, test_handler1, "testHandler1"));

  TEST_ASSERT_EQUAL(0, aos_jrpc_server_handler_unset(server, "testHandler0"));
  TEST_ASSERT_EQUAL(0, aos_jrpc_server_handler_unset(server, "testHandler1"));

  aos_jrpc_server_free(server);

  TEST_HEAP_STOP
}

TEST_CASE("Parse single requests (string)", "[server]") {
  TEST_HEAP_START

  // TODO: Test cases with too many requests for both client and server
  aos_jrpc_server_config_t config = {.maxrequests = 10, .maxinputlen = 500};
  aos_jrpc_server_t *server = aos_jrpc_server_alloc(&config);
  TEST_ASSERT_EQUAL(
      0, aos_jrpc_server_handler_set(server, test_handler0, "testHandler0"));
  TEST_ASSERT_EQUAL(
      0, aos_jrpc_server_handler_set(server, test_handler1, "testHandler1"));

  test_call(server, STRING_REQUEST_HANDLER0_VALID0);
  test_call(server, STRING_REQUEST_HANDLER0_VALID1);
  test_call(server, STRING_REQUEST_HANDLER0_VALID2);
  test_call(server, STRING_REQUEST_HANDLER0_VALID3);
  test_call(server, STRING_REQUEST_HANDLER0_VALID4);
  test_call(server, STRING_REQUEST_HANDLER0_INVALID0);
  test_call(server, STRING_REQUEST_HANDLER0_INVALID1);
  test_call(server, STRING_REQUEST_HANDLER0_INVALID2);
  test_call(server, STRING_REQUEST_HANDLER0_INVALID3);
  test_call(server, STRING_REQUEST_HANDLER0_INVALID4);
  test_call(server, STRING_REQUEST_HANDLER1_VALID0);
  test_call(server, STRING_REQUEST_HANDLER1_VALID1);
  test_call(server, STRING_REQUEST_HANDLER1_VALID2);
  test_call(server, STRING_REQUEST_HANDLER1_VALID3);
  test_call(server, STRING_REQUEST_HANDLER1_VALID4);
  test_call(server, STRING_REQUEST_HANDLER1_VALID5);
  test_call(server, STRING_REQUEST_HANDLER1_VALID6);
  test_call(server, STRING_REQUEST_HANDLER1_INVALID0);
  test_call(server, STRING_REQUEST_HANDLER1_INVALID1);
  test_call(server, STRING_REQUEST_HANDLER1_INVALID3);
  test_call(server, STRING_REQUEST_HANDLER1_INVALID4);
  test_call(server, STRING_REQUEST_HANDLER1_INVALID5);
  test_call(server, STRING_REQUEST_HANDLER1_INVALID6);
  test_call(server, STRING_REQUEST_HANDLER1_INVALID7);
  test_call(server, STRING_REQUEST_HANDLER1_INVALID8);

  TEST_ASSERT_EQUAL(0, aos_jrpc_server_handler_unset(server, "testHandler0"));
  TEST_ASSERT_EQUAL(0, aos_jrpc_server_handler_unset(server, "testHandler1"));

  aos_jrpc_server_free(server);

  TEST_HEAP_STOP
}

TEST_CASE("Parse single requests (json)", "[server]") {
  TEST_HEAP_START

  // TODO: Test cases with too many requests for both client and server
  aos_jrpc_server_config_t config = {.maxrequests = 10, .maxinputlen = 500};
  aos_jrpc_server_t *server = aos_jrpc_server_alloc(&config);
  TEST_ASSERT_EQUAL(
      0, aos_jrpc_server_handler_set(server, test_handler0, "testHandler0"));
  TEST_ASSERT_EQUAL(
      0, aos_jrpc_server_handler_set(server, test_handler1, "testHandler1"));

  test_call_json(server, STRING_REQUEST_HANDLER0_VALID0);
  test_call_json(server, STRING_REQUEST_HANDLER0_VALID1);
  test_call_json(server, STRING_REQUEST_HANDLER0_VALID2);
  test_call_json(server, STRING_REQUEST_HANDLER0_VALID3);
  test_call_json(server, STRING_REQUEST_HANDLER0_VALID4);
  test_call_json(server, STRING_REQUEST_HANDLER0_INVALID0);
  test_call_json(server, STRING_REQUEST_HANDLER0_INVALID1);
  test_call_json(server, STRING_REQUEST_HANDLER0_INVALID2);
  test_call_json(server, STRING_REQUEST_HANDLER0_INVALID3);
  test_call_json(server, STRING_REQUEST_HANDLER0_INVALID4);
  test_call_json(server, STRING_REQUEST_HANDLER1_VALID0);
  test_call_json(server, STRING_REQUEST_HANDLER1_VALID1);
  test_call_json(server, STRING_REQUEST_HANDLER1_VALID2);
  test_call_json(server, STRING_REQUEST_HANDLER1_VALID3);
  test_call_json(server, STRING_REQUEST_HANDLER1_VALID4);
  test_call_json(server, STRING_REQUEST_HANDLER1_VALID5);
  test_call_json(server, STRING_REQUEST_HANDLER1_VALID6);
  test_call_json(server, STRING_REQUEST_HANDLER1_INVALID0);
  test_call_json(server, STRING_REQUEST_HANDLER1_INVALID1);
  test_call_json(server, STRING_REQUEST_HANDLER1_INVALID3);
  test_call_json(server, STRING_REQUEST_HANDLER1_INVALID4);
  test_call_json(server, STRING_REQUEST_HANDLER1_INVALID5);
  test_call_json(server, STRING_REQUEST_HANDLER1_INVALID6);
  test_call_json(server, STRING_REQUEST_HANDLER1_INVALID7);
  test_call_json(server, STRING_REQUEST_HANDLER1_INVALID8);

  TEST_ASSERT_EQUAL(0, aos_jrpc_server_handler_unset(server, "testHandler0"));
  TEST_ASSERT_EQUAL(0, aos_jrpc_server_handler_unset(server, "testHandler1"));

  aos_jrpc_server_free(server);

  TEST_HEAP_STOP
}

TEST_CASE("Parse sequential batch", "[server]") {
  TEST_HEAP_START

  // TODO: Test cases with too many requests for both client and server
  aos_jrpc_server_config_t config = {.maxrequests = 10, .maxinputlen = 500};
  aos_jrpc_server_t *server = aos_jrpc_server_alloc(&config);
  TEST_ASSERT_EQUAL(
      0, aos_jrpc_server_handler_set(server, test_handler0, "testHandler0"));
  TEST_ASSERT_EQUAL(
      0, aos_jrpc_server_handler_set(server, test_handler1, "testHandler1"));

  test_call(server, STRING_BATCH_VALID0);
  test_call(server, STRING_BATCH_VALID1);
  test_call(server, STRING_BATCH_VALID2);
  test_call(server, STRING_BATCH_VALID3);
  test_call(server, STRING_BATCH_VALID4);
  test_call(server, STRING_BATCH_INVALID0);
  test_call(server, STRING_BATCH_INVALID1);

  TEST_ASSERT_EQUAL(0, aos_jrpc_server_handler_unset(server, "testHandler0"));
  TEST_ASSERT_EQUAL(0, aos_jrpc_server_handler_unset(server, "testHandler1"));

  aos_jrpc_server_free(server);

  TEST_HEAP_STOP
}

TEST_CASE("Parse parallel batch", "[server]") {
  TEST_HEAP_START

  // TODO: Test cases with too many requests for both client and server
  aos_jrpc_server_config_t config = {.maxrequests = 10, .maxinputlen = 500};
  aos_jrpc_server_t *server = aos_jrpc_server_alloc(&config);
  TEST_ASSERT_EQUAL(
      0, aos_jrpc_server_handler_set(server, test_handler0, "testHandler0"));
  TEST_ASSERT_EQUAL(
      0, aos_jrpc_server_handler_set(server, test_handler1, "testHandler1"));

  test_call(server, STRING_BATCH_VALID0);
  test_call(server, STRING_BATCH_VALID1);
  test_call(server, STRING_BATCH_VALID2);
  test_call(server, STRING_BATCH_VALID3);
  test_call(server, STRING_BATCH_VALID4);
  test_call(server, STRING_BATCH_INVALID0);
  test_call(server, STRING_BATCH_INVALID1);

  TEST_ASSERT_EQUAL(0, aos_jrpc_server_handler_unset(server, "testHandler0"));
  TEST_ASSERT_EQUAL(0, aos_jrpc_server_handler_unset(server, "testHandler1"));

  aos_jrpc_server_free(server);

  TEST_HEAP_STOP
}
