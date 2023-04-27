/**
 * @file aos_jrpc_server.c
 * @author Michele Riva (michele.riva@pm.me)
 * @brief AsyncRTOS JSON-RPC server implementation
 * @version 0.9.0
 * @date 2023-04-25
 *
 * @copyright Copyright (c) 2023
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless futureuired by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */
#include <aos_jrpc_message.h>
#include <aos_jrpc_server.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <math.h>
#include <sdkconfig.h>
#include <string.h>
#if CONFIG_AOS_JRPC_SERVER_LOG_NONE
#define LOG_LOCAL_LEVEL ESP_LOG_NONE
#elif CONFIG_AOS_JRPC_SERVER_LOG_ERROR
#define LOG_LOCAL_LEVEL ESP_LOG_ERROR
#elif CONFIG_AOS_JRPC_SERVER_LOG_WARN
#define LOG_LOCAL_LEVEL ESP_LOG_WARN
#elif CONFIG_AOS_JRPC_SERVER_LOG_INFO
#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#elif CONFIG_AOS_JRPC_SERVER_LOG_DEBUG
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#elif CONFIG_AOS_JRPC_SERVER_LOG_VERBOSE
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#endif
#include <esp_log.h>

// static const char *_tag = "AOS JSON-RPC server";

typedef struct _aos_jrpc_server_handler_entry_t
    _aos_jrpc_server_handler_entry_t;
struct _aos_jrpc_server_handler_entry_t {
  char *method;
  aos_jrpc_server_handler_t handler;
  _aos_jrpc_server_handler_entry_t *next;
};

struct _aos_jrpc_server_t {
  aos_jrpc_server_config_t config;
  SemaphoreHandle_t semaphore;
  _aos_jrpc_server_handler_entry_t *handlers;
  uint32_t counter;
};

AOS_DEFINE(aos_jrpc_server_handler, cJSON *, aos_jrpc_server_err_t)
static void aos_jrpc_server_call_cb(aos_future_t *future);
static void _aos_jrpc_server_request_handle(aos_jrpc_server_t *server,
                                            cJSON *request,
                                            aos_future_t *future);
static void _aos_jrpc_server_request_handle_cb(aos_future_t *future);
static void _aos_jrpc_server_batch_handle_sequential(aos_jrpc_server_t *server,
                                                     cJSON *request,
                                                     aos_future_t *future);
static void _aos_jrpc_server_batch_handle_sequential_cb(aos_future_t *future);
static void _aos_jrpc_server_batch_handle_parallel(aos_jrpc_server_t *server,
                                                   cJSON *request,
                                                   aos_future_t *future);
static void _aos_jrpc_server_batch_handle_parallel_cb(aos_future_t *future);
static aos_jrpc_server_handler_t
_aos_jrpc_server_handler_get(aos_jrpc_server_t *server, const char *method);
static bool _aos_jrpc_server_isvalid(cJSON *request);

aos_jrpc_server_t *aos_jrpc_server_alloc(aos_jrpc_server_config_t *config) {
  // Build config
  aos_jrpc_server_config_t complete_config = {
      .maxrequests = config->maxrequests ? config->maxrequests
                                         : CONFIG_AOS_JRPC_SERVER_MAXREQUESTS,
      .maxinputlen = config->maxinputlen ? config->maxinputlen
                                         : CONFIG_AOS_JRPC_SERVER_MAXINPUTLEN,
      .parallel = config->parallel};

  aos_jrpc_server_t *server = calloc(1, sizeof(aos_jrpc_server_t));
  SemaphoreHandle_t semaphore = xSemaphoreCreateRecursiveMutex();
  if (!server || !semaphore) {
    free(server);
    if (semaphore) {
      vSemaphoreDelete(semaphore);
    }
    return NULL;
  }
  server->config = complete_config;
  server->semaphore = semaphore;
  return server;
}

void aos_jrpc_server_free(aos_jrpc_server_t *server) {
  // Unset all handlers
  _aos_jrpc_server_handler_entry_t **entry = &server->handlers;
  while (*entry) {
    _aos_jrpc_server_handler_entry_t *entry_next = (*entry)->next;
    free((*entry)->method);
    free((*entry));
    *entry = entry_next;
  }
  // Delete server
  vSemaphoreDelete(server->semaphore);
  free(server);
}

