/**
 * @file aos_jrpc_peer.c
 * @author Michele Riva (michele.riva@pm.me)
 * @brief AsyncRTOS JSON-RPC peer implementation
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
#include <aos_jrpc_peer.h>
#include <aos_jrpc_server.h>
#include <sdkconfig.h>
#include <string.h>
#if CONFIG_AOS_JRPC_PEER_LOG_NONE
#define LOG_LOCAL_LEVEL ESP_LOG_NONE
#elif CONFIG_AOS_JRPC_PEER_LOG_ERROR
#define LOG_LOCAL_LEVEL ESP_LOG_ERROR
#elif CONFIG_AOS_JRPC_PEER_LOG_WARN
#define LOG_LOCAL_LEVEL ESP_LOG_WARN
#elif CONFIG_AOS_JRPC_PEER_LOG_INFO
#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#elif CONFIG_AOS_JRPC_PEER_LOG_DEBUG
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#elif CONFIG_AOS_JRPC_PEER_LOG_VERBOSE
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#endif
#include <esp_log.h>

static const char *_tag = "AOS JSON-RPC peer";

aos_jrpc_peer_t *aos_jrpc_peer_alloc(aos_jrpc_peer_config_t *config) {
  aos_jrpc_peer_t *peer = NULL;
  aos_jrpc_server_t *server = NULL;
  aos_jrpc_client_t *client = NULL;

  // Verify config
  if (!config->on_error || !config->on_output) {
    ESP_LOGE(_tag, "Incomplete configuration (on_error:%u on_output:%u)",
             config->on_error != NULL, config->on_output != NULL);
    goto aos_jrpc_peer_alloc_err;
  }

  // Build config
  aos_jrpc_peer_config_t complete_config = {
      .maxclientrequests = config->maxclientrequests
                               ? config->maxclientrequests
                               : CONFIG_AOS_JRPC_PEER_MAXCLIENTREQUESTS,
      .maxserverrequests = config->maxserverrequests
                               ? config->maxserverrequests
                               : CONFIG_AOS_JRPC_PEER_MAXSERVERREQUESTS,
      .maxinputlen = config->maxinputlen ? config->maxinputlen
                                         : CONFIG_AOS_JRPC_PEER_MAXINPUTLEN,
      .parallel = config->parallel,
      .on_error = config->on_error,
      .on_output = config->on_output};

  // Allocate resources
  peer = calloc(1, sizeof(aos_jrpc_peer_t));
  aos_jrpc_server_config_t server_config = {
      .maxrequests = complete_config.maxserverrequests,
      .parallel = complete_config.parallel};
  server = aos_jrpc_server_alloc(&server_config);
  aos_jrpc_client_config_t client_config = {
      .on_output = complete_config.on_output,
      .maxrequests = complete_config.maxclientrequests};
  client = aos_jrpc_client_alloc(&client_config);
  if (!peer || !server || !client) {
    goto aos_jrpc_peer_alloc_err;
  }
  peer->server = server;
  peer->client = client;
  peer->config = complete_config;

  return peer;

aos_jrpc_peer_alloc_err:
  free(peer);
  aos_jrpc_server_free(server);
  aos_jrpc_client_free(client);
  return NULL;
}

unsigned int aos_jrpc_peer_free(aos_jrpc_peer_t *peer) {
  unsigned int ret = aos_jrpc_client_free(peer->client);
  if (ret) {
    return ret;
  }
  aos_jrpc_server_free(peer->server);
  free(peer);
  return 0;
}

unsigned int aos_jrpc_peer_read(aos_jrpc_peer_t *peer, const char *data) {
  cJSON *error = NULL;
  char *error_str = NULL;
  if (strlen(data) > peer->config.maxinputlen) {
    error = aos_jrpc_message_error(NULL, -32000, "Server error");
    goto aos_jrpc_peer_read_err;
  }

  cJSON *json = cJSON_Parse(data);
  if (!json) {
    error = aos_jrpc_message_error(NULL, -32700, "Parse error");
  }

  unsigned int ret = aos_jrpc_peer_read_json(peer, json);
  cJSON_Delete(json);
  return ret;

aos_jrpc_peer_read_err:
  error_str = cJSON_PrintUnformatted(error);
  if (!error || !error_str) {
    free(error_str);
    cJSON_Delete(error);
    return 1;
  }
  peer->config.on_output(error_str);
  free(error_str);
  cJSON_Delete(error);
  return 0;
}

static void _aos_jrpc_peer_server_call_json_cb(aos_future_t *future) {
  AOS_ARGS_T(aos_jrpc_server_call_json) *args = aos_args_get(future);
  cJSON *response = args->out_response;
  unsigned int ret = args->out_err;
  aos_jrpc_peer_t *peer = aos_future_free(future);

  if (ret) {
    if (peer->config.on_error) {
      peer->config.on_error(ret);
    }
    cJSON_Delete(response);
    return;
  }

  char *response_str = cJSON_PrintUnformatted(response);
  if (!response_str) {
    if (peer->config.on_error) {
      peer->config.on_error(ret);
    }
    cJSON_Delete(response);
    return;
  }

  cJSON_Delete(response);
  peer->config.on_output(response_str);
  free(response_str);
}

unsigned int aos_jrpc_peer_read_json(aos_jrpc_peer_t *peer, cJSON *json) {
  if (cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json, 0), "result") ||
      cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(json, 0), "error") ||
      cJSON_GetObjectItemCaseSensitive(json, "result") ||
      cJSON_GetObjectItemCaseSensitive(json, "error")) {
    return aos_jrpc_client_read_json(peer->client, json);
  } else {
    aos_future_config_t config = {.cb = _aos_jrpc_peer_server_call_json_cb,
                                  .ctx = peer};
    aos_future_t *future =
        AOS_FUTURE_ALLOC_T(aos_jrpc_server_call_json)(&config, NULL, 0);
    if (!future) {
      return 1;
    }
    aos_jrpc_server_call_json(peer->server, json, future);
    return 0;
  }
}
