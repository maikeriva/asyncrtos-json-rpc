/**
 * @file aos_jrpc_client.c
 * @author Michele Riva (michele.riva@pm.me)
 * @brief AsyncRTOS JSON-RPC client implementation
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
#include <aos_jrpc_client.h>
#include <aos_jrpc_message.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <sdkconfig.h>
#include <string.h>
#if CONFIG_AOS_JRPC_CLIENT_LOG_NONE
#define LOG_LOCAL_LEVEL ESP_LOG_NONE
#elif CONFIG_AOS_JRPC_CLIENT_LOG_ERROR
#define LOG_LOCAL_LEVEL ESP_LOG_ERROR
#elif CONFIG_AOS_JRPC_CLIENT_LOG_WARN
#define LOG_LOCAL_LEVEL ESP_LOG_WARN
#elif CONFIG_AOS_JRPC_CLIENT_LOG_INFO
#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#elif CONFIG_AOS_JRPC_CLIENT_LOG_DEBUG
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#elif CONFIG_AOS_JRPC_CLIENT_LOG_VERBOSE
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#endif
#include <esp_log.h>

typedef struct _aos_jrpc_client_request_entry_t aos_jrpc_client_request_entry_t;
struct _aos_jrpc_client_request_entry_t {
  aos_future_t *future;
  uint32_t id;
  esp_timer_handle_t timer;
  aos_jrpc_client_request_entry_t *next;
};

struct _aos_jrpc_client_t {
  aos_jrpc_client_request_entry_t *requests;
  SemaphoreHandle_t semaphore;
  aos_jrpc_client_config_t config;
};

typedef struct _aos_jrpc_client_timer_args_t {
  aos_jrpc_client_t *client;
  uint32_t id;
} aos_jrpc_client_timer_args_t;

static void aos_jrpc_client_request_send_cb(aos_future_t *future);
static bool _aos_jrpc_client_isvalid(cJSON *response);
static void _aos_jrpc_client_timeout_cb(void *args);

static const char *_tag = "AOS JSON-RPC client";

aos_jrpc_client_t *aos_jrpc_client_alloc(aos_jrpc_client_config_t *config) {
  aos_jrpc_client_t *client = NULL;
  SemaphoreHandle_t semaphore = NULL;

  // Verify config
  if (!config->on_output) {
    ESP_LOGE(_tag, "Incomplete configuration (on_output:%u)",
             config->on_output != NULL);
    goto aos_jrpc_client_alloc_err;
  }

  // Build config
  aos_jrpc_client_config_t complete_config = {
      .maxrequests = config->maxrequests ? config->maxrequests
                                         : CONFIG_AOS_JRPC_CLIENT_MAXREQUESTS,
      .maxinputlen = config->maxinputlen ? config->maxinputlen
                                         : CONFIG_AOS_JRPC_CLIENT_MAXINPUTLEN,
      .on_output = config->on_output};

  client = calloc(1, sizeof(aos_jrpc_client_t));
  semaphore = xSemaphoreCreateRecursiveMutex();
  if (!client || !semaphore) {
    goto aos_jrpc_client_alloc_err;
  }
  client->semaphore = semaphore;
  client->config = complete_config;
  return client;

aos_jrpc_client_alloc_err:
  free(client);
  if (semaphore) {
    vSemaphoreDelete(semaphore);
  }
  return NULL;
}

unsigned int aos_jrpc_client_free(aos_jrpc_client_t *client) {
  // Client can be freed only if all timeouts have expired
  if (client->requests) {
    return 1;
  }
  vSemaphoreDelete(client->semaphore);
  free(client);
  return 0;
}

AOS_DEFINE(aos_jrpc_client_request_send, char *, aos_jrpc_client_err_t)
void aos_jrpc_client_request_send(aos_jrpc_client_t *client,
                                  unsigned int timeout_ms, const char *method,
                                  const char *params, aos_future_t *future) {
  AOS_ARGS_T(aos_jrpc_client_request_send) *args = aos_args_get(future);

  cJSON *json_params = cJSON_Parse(params);
  aos_future_config_t config = {.cb = aos_jrpc_client_request_send_cb,
                                .ctx = future};
  aos_future_t *new_future =
      AOS_FUTURE_ALLOC_T(aos_jrpc_client_request_send_json)(&config, NULL, 0);
  if ((params && !json_params) || !new_future) {
    cJSON_Delete(json_params);
    aos_future_free(new_future);
    args->out_err = AOS_JRPC_CLIENT_ERR_CLIENTERROR;
    aos_resolve(future);
    return;
  }
  aos_jrpc_client_request_send_json(client, timeout_ms, method, json_params,
                                    new_future);
  cJSON_Delete(json_params);
}

static void aos_jrpc_client_request_send_cb(aos_future_t *future) {
  AOS_ARGS_T(aos_jrpc_client_request_send_json) *args = aos_args_get(future);
  cJSON *out_result = args->out_result;
  unsigned int out_err = args->out_err;
  aos_future_t *old_future = aos_future_free(future);
  AOS_ARGS_T(aos_jrpc_client_request_send) *old_args = aos_args_get(old_future);

  old_args->out_err = out_err;
  if (old_args->out_err) {
    aos_resolve(old_future);
    return;
  }

  old_args->out_result = cJSON_PrintUnformatted(out_result);
  if (!old_args->out_result) {
    old_args->out_err = 1;
  }
  cJSON_Delete(out_result);
  aos_resolve(old_future);
}

unsigned int aos_jrpc_client_notification_send(aos_jrpc_client_t *client,
                                               const char *method,
                                               const char *params) {
  unsigned int err = 0;
  cJSON *json_params = cJSON_Parse(params);
  cJSON *notification = aos_jrpc_message_notification(method, json_params);
  char *data = cJSON_PrintUnformatted(notification);
  if (!(params && json_params) || !notification || !data ||
      !client->config.on_output(data)) {
    err = 1;
  }
  free(data);
  cJSON_Delete(notification);
  cJSON_Delete(json_params);
  return err;
}

AOS_DEFINE(aos_jrpc_client_request_send_json, cJSON *, aos_jrpc_client_err_t)
void aos_jrpc_client_request_send_json(aos_jrpc_client_t *client,
                                       unsigned int timeout_ms,
                                       const char *method, cJSON *params,
                                       aos_future_t *future) {
  AOS_ARGS_T(aos_jrpc_client_request_send_json) *args = aos_args_get(future);
  aos_jrpc_client_request_entry_t *new_entry = NULL;
  cJSON *id = NULL;
  cJSON *msg = NULL;
  char *data = NULL;
  aos_jrpc_client_timer_args_t *timer_args = NULL;
  esp_timer_handle_t timer = NULL;

  // Lock context
  xSemaphoreTakeRecursive(client->semaphore, portMAX_DELAY);

  // Check if we are exceeding limits
  size_t requests_count = 0;
  for (aos_jrpc_client_request_entry_t *entry = client->requests; entry;
       entry = entry->next) {
    requests_count++;
  }
  if (requests_count >= client->config.maxrequests) {
    args->out_err = AOS_JRPC_CLIENT_ERR_TOOMANYREQUESTS;
    xSemaphoreGiveRecursive(client->semaphore);
    aos_resolve(future);
    return;
  }

  // Generate a non-conflicting ID
  unsigned int id_num = esp_random();
  for (aos_jrpc_client_request_entry_t *entry = client->requests; entry;
       entry = entry->next) {
    if (entry->id == id_num) {
      id_num = esp_random();
      entry = client->requests;
    }
  }

  // Allocate resources
  new_entry = calloc(1, sizeof(aos_jrpc_client_request_entry_t));
  id = cJSON_CreateNumber(id_num);
  msg = aos_jrpc_message_request(id, method, params);
  data = cJSON_PrintUnformatted(msg);
  timer_args = calloc(1, sizeof(aos_jrpc_client_timer_args_t));
  esp_timer_create_args_t timer_config = {
      .callback = _aos_jrpc_client_timeout_cb, .arg = timer_args};
  if (!new_entry || !id || !msg || !data || !timer_args ||
      ESP_OK != esp_timer_create(&timer_config, &timer)) {
    goto _aos_jrpc_client_request_send_json_err;
  }
  timer_args->client = client;
  timer_args->id = id_num;
  new_entry->id = id_num;
  new_entry->timer = timer;
  new_entry->future = future;

  // Append entry to linked list
  aos_jrpc_client_request_entry_t **entry = &client->requests;
  while (*entry)
    entry = &(*entry)->next;
  *entry = new_entry;

  // Send request
  if (client->config.on_output(data) ||
      ESP_OK != esp_timer_start_once(timer, 1000 * timeout_ms)) {
    goto _aos_jrpc_client_request_send_json_err;
  }

  // Free temporary resources
  free(data);
  cJSON_Delete(msg);
  cJSON_Delete(id);

  // Unclock ctx
  xSemaphoreGiveRecursive(client->semaphore);
  return;

_aos_jrpc_client_request_send_json_err:
  esp_timer_delete(timer); // Passing NULL is ok, but it won't return ESP_OK
  free(timer_args);
  free(data);
  cJSON_Delete(msg);
  cJSON_Delete(id);
  free(new_entry);
  xSemaphoreGiveRecursive(client->semaphore);
  args->out_err = AOS_JRPC_CLIENT_ERR_CLIENTERROR;
  aos_resolve(future);
  return;
}

unsigned int aos_jrpc_client_notification_send_json(aos_jrpc_client_t *client,
                                                    const char *method,
                                                    cJSON *params) {
  unsigned int err = 0;
  cJSON *notification = aos_jrpc_message_notification(method, params);
  char *data = cJSON_PrintUnformatted(notification);
  if (!notification || !data || !client->config.on_output(data)) {
    err = 1;
  }
  free(data);
  cJSON_Delete(notification);
  return err;
}

unsigned int aos_jrpc_client_read(aos_jrpc_client_t *client, const char *data) {
  if (strlen(data) > client->config.maxinputlen) {
    return 1;
  }

  cJSON *json = cJSON_Parse(data);
  if (!json) {
    return 2;
  }

  unsigned int ret = aos_jrpc_client_read_json(client, json);
  cJSON_Delete(json);
  return ret;
}

unsigned int aos_jrpc_client_read_json(aos_jrpc_client_t *client, cJSON *json) {
  if (!_aos_jrpc_client_isvalid(json)) {
    return 3;
  }

  xSemaphoreTakeRecursive(client->semaphore, portMAX_DELAY);
  cJSON *id = cJSON_GetObjectItemCaseSensitive(json, "id");
  for (aos_jrpc_client_request_entry_t *entry = client->requests; entry;
       entry = entry->next) {
    // We also check that the future has not been resolved yet, or else we are
    // vulnerable to double-response attacks
    if (id->valuedouble == entry->id && entry->future) {
      // Retrieve future
      aos_future_t *future = entry->future;
      AOS_ARGS_T(aos_jrpc_client_request_send_json) *args =
          aos_args_get(future);

      // Try picking up the error if present, else the result
      if (cJSON_GetObjectItemCaseSensitive(json, "error")) {
        args->out_result = cJSON_Duplicate(
            cJSON_GetObjectItemCaseSensitive(json, "error"), true);
        args->out_err = args->out_result ? AOS_JRPC_CLIENT_ERR_SERVERERROR
                                         : AOS_JRPC_CLIENT_ERR_CLIENTERROR;
      } else // We are sure to have a result, as we validated the message
             // beforehand
      {
        args->out_result = cJSON_Duplicate(
            cJSON_GetObjectItemCaseSensitive(json, "result"), true);
        args->out_err = args->out_result ? AOS_JRPC_CLIENT_ERR_OK
                                         : AOS_JRPC_CLIENT_ERR_CLIENTERROR;
      }
      // Resource deallocation is left to the timer
      // We set *future = NULL in the entry so that the timer will know it has
      // been resolved and proceed to deallocate the entry
      entry->future = NULL;
      xSemaphoreGiveRecursive(client->semaphore);
      aos_resolve(future);
      return 0;
    }
  }
  xSemaphoreGiveRecursive(client->semaphore);
  return 4;
}

static bool _aos_jrpc_client_isvalid(cJSON *response) {
  cJSON *jrpc = cJSON_GetObjectItemCaseSensitive(response, "jsonrpc");
  cJSON *id = cJSON_GetObjectItemCaseSensitive(response, "id");
  cJSON *error = cJSON_GetObjectItemCaseSensitive(response, "error");
  cJSON *error_code = cJSON_GetObjectItemCaseSensitive(error, "code");
  cJSON *error_msg = cJSON_GetObjectItemCaseSensitive(error, "message");
  cJSON *result = cJSON_GetObjectItemCaseSensitive(response, "result");
  if ((!cJSON_IsString(jrpc) || strcmp(jrpc->valuestring, "2.0")) ||
      !(cJSON_IsNumber(id) && id->valuedouble >= 0 &&
        id->valuedouble <= UINT32_MAX) ||
      ((result == NULL) == (error == NULL))) {
    return false;
  }
  if (error &&
      !(cJSON_IsNumber(error_code) && error_code->valuedouble <= INT64_MAX &&
        error_code->valuedouble >= INT64_MIN && cJSON_IsString(error_msg))) {
    return false;
  }
  return true;
}

static void _aos_jrpc_client_timeout_cb(void *args) {
  aos_jrpc_client_timer_args_t *timer_args = args;
  aos_jrpc_client_t *client = timer_args->client;
  unsigned int id = timer_args->id;
  free(timer_args);

  // Check if we timed out the request
  xSemaphoreTakeRecursive(client->semaphore, portMAX_DELAY);
  aos_jrpc_client_request_entry_t **entry = &client->requests;
  while (*entry) {
    if ((*entry)->id == id) {
      aos_future_t *future = (*entry)->future;
      ESP_ERROR_CHECK(esp_timer_delete((*entry)->timer));
      aos_jrpc_client_request_entry_t *next_entry = (*entry)->next;
      free(*entry);
      *entry = next_entry;
      if (future) {
        // Not resolved yet, we do it with a timeout
        AOS_ARGS_T(aos_jrpc_client_request_send_json) *future_args =
            aos_args_get(future);
        future_args->out_err = AOS_JRPC_CLIENT_ERR_TIMEOUT;
        aos_resolve(future);
      }
      xSemaphoreGiveRecursive(client->semaphore);
      return;
    }
  }
  // We should ALWAYS have a corresponding ID in the linked list
  assert(NULL);
}
