/**
 * @file main.c
 * @author Michele Riva (micheleriva@protonmail.com)
 * @brief AsyncRTOS JSON-RPC peer example 0
 * @version 0.9.0
 * @date 2023-04-17
 *
 * @copyright Copyright (c) 2023
 *
 * This example shows how to setup a JSON-RPC peer over Websocket
 *
 * @note: The JSON-RPC peer is implemented as a shared context rather than a
 * task. For this reason, its internals are always executed by the task
 * currently invoking its functions. While this has the advantage of saving
 * another task's worth of resources, it makes using awaitable futures
 * troublesome in some occasions.
 *
 * For instance, in this example `aos_jrpc_peer_read` is executed by the
 * Websocket task in `ws_on_data`. That in turn executes `jrpc_on_output` when
 * the processing is done, and then `aos_ws_client_send_text` with a generic
 * future. This chain of exeutions is wholly performed by the Websocket task.
 *
 * If we had used an awaitable and awaited it in-place, the Websocket task would
 * be blocked and incapable of responding to inputs until
 * `aos_ws_client_send_text` completed, but that WOULD NEVER HAPPEN as the
 * Websocket task is indeed blocked and incapable of processing the request.
 */
#include <aos.h>
#include <aos_jrpc_peer.h>
#include <aos_wifi_client.h>
#include <aos_ws_client.h>
#include <esp_netif.h>
#include <esp_tls.h>
#include <stdio.h>

static const char *_ssid = "MY_SSID";
static const char *_password = "MY_PASSWORD";
static const char *_ws_host = "ws.postman-echo.com";
extern const uint8_t
    server_root_cert_pem_start[] asm("_binary_postman_echo_com_pem_start");
extern const uint8_t
    server_root_cert_pem_end[] asm("_binary_postman_echo_com_pem_end");

static aos_task_t *_ws_task = NULL;
static aos_jrpc_peer_t *_jrpc_peer = NULL;

static void wifi_event_handler(aos_wifi_client_event_t event, void *args) {
  printf("Received WiFi event (%d)\n", event);
}

static void ws_event_handler(aos_ws_client_event_t event, void *args) {
  printf("Received Websocket event (%d)\n", event);
}

static void ws_on_data(const void *data, size_t data_len) {
  // Pipe data in the JSON-RPC peer
  printf("Websocket client received:%.*s\n", data_len, (char *)data);
  aos_jrpc_peer_read(_jrpc_peer, data);
}

static void jrpc_on_error(unsigned int err) {
  printf("JSON-RPC raised an error (%u)\n", err);
}

static void jrpc_on_output_cb(aos_future_t *future);
static unsigned int jrpc_on_output(const char *data) {
  // Send output data through Websocket
  // NOTE: Read the note at the top of the file on why we use a generic future
  aos_future_config_t config = {.cb = jrpc_on_output_cb};
  char *data_dup = strdup(data);
  aos_future_t *send_future =
      AOS_FUTURE_ALLOC_T(aos_ws_client_send_text)(&config, data_dup, 0);
  aos_ws_client_send_text(_ws_task, send_future);
  return 0;
}

// A generic handler for the "dosomething" method
static void jrpc_handler_dosomething(cJSON *params, aos_future_t *future) {
  AOS_ARGS_T(aos_jrpc_server_handler) *args = aos_args_get(future);

  // Extract arguments from parameters structure
  unsigned int arg0 = 0;
  if (aos_jrpc_server_param_uint32_get(params, 0, "arg0", &arg0)) {
    // Parameter not found, resolve early with INVALIDPARAMS error
    args->out_err = AOS_JRPC_SERVER_ERR_INVALIDPARAMS;
    aos_resolve(future);
    return;
  }

  printf("Executing handler for \"dosomething\" (arg0:%u)\n", arg0);
  aos_resolve(future);
}