AOS_DEFINE(aos_jrpc_server_call, char *, unsigned int)
void aos_jrpc_server_call(aos_jrpc_server_t *server, const char *data,
                          aos_future_t *future) {
  AOS_ARGS_T(aos_jrpc_server_call) *args = aos_args_get(future);
  cJSON *err_response = NULL;
  cJSON *request = NULL;

  if (strlen(data) > server->config.maxinputlen) {
    err_response = aos_jrpc_message_error(
        NULL, -32000, "Server error"); // NOTE: -32000 means input too long
    goto aos_jrpc_server_call_err;
  }

  request = cJSON_Parse(data);
  if (!request) {
    err_response = aos_jrpc_message_error(NULL, -32700, "Parse error");
    goto aos_jrpc_server_call_err;
  }

  aos_future_config_t config = {.cb = aos_jrpc_server_call_cb, .ctx = future};
  aos_future_t *json_future =
      AOS_FUTURE_ALLOC_T(aos_jrpc_server_call_json)(&config, NULL, 0);
  if (!json_future) {
    err_response = aos_jrpc_message_error(NULL, -32603, "Internal error");
    goto aos_jrpc_server_call_err;
  }
  aos_jrpc_server_call_json(server, request, json_future);
  cJSON_Delete(request);
  return;

aos_jrpc_server_call_err:
  cJSON_Delete(request);
  args->out_data = cJSON_PrintUnformatted(err_response);
  cJSON_Delete(err_response);
  if (!args->out_data) {
    args->out_err = 1;
  }
  aos_resolve(future);
  return;
}

static void aos_jrpc_server_call_cb(aos_future_t *future) {
  AOS_ARGS_T(aos_jrpc_server_call_json) *args = aos_args_get(future);
  cJSON *out_response = args->out_response;
  unsigned int out_err = args->out_err;

  aos_future_t *call_future = aos_future_free(future);
  AOS_ARGS_T(aos_jrpc_server_call) *call_args = aos_args_get(call_future);

  if (out_err) {
    call_args->out_err = 1;
    goto aos_jrpc_server_call_cb_end;
  }

  if (out_response) {
    call_args->out_data = cJSON_PrintUnformatted(out_response);
    if (!call_args->out_data) {
      call_args->out_err = 1;
      goto aos_jrpc_server_call_cb_end;
    }
  }

aos_jrpc_server_call_cb_end:
  cJSON_Delete(out_response);
  aos_resolve(call_future);
}

AOS_DEFINE(aos_jrpc_server_call_json, cJSON *, unsigned int)
void aos_jrpc_server_call_json(aos_jrpc_server_t *server, cJSON *data,
                               aos_future_t *future) {
  AOS_ARGS_T(aos_jrpc_server_call_json) *args = aos_args_get(future);

  if (cJSON_IsObject(data)) {
    _aos_jrpc_server_request_handle(server, data, future);
  } else if (cJSON_IsArray(data)) {
    if (!server->config.parallel) {
      _aos_jrpc_server_batch_handle_sequential(server, data, future);
    } else {
      _aos_jrpc_server_batch_handle_parallel(server, data, future);
    }
  } else {
    args->out_response =
        aos_jrpc_message_error(NULL, -32600, "Invalid Request");
    if (!args->out_response) {
      args->out_err = 1;
    }
    aos_resolve(future);
  }
}

