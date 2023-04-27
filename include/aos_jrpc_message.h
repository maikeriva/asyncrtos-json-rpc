/**
 * @file aos_jrpc_message.h
 * @author Michele Riva (michele.riva@pm.me)
 * @brief AsyncRTOS JSON-RPC message API
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
#include <cJSON.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Build a JSON-RPC notification message.
 * Parameters are passed by copy.
 *
 * @param method Notification method
 * @param params Notification parameters
 * @return cJSON* Notification message if success, NULL if fail
 */
cJSON *aos_jrpc_message_notification(const char *method, cJSON *params);

/**
 * @brief Build a JSON-RPC request message.
 * Parameters and ID are passed by copy.
 *
 * @param id Request ID
 * @param method Request method
 * @param params Request parameters
 * @return cJSON* Request message if success, NULL if fail
 */
cJSON *aos_jrpc_message_request(cJSON *id, const char *method, cJSON *params);

/**
 * @brief Build a JSON-RPC error message.
 * ID is passed by copy.
 *
 * @param id Request ID
 * @param code Error code
 * @param msg Error message
 * @return cJSON* Error message if success, NULL if fail
 */
cJSON *aos_jrpc_message_error(cJSON *id, int code, const char *msg);

/**
 * @brief Build a JSON-RPC result message.
 * ID and result are passed by copy.
 *
 * @param id Request ID
 * @param result Result
 * @return cJSON* Result message if success, NULL if fail
 */
cJSON *aos_jrpc_message_result(cJSON *id, cJSON *result);

#ifdef __cplusplus
}
#endif