void app_main(void) {
  // Initialize ESP netif and global CA store
  esp_netif_init();
  esp_tls_set_global_ca_store(server_root_cert_pem_start,
                              server_root_cert_pem_end -
                                  server_root_cert_pem_start);

  // Initialize AOS WiFi client
  // All fields are mandatory, we want to be explicit
  aos_wifi_client_config_t wifi_config = {.connection_attempts = UINT32_MAX,
                                          .reconnection_attempts = UINT32_MAX,
                                          .event_handler = wifi_event_handler};
  aos_wifi_client_init(&wifi_config);

  // Initialize AOS Websocket client
  aos_ws_client_config_t ws_config = {.connection_attempts = UINT32_MAX,
                                      .reconnection_attempts = UINT32_MAX,
                                      .event_handler = ws_event_handler,
                                      .on_data = ws_on_data,
                                      .host = _ws_host,
                                      .path = "/raw"};
  _ws_task = aos_ws_client_alloc(&ws_config);

  // Initialize AOS JSON-RPC peer
  aos_jrpc_peer_config_t peer_config = {
      .on_error = jrpc_on_error,
      .on_output = jrpc_on_output,
  };
  _jrpc_peer = aos_jrpc_peer_alloc(&peer_config);

  // Set JSON-RPC handlers
  aos_jrpc_server_handler_set(_jrpc_peer->server, jrpc_handler_dosomething,
                              "dosomething");

  // Start the wifi client
  aos_future_t *wifi_start = AOS_AWAITABLE_ALLOC_T(aos_wifi_client_start)(0);
  aos_await(aos_wifi_client_start(wifi_start));
  aos_awaitable_free(wifi_start);

  // Start the websocket client
  aos_future_t *ws_start = aos_awaitable_alloc(0);
  aos_await(aos_task_start(_ws_task, ws_start));
  aos_awaitable_free(ws_start);

  // Connect to network
  aos_future_t *connect =
      AOS_AWAITABLE_ALLOC_T(aos_wifi_client_connect)(_ssid, _password, 0);
  aos_await(aos_wifi_client_connect(connect));
  aos_awaitable_free(connect);

  // Connect to websocket
  aos_future_t *ws_connect = AOS_AWAITABLE_ALLOC_T(aos_ws_client_connect)(0);
  aos_await(aos_ws_client_connect(_ws_task, ws_connect));
  aos_awaitable_free(ws_connect);

  // All good! Send a test message
  aos_future_t *send_future =
      AOS_AWAITABLE_ALLOC_T(aos_jrpc_client_request_send)(NULL, 0);
  aos_jrpc_client_request_send(_jrpc_peer->client, 3000, "dosomething", NULL,
                               send_future);
  aos_await(send_future);
  AOS_ARGS_T(aos_jrpc_client_request_send) *send_args =
      aos_args_get(send_future);
  if (send_args->out_err) {
    printf("Error while making JSON-RPC request:%d\n", send_args->out_err);
  } else {
    printf("Received JSON-RPC response:%s\n", send_args->out_result);
  }
  free(send_args->out_result);
  aos_awaitable_free(send_future);

  // Let's now shut down

  // Stop and free websocket
  aos_future_t *ws_stop = aos_awaitable_alloc(0);
  aos_await(aos_task_stop(_ws_task, ws_stop));
  aos_awaitable_free(ws_stop);
  aos_ws_client_free(_ws_task);

  // Free JSON-RPC peer
  // NOTE: `aos_jrpc_peer_free` will fail if not all `_send` timeouts have
  // expired yet. This is regardless of whether JSON-RPC messages got sent or
  // not.
  vTaskDelay(pdMS_TO_TICKS(3000));
  aos_jrpc_peer_free(_jrpc_peer);
}

static void jrpc_on_output_cb(aos_future_t *future) {
  AOS_ARGS_T(aos_ws_client_send_text) *args = aos_args_get(future);
  free(args->in_data);
  aos_future_free(future);
}