typedef struct _aos_jrpc_server_request_handle_ctx_t {
  aos_future_t *future;
  cJSON *id;
  aos_jrpc_server_t *server;
} _aos_jrpc_server_request_handle_ctx_t;
static void _aos_jrpc_server_request_handle(aos_jrpc_server_t *server,
                                            cJSON *request,
                                            aos_future_t *future) {
  AOS_ARGS_T(aos_jrpc_server_call_json) *args = aos_args_get(future);
  cJSON *id = NULL;
  _aos_jrpc_server_request_handle_ctx_t *ctx = NULL;

  xSemaphoreTakeRecursive(server->semaphore, portMAX_DELAY);
  server->counter++;
  xSemaphoreGiveRecursive(server->semaphore);

  // Too many requests?
  if (server->counter >= server->config.maxrequests) {
    args->out_response = aos_jrpc_message_error(
        id, -32001, "Server error"); // NOTE: -32001 means too many requests
    goto _aos_jrpc_server_request_handle_err;
  }

  // Is valid?
  if (!_aos_jrpc_server_isvalid(request)) {
    // Invalid payload
    args->out_response = aos_jrpc_message_error(id, -32600, "Invalid Request");
    goto _aos_jrpc_server_request_handle_err;
  }

  // Get ID if any
  id = cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(request, "id"), false);
  if (cJSON_GetObjectItemCaseSensitive(request, "id") && !id) {
    args->out_response = aos_jrpc_message_error(id, -32603, "Internal error");
    goto _aos_jrpc_server_request_handle_err;
  }

  // UNIMPLEMENTED: Check id is not currently in use
  // To do this we need to have a server struct containing pointers to current
  // contexts, so that we can parse them to look whether an id is currently
  // being used Together with this, we should also prevent the server from being
  // freed if a request (which could be async) is currently in progress. To
  // accomplish it, check if any id is currently being processed. We may also
  // want a counter to account for notifications. Probably, to avoid race
  // conditions it would be best to restructure the server in an async task for
  // a 2.0 release. In such task, shutting down would mean to wait for the
  // request counter to drop to zero (thus don't block task, only prevent new
  // requests), then shutting down the task.

  // Fetch handler
  aos_jrpc_server_handler_t handler = _aos_jrpc_server_handler_get(
      server, cJSON_GetObjectItemCaseSensitive(request, "method")->valuestring);
  if (!handler) {
    args->out_response = aos_jrpc_message_error(id, -32601, "Method not found");
    goto _aos_jrpc_server_request_handle_err;
  }

  // Alloc context
  ctx = calloc(1, sizeof(_aos_jrpc_server_request_handle_ctx_t));
  if (!ctx) {
    args->out_response = aos_jrpc_message_error(id, -32603, "Internal error");
    goto _aos_jrpc_server_request_handle_err;
  }
  ctx->future = future;
  ctx->id = id;
  ctx->server = server;

  // Alloc future
  aos_future_config_t handler_future_config = {
      .cb = _aos_jrpc_server_request_handle_cb, .ctx = ctx};
  aos_future_t *handler_future = AOS_FUTURE_ALLOC_T(aos_jrpc_server_handler)(
      &handler_future_config, NULL, 0);
  if (!handler_future) {
    args->out_response = aos_jrpc_message_error(id, -32603, "Internal error");
    goto _aos_jrpc_server_request_handle_err;
  }

  // Launch handler
  handler(cJSON_GetObjectItemCaseSensitive(request, "params"), handler_future);
  return;

_aos_jrpc_server_request_handle_err:
  free(ctx);
  cJSON_Delete(id);
  if (!args->out_response) {
    args->out_err = 1;
  }
  xSemaphoreTakeRecursive(server->semaphore, portMAX_DELAY);
  server->counter--;
  xSemaphoreGiveRecursive(server->semaphore);
  aos_resolve(future);
}
static void _aos_jrpc_server_request_handle_cb(aos_future_t *future) {
  AOS_ARGS_T(aos_jrpc_server_handler) *args = aos_args_get(future);
  unsigned int out_err = args->out_err;
  cJSON *out_result = args->out_result;
  _aos_jrpc_server_request_handle_ctx_t *ctx = aos_future_free(future);

  // Disassemble ctx for convenience
  cJSON *id = ctx->id;
  aos_jrpc_server_t *server = ctx->server;
  aos_future_t *call_future = ctx->future;
  free(ctx);

  AOS_ARGS_T(aos_jrpc_server_call_json) *call_args = aos_args_get(call_future);

  /* Check return value */
  switch (out_err) {
  case 0: {
    // All good, is it a notification?
    if (!id) {
      cJSON_Delete(id);
      cJSON_Delete(out_result);
      xSemaphoreTakeRecursive(server->semaphore, portMAX_DELAY);
      server->counter--;
      xSemaphoreGiveRecursive(server->semaphore);
      aos_resolve(call_future);
      return;
    }

    // Response required, let's build it
    call_args->out_response = aos_jrpc_message_result(id, out_result);
    if (!call_args->out_response) {
      // Create an error response
      call_args->out_response =
          aos_jrpc_message_error(id, -32603, "Internal error");
    }
    goto _aos_jrpc_server_request_handle_cb_end;
  }
  case AOS_JRPC_SERVER_ERR_INVALIDPARAMS: {
    // Create an error response
    call_args->out_response =
        aos_jrpc_message_error(id, -32602, "Invalid params");
    goto _aos_jrpc_server_request_handle_cb_end;
  }
  default: {
    // Create an error response
    call_args->out_response =
        aos_jrpc_message_error(id, -32603, "Internal error");
    goto _aos_jrpc_server_request_handle_cb_end;
  }
  }

_aos_jrpc_server_request_handle_cb_end:
  if (!call_args->out_response) {
    call_args->out_err = 1;
  }
  cJSON_Delete(id);
  cJSON_Delete(out_result);
  xSemaphoreTakeRecursive(server->semaphore, portMAX_DELAY);
  server->counter--;
  xSemaphoreGiveRecursive(server->semaphore);
  aos_resolve(call_future);
  return;
}

/**
 * Sequential batch
 */
