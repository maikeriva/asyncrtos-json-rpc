#include <aos_jrpc_peer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <test_handlers.h>
#include <test_macros.h>
#include <test_mock_server.h>
#include <unity.h>
#include <unity_test_runner.h>

#define STRING_REQUEST_HANDLER0_VALID0                                         \
  "{\"jsonrpc\": \"2.0\", \"method\":\"testHandler0\", \"id\":5}"
#define STRING_REQUEST_HANDLER0_INVALID0                                       \
  "{\"jsonrpc\": \"2.0\", \"method\":\"testHandler0\", \"id\":[]}"

#define STRING_REQUEST_HANDLER1_VALID0                                         \
  "{\"jsonrpc\": \"2.0\", \"method\":\"testHandler1\", \"params\":[1], "       \
  "\"id\":5}"

#define STRING_BATCH_VALID0                                                    \
  "[" STRING_REQUEST_HANDLER0_VALID0 "," STRING_REQUEST_HANDLER1_VALID0        \
  "]" // NOTE: This should fail once we have implemented id checking among
      // active requests
#define STRING_BATCH_INVALID0 "[]"
#define STRING_BATCH_MIXED0                                                    \
  "[" STRING_REQUEST_HANDLER0_VALID0                                           \
  ",{\"jsonrpc\": \"2.0\", \"id\":5, \"response\":\"0\"}]"

static aos_jrpc_peer_t *_peer = NULL;
static bool _simulateOutputFail = false;

unsigned int test_peer_on_output(const char *data) {
  printf("Peer output: %s\n", data);
  return _simulateOutputFail ? 1 : 0;
}

unsigned int test_peer_on_output_tomockserver(const char *data) {
  printf("Peer output: %s\n", data);
  if (_simulateOutputFail)
    return 1;
  test_mock_server_read(data);
  return 0;
}

void test_peer_on_error(unsigned int err) { printf("Peer error: %u\n", err); }

void test_peer_read(const char *data) {
  if (!_peer) {
    return;
  }
  printf("Peer input: %s\n", data);
  aos_jrpc_peer_read(_peer, data);
}

TEST_CASE("Alloc/dealloc", "[peer]") {
  TEST_HEAP_START

  aos_jrpc_peer_config_t config = {.maxclientrequests = 10,
                                   .maxserverrequests = 10,
                                   .maxinputlen = 1000,
                                   .on_output = test_peer_on_output,
                                   .on_error = test_peer_on_error};
  _peer = aos_jrpc_peer_alloc(&config);
  TEST_ASSERT_NOT_NULL(_peer);
  TEST_ASSERT_EQUAL(0, aos_jrpc_peer_free(_peer));
  _peer = NULL;

  TEST_HEAP_STOP
}

TEST_CASE("Handle single request (text)", "[peer]") {
  TEST_HEAP_START

  aos_jrpc_peer_config_t config = {.maxclientrequests = 10,
                                   .maxserverrequests = 10,
                                   .maxinputlen = 1000,
                                   .on_output = test_peer_on_output,
                                   .on_error = test_peer_on_error};
  _peer = aos_jrpc_peer_alloc(&config);
  TEST_ASSERT_NOT_NULL(_peer);
  TEST_ASSERT_EQUAL(0, aos_jrpc_server_handler_set(_peer->server, test_handler0,
                                                   "testHandler0"));
  TEST_ASSERT_EQUAL(0, aos_jrpc_server_handler_set(_peer->server, test_handler1,
                                                   "testHandler1"));
  TEST_ASSERT_EQUAL(0, aos_jrpc_server_handler_set(_peer->server,
                                                   test_handler_delayed,
                                                   "testHandlerDelayed"));

  printf("Peer input: %s\n", STRING_REQUEST_HANDLER0_VALID0);
  TEST_ASSERT_EQUAL(0,
                    aos_jrpc_peer_read(_peer, STRING_REQUEST_HANDLER0_VALID0));
  printf("Peer input: %s\n", STRING_REQUEST_HANDLER0_INVALID0);
  TEST_ASSERT_EQUAL(
      0, aos_jrpc_peer_read(_peer, STRING_REQUEST_HANDLER0_INVALID0));

  TEST_ASSERT_EQUAL(0, aos_jrpc_peer_free(_peer));
  _peer = NULL;

  TEST_HEAP_STOP
}

