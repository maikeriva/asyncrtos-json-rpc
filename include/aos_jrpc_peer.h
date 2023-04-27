/**
 * @file aos_jrpc_peer.h
 * @author Michele Riva (michele.riva@pm.me)
 * @brief AsyncRTOS JSON-RPC peer API
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
#pragma once
#include <aos_jrpc_client.h>
#include <aos_jrpc_server.h>
#include <cJSON.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief JSON-RPC peer configuration
 */
typedef struct aos_jrpc_peer_config_t {
  unsigned int (*on_output)(const char *data); // Output function
  void (*on_error)(unsigned int);              // Error function
  size_t maxinputlen;                          // Maximum input length
  size_t maxclientrequests; // Maximum client parallel requests
  size_t maxserverrequests; // Maximum server parallel requests
  bool parallel; // Process batch requests concurrently rather than sequentially
} aos_jrpc_peer_config_t;

/**
 * @brief JSON-RPC peer instance
 */
typedef struct aos_jrpc_peer_t {
  aos_jrpc_server_t *server;
  aos_jrpc_client_t *client;
  aos_jrpc_peer_config_t config;
} aos_jrpc_peer_t;

/**
 * @brief Allocate a JSON-RPC peer instance
 *
 * @param config Peer configuration
 * @return aos_jrpc_peer_t* Peer instance
 */
aos_jrpc_peer_t *aos_jrpc_peer_alloc(aos_jrpc_peer_config_t *config);

/**
 * @brief Free a JSON-RPC peer instance
 * This function will fail and return 1 if there are some active timeouts left.
 * This applies even if a request received a timely response, as timeouts still
 * need to expire to clean their resources. For this reason, don't use overly
 * long timeouts in aos_jrpc_client_request_send.
 *
 * @param peer Peer instance
 * @return unsigned int 0 if success, 1 otherwise
 */
unsigned int aos_jrpc_peer_free(aos_jrpc_peer_t *peer);

/**
 * @brief Peer input function (text)
 * Input data in text format such as responses are ingested through this
 * function.
 *
 * @param peer Peer instance
 * @param data Input data
 * @return unsigned int
 * 0 if data is processed correctly.
 * 1 if data is longer than maxinputlen OR if there are allocation problems
 * (FIXME: split errors) 2 if data could not be parsed. 3 if data is not a valid
 * JSON-RPC payload. 4 if parsed response does not have a corresponding request.
 */
unsigned int aos_jrpc_peer_read(aos_jrpc_peer_t *peer, const char *data);

/**
 * @brief Peer input function (json)
 * Input data in json format such as responses are ingested through this
 * function.
 *
 * @param peer Peer instance
 * @param data Input data
 * @return unsigned int
 * 0 if data is processed correctly.
 * 1 if there are allocation problems
 * 3 if data is not a valid JSON-RPC payload.
 * 4 if parsed response does not have a corresponding request.
 */
unsigned int aos_jrpc_peer_read_json(aos_jrpc_peer_t *peer, cJSON *json);

#ifdef __cplusplus
}
#endif