typedef struct _aos_jrpc_server_batch_handle_sequential_ctx_t {
  size_t counter;
  aos_jrpc_server_t *server;
  cJSON *request;
  aos_future_t *future;
} _aos_jrpc_server_batch_handle_sequential_ctx_t;
static void _aos_jrpc_server_batch_handle_sequential(aos_jrpc_server_t *server,
                                                     cJSON *request,
                                                     aos_future_t *future) {
  AOS_ARGS_T(aos_jrpc_server_call_json) *args = aos_args_get(future);
  _aos_jrpc_server_batch_handle_sequential_ctx_t *ctx = NULL;
  cJSON *request_dup = NULL;

  // Check for invalid arrays
  if (!cJSON_GetArraySize(request)) {
    args->out_response =
        aos_jrpc_message_error(NULL, -32600, "Invalid Request");
    goto _aos_jrpc_server_batch_handle_sequential_err;
  }

  // Allocate context
  ctx = calloc(1, sizeof(_aos_jrpc_server_batch_handle_sequential_ctx_t));
  request_dup = cJSON_Duplicate(request, true);
  if (!ctx || !request_dup) {
    goto _aos_jrpc_server_batch_handle_sequential_err;
  }
  ctx->server = server;
  ctx->future = future;
  ctx->request = request_dup;
  ctx->counter = cJSON_GetArraySize(request);

  // Start first processing
  aos_future_config_t config = {
      .cb = _aos_jrpc_server_batch_handle_sequential_cb, .ctx = ctx};
  aos_future_t *req_future =
      AOS_FUTURE_ALLOC_T(aos_jrpc_server_call_json)(&config, NULL, 0);
  if (!req_future) {
    args->out_response = aos_jrpc_message_error(NULL, -32603, "Internal error");
    goto _aos_jrpc_server_batch_handle_sequential_err;
  }
  cJSON *item = cJSON_DetachItemFromArray(request_dup, 0);
  _aos_jrpc_server_request_handle(server, item, req_future);
  cJSON_Delete(item);
  return;

_aos_jrpc_server_batch_handle_sequential_err:
  free(ctx);
  if (!args->out_response) {
    args->out_err = 1;
  }
  aos_resolve(future);
}

static void _aos_jrpc_server_batch_handle_sequential_cb(aos_future_t *future) {
  AOS_ARGS_T(aos_jrpc_server_call_json) *args = aos_args_get(future);
  cJSON *out_response = args->out_response;
  unsigned int out_err = args->out_err;

  // Disassemble ctx for convenience
  _aos_jrpc_server_batch_handle_sequential_ctx_t *ctx = aos_future_free(future);
  aos_jrpc_server_t *server = ctx->server;
  cJSON *request = ctx->request;
  aos_future_t *call_future = ctx->future;

  AOS_ARGS_T(aos_jrpc_server_call_json) *call_args = aos_args_get(call_future);
  cJSON *err_response = NULL;

  // Decrease counter
  ctx->counter--;

  // Check if response processing was alright
  if (out_err) {
    // Not alright, create an error response for the whole batch
    err_response = aos_jrpc_message_error(NULL, -32603, "Internal error");
    goto _aos_jrpc_server_batch_handle_sequential_cb_err;
  }

  // Append response only if not a notification
  if (out_response) {
    // Do we still need to allocate the response array?
    if (!call_args->out_response) {
      // This is the first single response we receive, allocate batch response
      // array
      call_args->out_response = cJSON_CreateArray();
      if (!call_args->out_response) {
        // Allocation failed, create an error response for the whole batch
        err_response = aos_jrpc_message_error(NULL, -32603, "Internal error");
        goto _aos_jrpc_server_batch_handle_sequential_cb_err;
      }
    }

    // Add response (or error) to batch response array (only if not a
    // notification)
    if (!cJSON_AddItemToArray(call_args->out_response, out_response)) {
      // Operation failed, create an error response for the whole batch
      err_response = aos_jrpc_message_error(NULL, -32603, "Internal error");
      goto _aos_jrpc_server_batch_handle_sequential_cb_err;
    }
  }

  // Did we process all requests?
  if (ctx->counter == 0) {
    // Yes, free ctx then callback
    cJSON_Delete(request);
    free(ctx);
    aos_resolve(call_future);
    return;
  }

  // Some requests left, move on to the next one
  aos_future_config_t config = {
      .cb = _aos_jrpc_server_batch_handle_sequential_cb, .ctx = ctx};
  aos_future_t *new_future =
      AOS_FUTURE_ALLOC_T(aos_jrpc_server_call_json)(&config, NULL, 0);
  if (!new_future) {
    // Create an error response for the whole batch
    err_response = aos_jrpc_message_error(NULL, -32603, "Internal error");
    goto _aos_jrpc_server_batch_handle_sequential_cb_err;
  }
  cJSON *item = cJSON_DetachItemFromArray(request, 0);
  _aos_jrpc_server_request_handle(server, item, new_future);
  cJSON_Delete(item);
  return;

_aos_jrpc_server_batch_handle_sequential_cb_err:
  cJSON_Delete(out_response);
  free(ctx);
  cJSON_Delete(
      call_args->out_response); // Cleanup incomplete batch response if any
  call_args->out_response = err_response;
  if (!call_args->out_response) {
    call_args->out_err = 1;
  }
  aos_resolve(call_future);
}