TEST_CASE("Handle batch request (text)", "[peer]") {
  TEST_HEAP_START

  aos_jrpc_peer_config_t config = {.maxclientrequests = 10,
                                   .maxserverrequests = 10,
                                   .maxinputlen = 1000,
                                   .on_output = test_peer_on_output,
                                   .on_error = test_peer_on_error};
  _peer = aos_jrpc_peer_alloc(&config);
  TEST_ASSERT_NOT_NULL(_peer);
  TEST_ASSERT_EQUAL(0, aos_jrpc_server_handler_set(_peer->server, test_handler0,
                                                   "testHandler0"));
  TEST_ASSERT_EQUAL(0, aos_jrpc_server_handler_set(_peer->server, test_handler1,
                                                   "testHandler1"));
  TEST_ASSERT_EQUAL(0, aos_jrpc_server_handler_set(_peer->server,
                                                   test_handler_delayed,
                                                   "testHandlerDelayed"));

  printf("Peer input: %s\n", STRING_BATCH_VALID0);
  TEST_ASSERT_EQUAL(0, aos_jrpc_peer_read(_peer, STRING_BATCH_VALID0));
  printf("Peer input: %s\n", STRING_BATCH_INVALID0);
  TEST_ASSERT_EQUAL(0, aos_jrpc_peer_read(_peer, STRING_BATCH_INVALID0));
  printf("Peer input: %s\n", STRING_BATCH_MIXED0);
  TEST_ASSERT_EQUAL(0, aos_jrpc_peer_read(_peer, STRING_BATCH_MIXED0));

  TEST_ASSERT_EQUAL(0, aos_jrpc_peer_free(_peer));
  _peer = NULL;

  TEST_HEAP_STOP
}

TEST_CASE("Handle single request (json)", "[peer]") {
  TEST_HEAP_START

  aos_jrpc_peer_config_t config = {.maxclientrequests = 10,
                                   .maxserverrequests = 10,
                                   .maxinputlen = 1000,
                                   .on_output = test_peer_on_output,
                                   .on_error = test_peer_on_error};
  _peer = aos_jrpc_peer_alloc(&config);
  TEST_ASSERT_NOT_NULL(_peer);
  TEST_ASSERT_EQUAL(0, aos_jrpc_server_handler_set(_peer->server, test_handler0,
                                                   "testHandler0"));
  TEST_ASSERT_EQUAL(0, aos_jrpc_server_handler_set(_peer->server, test_handler1,
                                                   "testHandler1"));
  TEST_ASSERT_EQUAL(0, aos_jrpc_server_handler_set(_peer->server,
                                                   test_handler_delayed,
                                                   "testHandlerDelayed"));

  printf("Peer input: %s\n", STRING_REQUEST_HANDLER0_VALID0);
  cJSON *req0 = cJSON_Parse(STRING_REQUEST_HANDLER0_VALID0);
  TEST_ASSERT_NOT_NULL(req0);
  TEST_ASSERT_EQUAL(0, aos_jrpc_peer_read_json(_peer, req0));
  cJSON_Delete(req0);

  printf("Peer input: %s\n", STRING_REQUEST_HANDLER0_INVALID0);
  cJSON *req1 = cJSON_Parse(STRING_REQUEST_HANDLER0_INVALID0);
  TEST_ASSERT_NOT_NULL(req1);
  TEST_ASSERT_EQUAL(0, aos_jrpc_peer_read_json(_peer, req1));
  cJSON_Delete(req1);

  TEST_ASSERT_EQUAL(0, aos_jrpc_peer_free(_peer));
  _peer = NULL;

  TEST_HEAP_STOP
}

