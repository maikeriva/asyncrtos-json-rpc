/**
 * @file aos_jrpc_message.c
 * @author Michele Riva (michele.riva@pm.me)
 * @brief AsyncRTOS JSON-RPC message implementation
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
#include <stdbool.h>

cJSON *aos_jrpc_message_error(cJSON *id, int code, const char *msg) {
  cJSON *message = cJSON_CreateObject();
  cJSON *error = cJSON_CreateObject();
  cJSON *id_dup = cJSON_Duplicate(id, true);
  if (!id) {
    id_dup = cJSON_CreateNull();
  }

  if (!message || !error ||
      !cJSON_AddStringToObject(message, "jsonrpc", "2.0") ||
      !cJSON_AddItemToObject(message, "id", id_dup) ||
      !cJSON_AddNumberToObject(error, "code", code) ||
      !cJSON_AddStringToObject(error, "message", msg) ||
      !cJSON_AddItemToObject(message, "error", error)) {
    cJSON_Delete(message);
    cJSON_Delete(error);
    cJSON_Delete(id_dup);
    return NULL;
  }

  return message;
}

cJSON *aos_jrpc_message_result(cJSON *id, cJSON *result) {
  cJSON *message = cJSON_CreateObject();
  cJSON *id_dup = cJSON_Duplicate(id, true);
  cJSON *result_dup = cJSON_Duplicate(result, true);

  if (!message || !id_dup || !result_dup ||
      !cJSON_AddStringToObject(message, "jsonrpc", "2.0") ||
      !cJSON_AddItemToObject(message, "id", id_dup)) {
    cJSON_Delete(message);
    cJSON_Delete(id_dup);
    cJSON_Delete(result_dup);
    return NULL;
  }
  if (!cJSON_AddItemToObject(message, "result", result_dup)) {
    cJSON_Delete(message);
    cJSON_Delete(result_dup);
    return NULL;
  }
  return message;
}

cJSON *aos_jrpc_message_notification(const char *method, cJSON *params) {
  cJSON *message = cJSON_CreateObject();
  cJSON *params_dup = cJSON_Duplicate(params, true);

  if (!message || !cJSON_AddStringToObject(message, "jsonrpc", "2.0") ||
      !cJSON_AddStringToObject(message, "method", method) ||
      (params && !cJSON_AddItemToObject(message, "params", params_dup))) {
    cJSON_Delete(message);
    cJSON_Delete(params_dup);
    return NULL;
  }
  return message;
}

cJSON *aos_jrpc_message_request(cJSON *id, const char *method, cJSON *params) {
  cJSON *message = cJSON_CreateObject();
  cJSON *id_dup = cJSON_Duplicate(id, true);
  cJSON *params_dup = cJSON_Duplicate(params, true);

  if (!message || !id_dup ||
      !cJSON_AddStringToObject(message, "jsonrpc", "2.0") ||
      !cJSON_AddStringToObject(message, "method", method) ||
      !cJSON_AddItemToObject(message, "id", id_dup)) {
    cJSON_Delete(message);
    cJSON_Delete(id_dup);
    cJSON_Delete(params_dup);
    return NULL;
  }
  if ((params && !cJSON_AddItemToObject(message, "params", params_dup))) {
    cJSON_Delete(message);
    cJSON_Delete(params_dup);
    return NULL;
  }
  return message;
}