/**
 * Parallel batch
 */
typedef struct _aos_jrpc_server_batch_handle_parallel_ctx_t {
  size_t counter;
  aos_jrpc_server_t *server;
  aos_future_t *future;
  SemaphoreHandle_t semaphore;
  bool fail;
} _aos_jrpc_server_batch_handle_parallel_ctx_t;
static void _aos_jrpc_server_batch_handle_parallel(aos_jrpc_server_t *server,
                                                   cJSON *request,
                                                   aos_future_t *future) {
  AOS_ARGS_T(aos_jrpc_server_call_json) *args = aos_args_get(future);
  SemaphoreHandle_t semaphore = NULL;
  _aos_jrpc_server_batch_handle_parallel_ctx_t *ctx = NULL;
  aos_future_t **parallel_futures = NULL;

  // Check for invalid arrays
  if (!cJSON_GetArraySize(request)) {
    args->out_response =
        aos_jrpc_message_error(NULL, -32600, "Invalid Request");
    goto _aos_jrpc_server_batch_handle_parallel_err;
  }

  // Allocate ctx
  ctx = calloc(1, sizeof(_aos_jrpc_server_batch_handle_parallel_ctx_t));
  semaphore = xSemaphoreCreateRecursiveMutex();
  if (!ctx || !semaphore) {
    args->out_response = aos_jrpc_message_error(NULL, -32603, "Internal error");
    goto _aos_jrpc_server_batch_handle_parallel_err;
  }
  ctx->semaphore = semaphore;
  ctx->future = future;
  ctx->counter = cJSON_GetArraySize(request);

  // Allocate all futures beforehand in a temporary array, we don't want to
  // launch operations if we're not sure we can start all of them
  aos_future_config_t config = {.cb = _aos_jrpc_server_batch_handle_parallel_cb,
                                .ctx = ctx};
  parallel_futures =
      calloc(cJSON_GetArraySize(request), sizeof(aos_future_t *));
  if (!parallel_futures) {
    args->out_response = aos_jrpc_message_error(NULL, -32603, "Internal error");
    goto _aos_jrpc_server_batch_handle_parallel_err;
  }
  for (size_t i = 0; i < cJSON_GetArraySize(request); i++) {
    parallel_futures[i] =
        AOS_FUTURE_ALLOC_T(aos_jrpc_server_call_json)(&config, NULL, 0);
    if (!parallel_futures[i]) {
      args->out_response =
          aos_jrpc_message_error(NULL, -32603, "Internal error");
      goto _aos_jrpc_server_batch_handle_parallel_err;
    }
  }

  // Launch futures
  // NOTE: When the whole processing is single-threaded it is possible that the
  // request gets freed during the last iteration (depends on the function
  // handing the response). In such case the check after the last iteration will
  // fail if we still rely on cJSON_GetArraySize. We thus use a separate
  // variable.
  unsigned int array_size = cJSON_GetArraySize(request);
  for (size_t i = 0; i < array_size; i++) {
    _aos_jrpc_server_request_handle(server, cJSON_GetArrayItem(request, i),
                                    parallel_futures[i]);
  }
  free(parallel_futures);
  return;

_aos_jrpc_server_batch_handle_parallel_err:
  if (parallel_futures) {
    for (size_t i = 0; i < cJSON_GetArraySize(request); i++) {
      if (parallel_futures[i]) {
        aos_future_free(parallel_futures[i]);
      }
    }
  }
  free(parallel_futures);
  free(ctx);
  if (semaphore) {
    vSemaphoreDelete(semaphore);
  }
  if (!args->out_response) {
    args->out_err = 1;
  }
  aos_resolve(future);
}