TEST_CASE("Handle batch request (json)", "[peer]") {
  TEST_HEAP_START

  aos_jrpc_peer_config_t config = {.maxclientrequests = 10,
                                   .maxserverrequests = 10,
                                   .maxinputlen = 1000,
                                   .on_output = test_peer_on_output,
                                   .on_error = test_peer_on_error};
  _peer = aos_jrpc_peer_alloc(&config);
  TEST_ASSERT_NOT_NULL(_peer);
  TEST_ASSERT_EQUAL(0, aos_jrpc_server_handler_set(_peer->server, test_handler0,
                                                   "testHandler0"));
  TEST_ASSERT_EQUAL(0, aos_jrpc_server_handler_set(_peer->server, test_handler1,
                                                   "testHandler1"));
  TEST_ASSERT_EQUAL(0, aos_jrpc_server_handler_set(_peer->server,
                                                   test_handler_delayed,
                                                   "testHandlerDelayed"));

  printf("Peer input: %s\n", STRING_BATCH_VALID0);
  cJSON *req0 = cJSON_Parse(STRING_BATCH_VALID0);
  TEST_ASSERT_NOT_NULL(req0);
  TEST_ASSERT_EQUAL(0, aos_jrpc_peer_read(_peer, STRING_BATCH_VALID0));
  cJSON_Delete(req0);

  printf("Peer input: %s\n", STRING_BATCH_INVALID0);
  cJSON *req1 = cJSON_Parse(STRING_BATCH_INVALID0);
  TEST_ASSERT_NOT_NULL(req1);
  TEST_ASSERT_EQUAL(0, aos_jrpc_peer_read(_peer, STRING_BATCH_INVALID0));
  cJSON_Delete(req1);

  printf("Peer input: %s\n", STRING_BATCH_MIXED0);
  cJSON *req2 = cJSON_Parse(STRING_BATCH_MIXED0);
  TEST_ASSERT_NOT_NULL(req2);
  TEST_ASSERT_EQUAL(0, aos_jrpc_peer_read(_peer, STRING_BATCH_MIXED0));
  cJSON_Delete(req2);

  TEST_ASSERT_EQUAL(0, aos_jrpc_peer_free(_peer));
  _peer = NULL;

  TEST_HEAP_STOP
}

TEST_CASE("Send request (text)", "[peer]") {
  TEST_HEAP_START

  test_mock_server_init(test_peer_read);

  aos_jrpc_peer_config_t config = {.maxclientrequests = 10,
                                   .maxserverrequests = 10,
                                   .maxinputlen = 1000,
                                   .on_output =
                                       test_peer_on_output_tomockserver,
                                   .on_error = test_peer_on_error};
  _peer = aos_jrpc_peer_alloc(&config);
  TEST_ASSERT_NOT_NULL(_peer);

  aos_future_t *req0 =
      AOS_AWAITABLE_ALLOC_T(aos_jrpc_client_request_send)(NULL, 0);
  TEST_ASSERT_NOT_NULL(req0);
  aos_jrpc_client_request_send(_peer->client, 100, "testHandler0", NULL, req0);
  TEST_ASSERT_TRUE(aos_isresolved(aos_await(req0)));
  AOS_ARGS_T(aos_jrpc_client_request_send) *args0 = aos_args_get(req0);
  TEST_ASSERT_EQUAL(AOS_JRPC_CLIENT_ERR_OK, args0->out_err);
  TEST_ASSERT_NOT_NULL(args0->out_result);
  printf("Response (err:%d): %s\n", args0->out_err,
         args0->out_result ? args0->out_result : "none");
  free(args0->out_result);
  aos_awaitable_free(req0);

  aos_future_t *req1 =
      AOS_AWAITABLE_ALLOC_T(aos_jrpc_client_request_send)(NULL, 0);
  TEST_ASSERT_NOT_NULL(req1);
  aos_jrpc_client_request_send(_peer->client, 100, "testHandler1", "[1]", req1);
  TEST_ASSERT_TRUE(aos_isresolved(aos_await(req1)));
  AOS_ARGS_T(aos_jrpc_client_request_send) *args1 = aos_args_get(req1);
  TEST_ASSERT_EQUAL(AOS_JRPC_CLIENT_ERR_OK, args1->out_err);
  TEST_ASSERT_NOT_NULL(args1->out_result);
  printf("Response (err:%d): %s\n", args1->out_err,
         args1->out_result ? args1->out_result : "none");
  free(args1->out_result);
  aos_awaitable_free(req1);

  aos_future_t *req2 =
      AOS_AWAITABLE_ALLOC_T(aos_jrpc_client_request_send)(NULL, 0);
  TEST_ASSERT_NOT_NULL(req2);
  aos_jrpc_client_request_send(_peer->client, 100, "testHandlerDelayed", NULL,
                               req2);
  TEST_ASSERT_TRUE(aos_isresolved(aos_await(req2)));
  AOS_ARGS_T(aos_jrpc_client_request_send) *args2 = aos_args_get(req2);
  TEST_ASSERT_EQUAL(AOS_JRPC_CLIENT_ERR_TIMEOUT, args2->out_err);
  TEST_ASSERT_NULL(args2->out_result);
  printf("Response (err:%d): %s\n", args2->out_err,
         args2->out_result ? args2->out_result : "none");
  free(args2->out_result);
  aos_awaitable_free(req2);

  TEST_ASSERT_EQUAL(0, aos_jrpc_peer_free(_peer));
  _peer = NULL;

  test_mock_server_deinit();

  TEST_HEAP_STOP
}
