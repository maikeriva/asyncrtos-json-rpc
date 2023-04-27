/**
 * @file aos_jrpc_client.h
 * @author Michele Riva (michele.riva@pm.me)
 * @brief AsyncRTOS JSON-RPC client API
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
#include <aos.h>
#include <cJSON.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief JSON-RPC client instance
 */
typedef struct _aos_jrpc_client_t aos_jrpc_client_t;

/**
 * @brief JSON-RPC client configuration
 */
typedef struct aos_jrpc_client_config_t {
  size_t maxrequests; // Maximum number of parallel requests
  size_t maxinputlen; // Maximum input lenght
  unsigned int (*on_output)(const char *data); // Output function
} aos_jrpc_client_config_t;

/**
 * @brief Allcoate a new JSON-RPC client instance
 *
 * @param config Client configuration
 * @return aos_jrpc_client_t* Client instance
 */
aos_jrpc_client_t *aos_jrpc_client_alloc(aos_jrpc_client_config_t *config);

/**
 * @brief Free a JSON-RPC client instance
 * This function will fail and return 1 if there are some active timeouts left.
 * This applies even if a request received a timely response, as timeouts still
 * need to expire to clean their resources. For this reason, don't use overly
 * long timeouts in aos_jrpc_client_request_send.
 *
 * @param client Client instance
 * @return unsigned int 0 if freed succesfully, 1 otherwise
 */
unsigned int aos_jrpc_client_free(aos_jrpc_client_t *client);

/**
 * @brief Client input function (text)
 * Input data in text format such as responses are ingested through this
 * function.
 *
 * @param client Client instance
 * @param data Input data
 * @return unsigned int
 * 0 if data is processed correctly.
 * 1 if data is longer than maxinputlen.
 * 2 if data could not be parsed.
 * 3 if data is not a valid JSON-RPC response.
 * 4 if parsed response does not have a corresponding request.
 */
unsigned int aos_jrpc_client_read(aos_jrpc_client_t *client, const char *data);

/**
 * @brief Client input function (cJSON)
 * Input data in cJSON format such as responses are ingested through this
 * function.
 *
 * @param client Client instance
 * @param data Input data
 * @return unsigned int
 * 0 if data is processed correctly.
 * 3 if data is not a valid JSON-RPC response.
 * 4 if parsed response does not have a corresponding request.
 */
unsigned int aos_jrpc_client_read_json(aos_jrpc_client_t *client, cJSON *json);

/**
 * @brief Client error codes
 */
typedef enum {
  AOS_JRPC_CLIENT_ERR_OK = 0,          // No error
  AOS_JRPC_CLIENT_ERR_CLIENTERROR,     // Client-side error
  AOS_JRPC_CLIENT_ERR_SERVERERROR,     // Server-side error
  AOS_JRPC_CLIENT_ERR_TIMEOUT,         // Request timed out
  AOS_JRPC_CLIENT_ERR_TOOMANYREQUESTS, // Too many requests in progress
} aos_jrpc_client_err_t;

AOS_DECLARE(aos_jrpc_client_request_send, char *out_result,
            aos_jrpc_client_err_t out_err)
/**
 * @brief Send a JSON-RPC request (text)
 * Sends a JSON-RPC request with the parameters in text format.
 * Parameters are passed by copy.
 *
 * @param client Client instance
 * @param timeout_ms Request timeout in ms
 * @param method Request method
 * @param params Request parameters
 * @param future Future
 * @param out_result (in future) Either the result or error field of the
 * JSON-RPC response in text format, if any
 * @param out_err A aos_jrpc_client_err_t error state
 */
void aos_jrpc_client_request_send(aos_jrpc_client_t *client,
                                  unsigned int timeout_ms, const char *method,
                                  const char *params, aos_future_t *future);

AOS_DECLARE(aos_jrpc_client_request_send_json, cJSON *out_result,
            aos_jrpc_client_err_t out_err)
/**
 * @brief Send a JSON-RPC request (cJSON)
 * Sends a JSON-RPC request with the parameters in cJSON format.
 * Parameters are passed by copy.
 *
 * @param client Client instance
 * @param timeout_ms Request timeout in ms
 * @param method Request method
 * @param params Request parameters
 * @param future Future
 * @param out_result (in future) Either the result or error field of the
 * JSON-RPC response in cJSON format, if any
 * @param out_err A aos_jrpc_client_err_t error state
 */
void aos_jrpc_client_request_send_json(aos_jrpc_client_t *client,
                                       unsigned int timeout_ms,
                                       const char *method, cJSON *params,
                                       aos_future_t *future);

/**
 * @brief Send a JSON-RPC notification (text)
 * Sends a JSON-RPC notification with the parameters in text format.
 * Parameters are passed by copy.
 *
 * @param client Client instance
 * @param method Notification method
 * @param params Notification parameters
 * @return unsigned int
 * 0 if sent correctly
 * 1 if could not be sent
 */
unsigned int aos_jrpc_client_notification_send(aos_jrpc_client_t *client,
                                               const char *method,
                                               const char *params);

/**
 * @brief Send a JSON-RPC notification (cJSON)
 * Sends a JSON-RPC notification with the parameters in cJSON format.
 * Parameters are passed by copy.
 *
 * @param client Client instance
 * @param method Notification method
 * @param params Notification parameters
 * @return unsigned int
 * 0 if sent correctly
 * 1 if could not be sent
 */
unsigned int aos_jrpc_client_notification_send_json(aos_jrpc_client_t *client,
                                                    const char *method,
                                                    cJSON *params);

#ifdef __cplusplus
}
#endif