static void _aos_jrpc_server_batch_handle_parallel_cb(aos_future_t *future) {
  AOS_ARGS_T(aos_jrpc_server_call_json) *args = aos_args_get(future);
  cJSON *out_response = args->out_response;
  unsigned int out_err = args->out_err;

  // Disassemble ctx pointers for convenience
  _aos_jrpc_server_batch_handle_parallel_ctx_t *ctx = aos_future_free(future);
  aos_future_t *call_future = ctx->future;

  AOS_ARGS_T(aos_jrpc_server_call_json) *call_args = aos_args_get(call_future);

  // Lock context to avoid race conditions
  xSemaphoreTakeRecursive(ctx->semaphore, portMAX_DELAY);

  // Decrease counter
  ctx->counter--;

  // Check if response processing was alright or if we failed already
  if (out_err || ctx->fail) {
    // Context failed in previous iterations, don't do anything and wait for all
    // other futures to complete.
    cJSON_Delete(out_response);
    ctx->fail = true;
    goto _aos_jrpc_server_batch_handle_parallel_cb_end;
  }

  // If not a notification, add response to array
  if (out_response) {
    // Do we need to allocate the response array?
    if (!call_args->out_response) {
      // This is the first single response we receive, allocate batch response
      // array
      call_args->out_response = cJSON_CreateArray();
      if (!call_args->out_response) {
        // Allocation failed, set fail flag and cleanup
        cJSON_Delete(out_response);
        ctx->fail = true;
        goto _aos_jrpc_server_batch_handle_parallel_cb_end;
      }
    }

    // Add response (or error) to batch response array
    if (!cJSON_AddItemToArray(call_args->out_response, out_response)) {
      // Operation failed,
      ctx->fail = true;
      cJSON_Delete(out_response);
      goto _aos_jrpc_server_batch_handle_parallel_cb_end;
    }
  }

// Are we done?
_aos_jrpc_server_batch_handle_parallel_cb_end:
  if (ctx->counter == 0) {
    // Did context fail?
    if (ctx->fail) {
      // Cleanup partial response, return a failure
      vSemaphoreDelete(ctx->semaphore);
      free(ctx);
      cJSON_Delete(call_args->out_response);
      call_args->out_response = NULL;
      // Create an error response for the whole batch
      call_args->out_response =
          aos_jrpc_message_error(NULL, -32603, "Internal error");
      if (!call_args->out_response) {
        call_args->out_err = 1;
      }
      aos_resolve(call_future);
      return;
    }
    // All good, complete processing
    vSemaphoreDelete(ctx->semaphore);
    free(ctx);
    aos_resolve(call_future);
    return;
  }

  // Nope, give back semaphore
  xSemaphoreGiveRecursive(ctx->semaphore);
}

/**
 * Handler get/set/unset
 */
static aos_jrpc_server_handler_t
_aos_jrpc_server_handler_get(aos_jrpc_server_t *server, const char *method) {
  for (_aos_jrpc_server_handler_entry_t *entry = server->handlers; entry;
       entry = entry->next) {
    if (!strcmp(entry->method, method)) {
      return entry->handler;
    }
  }
  return NULL;
}

unsigned int aos_jrpc_server_handler_set(aos_jrpc_server_t *server,
                                         aos_jrpc_server_handler_t handler,
                                         const char *method) {
  xSemaphoreTakeRecursive(server->semaphore, portMAX_DELAY);
  _aos_jrpc_server_handler_entry_t **entry = &server->handlers;
  while (*entry) {
    if (!strcmp((*entry)->method, method)) {
      (*entry)->handler = handler;
      xSemaphoreGiveRecursive(server->semaphore);
      return 0;
    }
    entry = &(*entry)->next;
  }
  *entry = calloc(1, sizeof(_aos_jrpc_server_handler_entry_t));
  char *handler_method = strdup(method);
  if (!*entry || !handler_method) {
    free(*entry);
    *entry = NULL;
    free(handler_method);
    xSemaphoreGiveRecursive(server->semaphore);
    return 1;
  }
  (*entry)->method = handler_method;
  (*entry)->handler = handler;
  xSemaphoreGiveRecursive(server->semaphore);
  return 0;
}

unsigned int aos_jrpc_server_handler_unset(aos_jrpc_server_t *server,
                                           const char *method) {
  xSemaphoreTakeRecursive(server->semaphore, portMAX_DELAY);
  _aos_jrpc_server_handler_entry_t **entry = &server->handlers;
  while (*entry) {
    if (!strcmp((*entry)->method, method)) {
      _aos_jrpc_server_handler_entry_t *entry_next = (*entry)->next;
      free((*entry)->method);
      free((*entry));
      *entry = entry_next;
      xSemaphoreGiveRecursive(server->semaphore);
      return 0;
    }
    entry = &(*entry)->next;
  }
  xSemaphoreGiveRecursive(server->semaphore);
  return 1;
}

/**
 * Validator
 */
static bool _aos_jrpc_server_isvalid(cJSON *request) {
  cJSON *jrpc = cJSON_GetObjectItemCaseSensitive(request, "jsonrpc");
  cJSON *id = cJSON_GetObjectItemCaseSensitive(request, "id");
  cJSON *method = cJSON_GetObjectItemCaseSensitive(request, "method");
  if ((!cJSON_IsString(jrpc) || strcmp(jrpc->valuestring, "2.0")) ||
      (id && !(cJSON_IsNumber(id) || cJSON_IsString(id) || cJSON_IsNull(id))) ||
      !cJSON_IsString(method)) {
    return false;
  }
  return true;
}

/**
 * Param getters
 */
unsigned int aos_jrpc_server_param_uint8_get(cJSON *json, unsigned int pos,
                                             const char *name,
                                             unsigned int *param) {
  cJSON *json_param = NULL;
  if (cJSON_IsObject(json))
    json_param = cJSON_GetObjectItemCaseSensitive(json, name);
  else if (cJSON_IsArray(json))
    json_param = cJSON_GetArrayItem(json, pos);
  else
    return 1;

  if (!cJSON_IsNumber(json_param) || (json_param->valuedouble < 0) ||
      (json_param->valuedouble > UINT8_MAX))
    return 2;

  *param = json_param->valuedouble;
  return 0;
}

unsigned int aos_jrpc_server_param_uint16_get(cJSON *json, unsigned int pos,
                                              const char *name,
                                              uint16_t *param) {
  cJSON *json_param = NULL;
  if (cJSON_IsObject(json))
    json_param = cJSON_GetObjectItemCaseSensitive(json, name);
  else if (cJSON_IsArray(json))
    json_param = cJSON_GetArrayItem(json, pos);
  else
    return 1;

  if (!cJSON_IsNumber(json_param) || (json_param->valuedouble < 0) ||
      (json_param->valuedouble > UINT16_MAX))
    return 2;

  *param = json_param->valuedouble;
  return 0;
}

unsigned int aos_jrpc_server_param_uint32_get(cJSON *json, unsigned int pos,
                                              const char *name,
                                              uint32_t *param) {
  cJSON *json_param = NULL;
  if (cJSON_IsObject(json))
    json_param = cJSON_GetObjectItemCaseSensitive(json, name);
  else if (cJSON_IsArray(json))
    json_param = cJSON_GetArrayItem(json, pos);
  else
    return 1;

  if (!cJSON_IsNumber(json_param) || (json_param->valuedouble < 0) ||
      (json_param->valuedouble > UINT32_MAX))
    return 2;

  *param = json_param->valuedouble;
  return 0;
}

unsigned int aos_jrpc_server_param_uint64_get(cJSON *json, unsigned int pos,
                                              const char *name,
                                              uint64_t *param) {
  cJSON *json_param = NULL;
  if (cJSON_IsObject(json))
    json_param = cJSON_GetObjectItemCaseSensitive(json, name);
  else if (cJSON_IsArray(json))
    json_param = cJSON_GetArrayItem(json, pos);
  else
    return 1;

  if (!cJSON_IsNumber(json_param) || (json_param->valuedouble < 0) ||
      (json_param->valuedouble > UINT64_MAX))
    return 2;

  *param = json_param->valuedouble;
  return 0;
}

unsigned int aos_jrpc_server_param_int8_get(cJSON *json, unsigned int pos,
                                            const char *name, int8_t *param) {
  cJSON *json_param = NULL;
  if (cJSON_IsObject(json))
    json_param = cJSON_GetObjectItemCaseSensitive(json, name);
  else if (cJSON_IsArray(json))
    json_param = cJSON_GetArrayItem(json, pos);
  else
    return 1;

  if (!cJSON_IsNumber(json_param) || (json_param->valuedouble < INT8_MIN) ||
      (json_param->valuedouble > INT8_MAX))
    return 2;

  *param = json_param->valuedouble;
  return 0;
}

unsigned int aos_jrpc_server_param_int16_get(cJSON *json, unsigned int pos,
                                             const char *name, int16_t *param) {
  cJSON *json_param = NULL;
  if (cJSON_IsObject(json))
    json_param = cJSON_GetObjectItemCaseSensitive(json, name);
  else if (cJSON_IsArray(json))
    json_param = cJSON_GetArrayItem(json, pos);
  else
    return 1;

  if (!cJSON_IsNumber(json_param) || (json_param->valuedouble < INT16_MIN) ||
      (json_param->valuedouble > INT16_MAX))
    return 2;

  *param = json_param->valuedouble;
  return 0;
}

unsigned int aos_jrpc_server_param_int32_get(cJSON *json, unsigned int pos,
                                             const char *name, int32_t *param) {
  cJSON *json_param = NULL;
  if (cJSON_IsObject(json))
    json_param = cJSON_GetObjectItemCaseSensitive(json, name);
  else if (cJSON_IsArray(json))
    json_param = cJSON_GetArrayItem(json, pos);
  else
    return 1;

  if (!cJSON_IsNumber(json_param) || (json_param->valuedouble < INT32_MIN) ||
      (json_param->valuedouble > INT32_MAX))
    return 2;

  *param = json_param->valuedouble;
  return 0;
}

unsigned int aos_jrpc_server_param_int64_get(cJSON *json, unsigned int pos,
                                             const char *name, int64_t *param) {
  cJSON *json_param = NULL;
  if (cJSON_IsObject(json))
    json_param = cJSON_GetObjectItemCaseSensitive(json, name);
  else if (cJSON_IsArray(json))
    json_param = cJSON_GetArrayItem(json, pos);
  else
    return 1;

  if (!cJSON_IsNumber(json_param) || (json_param->valuedouble < INT64_MIN) ||
      (json_param->valuedouble > INT64_MAX))
    return 2;

  *param = json_param->valuedouble;
  return 0;
}

unsigned int aos_jrpc_server_param_float_get(cJSON *json, unsigned int pos,
                                             const char *name, float *param) {
  cJSON *json_param = NULL;
  if (cJSON_IsObject(json))
    json_param = cJSON_GetObjectItemCaseSensitive(json, name);
  else if (cJSON_IsArray(json))
    json_param = cJSON_GetArrayItem(json, pos);
  else
    return 1;

  if (!cJSON_IsNumber(json_param) || !isfinite((float)json_param->valuedouble))
    return 2;

  *param = json_param->valuedouble;
  return 0;
}

unsigned int aos_jrpc_server_param_double_get(cJSON *json, unsigned int pos,
                                              const char *name, double *param) {
  cJSON *json_param = NULL;
  if (cJSON_IsObject(json))
    json_param = cJSON_GetObjectItemCaseSensitive(json, name);
  else if (cJSON_IsArray(json))
    json_param = cJSON_GetArrayItem(json, pos);
  else
    return 1;

  if (!cJSON_IsNumber(json_param) || !isfinite((double)json_param->valuedouble))
    return 2;

  *param = json_param->valuedouble;
  return 0;
}

unsigned int aos_jrpc_server_param_str_get(cJSON *json, unsigned int pos,
                                           const char *name, char **param) {
  cJSON *json_param = NULL;
  if (cJSON_IsObject(json))
    json_param = cJSON_GetObjectItemCaseSensitive(json, name);
  else if (cJSON_IsArray(json))
    json_param = cJSON_GetArrayItem(json, pos);
  else
    return 1;

  if (!cJSON_IsString(json_param))
    return 2;

  *param = json_param->valuestring;
  return 0;
}

unsigned int aos_jrpc_server_param_bool_get(cJSON *json, unsigned int pos,
                                            const char *name, bool *param) {
  cJSON *json_param = NULL;
  if (cJSON_IsObject(json))
    json_param = cJSON_GetObjectItemCaseSensitive(json, name);
  else if (cJSON_IsArray(json))
    json_param = cJSON_GetArrayItem(json, pos);
  else
    return 1;

  if (!cJSON_IsBool(json_param))
    return 2;

  *param = (cJSON_IsTrue(json_param) ? true : false);
  return 0;
}

unsigned int aos_jrpc_server_param_array_get(cJSON *json, unsigned int pos,
                                             const char *name, cJSON **param) {
  cJSON *json_param = NULL;
  if (cJSON_IsObject(json))
    json_param = cJSON_GetObjectItemCaseSensitive(json, name);
  else if (cJSON_IsArray(json))
    json_param = cJSON_GetArrayItem(json, pos);
  else
    return 1;

  if (!cJSON_IsArray(json_param))
    return 2;

  *param = json_param;
  return 0;
}

unsigned int aos_jrpc_server_param_object_get(cJSON *json, unsigned int pos,
                                              const char *name, cJSON **param) {
  cJSON *json_param = NULL;
  if (cJSON_IsObject(json))
    json_param = cJSON_GetObjectItemCaseSensitive(json, name);
  else if (cJSON_IsArray(json))
    json_param = cJSON_GetArrayItem(json, pos);
  else
    return 1;

  if (!cJSON_IsObject(json_param))
    return 2;

  *param = json_param;
  return 0;
}
