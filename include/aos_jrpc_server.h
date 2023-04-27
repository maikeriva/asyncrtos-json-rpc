/**
 * @file aos_jrpc_server.h
 * @author Michele Riva (michele.riva@pm.me)
 * @brief AsyncRTOS JSON-RPC server API
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
 * @brief JSON-RPC server instance
 */
typedef struct _aos_jrpc_server_t aos_jrpc_server_t;

/**
 * @brief JSON-RPC server configuration
 * @param maxrequests Maximum parallel requests
 * @param maxinputlen Maximum input string length
 * @param sequential Enforce sequential processing of batch requests (batch
 * response will follow the same order)
 */
typedef struct aos_jrpc_server_config_t {
  size_t maxrequests;
  size_t maxinputlen;
  bool parallel;
} aos_jrpc_server_config_t;

/**
 * @brief Allocate a new JSON-RPC server instance
 *
 * @param config Configuration
 * @return aos_jrpc_server_t* server instance
 */
aos_jrpc_server_t *aos_jrpc_server_alloc(aos_jrpc_server_config_t *config);

/**
 * @brief Free server instance
 * @warning Due to current limitations it is necessary to manually ensure that
 * all requests have completed processing before freeing the server.
 *
 * @param server Server instance
 */
void aos_jrpc_server_free(aos_jrpc_server_t *server);

AOS_DECLARE(aos_jrpc_server_call_json, cJSON *out_response,
            unsigned int out_err)
/**
 * @brief Call a server function through cJSON request
 *
 * @param server Server instance
 * @param request cJSON request
 * @param future Future
 * @param out_response (on future) cJSON response
 * @param out_err (on future) Output error in case response could not be
 * computed
 */
void aos_jrpc_server_call_json(aos_jrpc_server_t *in_server, cJSON *in_request,
                               aos_future_t *future);

AOS_DECLARE(aos_jrpc_server_call, char *out_data, unsigned int out_err)
/**
 * @brief Call a server function through textual request
 *
 * @param server Server instance
 * @param data Textual request
 * @param future Future
 * @param out_data (on future) Textual response
 * @param out_err (on future) Output error in case response could not be
 * computed
 */
void aos_jrpc_server_call(aos_jrpc_server_t *server, const char *data,
                          aos_future_t *future);

/**
 * @brief Handler error code
 */
typedef enum aos_jrpc_server_err_t {
  AOS_JRPC_SERVER_ERR_OK = 0,        // Handler executed successfully
  AOS_JRPC_SERVER_ERR_INVALIDPARAMS, // Could not execute handler due to invalid
                                     // parameters
} aos_jrpc_server_err_t;

AOS_DECLARE(aos_jrpc_server_handler, cJSON *out_result,
            aos_jrpc_server_err_t out_err)
/**
 * @brief JSON-RPC handler prototype
 * @param params Parameter structure
 * @param future Future
 * @param out_result (on future) cJSON result
 * @param out_err (on future) Error
 * @attention
 * A JSON-RPC handler must behave as follows:
 * 1. Extract relevant parameters from the parameter structure using
 * aos_jrpc_server_param_* functions
 * 2. Call the intended function with the extracted parameters
 * 3. Create a cJSON response object reflecting the function outcome
 * 4. Resolve the future
 * In case the necessary parameters are missing or invalid, you can set the
 * out_err future value to AOS_JRPC_SERVER_ERR_INVALIDPARAMS before resolving
 * the future. This will make the server reply with a standard JSON-RPC "Invalid
 * params" error reponse.
 */
typedef void (*aos_jrpc_server_handler_t)(cJSON *params, aos_future_t *future);

/**
 * @brief Set handler
 *
 * @param server Server instance
 * @param handler Handler
 * @param method Method
 * @return unsigned int 0 if successful, 1 if failed
 */
unsigned int aos_jrpc_server_handler_set(aos_jrpc_server_t *server,
                                         aos_jrpc_server_handler_t handler,
                                         const char *method);

/**
 * @brief Unset handler
 *
 * @param server Server instance
 * @param handler Handler
 * @param method Method
 * @return unsigned int 0 if handler got unset, 1 if handler could not be found
 */
unsigned int aos_jrpc_server_handler_unset(aos_jrpc_server_t *server,
                                           const char *method);

/**
 * @brief Get UINT8 from parameter struct
 *
 * @param json Parameter struct
 * @param pos Expected parameter position (if struct is an array)
 * @param name Expected parameter name (if struct is an object)
 * @param param Pointer to output parameter
 * @return unsigned int 0 if parameter was found, 1 otherwise
 */
unsigned int aos_jrpc_server_param_uint8_get(cJSON *json, unsigned int pos,
                                             const char *name,
                                             unsigned int *param);

/**
 * @brief Get UINT16 from parameter struct
 *
 * @param json Parameter struct
 * @param pos Expected parameter position (if struct is an array)
 * @param name Expected parameter name (if struct is an object)
 * @param param Pointer to output parameter
 * @return unsigned int 0 if parameter was found, 1 otherwise
 */
unsigned int aos_jrpc_server_param_uint16_get(cJSON *json, unsigned int pos,
                                              const char *name,
                                              uint16_t *param);

/**
 * @brief Get UINT32 from parameter struct
 *
 * @param json Parameter struct
 * @param pos Expected parameter position (if struct is an array)
 * @param name Expected parameter name (if struct is an object)
 * @param param Pointer to output parameter
 * @return unsigned int 0 if parameter was found, 1 otherwise
 */
unsigned int aos_jrpc_server_param_uint32_get(cJSON *json, unsigned int pos,
                                              const char *name,
                                              uint32_t *param);

/**
 * @brief Get UINT64 from parameter struct
 *
 * @param json Parameter struct
 * @param pos Expected parameter position (if struct is an array)
 * @param name Expected parameter name (if struct is an object)
 * @param param Pointer to output parameter
 * @return unsigned int 0 if parameter was found, 1 otherwise
 */
unsigned int aos_jrpc_server_param_uint64_get(cJSON *json, unsigned int pos,
                                              const char *name,
                                              uint64_t *param);

/**
 * @brief Get INT8 from parameter struct
 *
 * @param json Parameter struct
 * @param pos Expected parameter position (if struct is an array)
 * @param name Expected parameter name (if struct is an object)
 * @param param Pointer to output parameter
 * @return unsigned int 0 if parameter was found, 1 otherwise
 */
unsigned int aos_jrpc_server_param_int8_get(cJSON *json, unsigned int pos,
                                            const char *name, int8_t *param);

/**
 * @brief Get INT16 from parameter struct
 *
 * @param json Parameter struct
 * @param pos Expected parameter position (if struct is an array)
 * @param name Expected parameter name (if struct is an object)
 * @param param Pointer to output parameter
 * @return unsigned int 0 if parameter was found, 1 otherwise
 */
unsigned int aos_jrpc_server_param_int16_get(cJSON *json, unsigned int pos,
                                             const char *name, int16_t *param);

/**
 * @brief Get INT32 from parameter struct
 *
 * @param json Parameter struct
 * @param pos Expected parameter position (if struct is an array)
 * @param name Expected parameter name (if struct is an object)
 * @param param Pointer to output parameter
 * @return unsigned int 0 if parameter was found, 1 otherwise
 */
unsigned int aos_jrpc_server_param_int32_get(cJSON *json, unsigned int pos,
                                             const char *name, int32_t *param);

/**
 * @brief Get INT64 from parameter struct
 *
 * @param json Parameter struct
 * @param pos Expected parameter position (if struct is an array)
 * @param name Expected parameter name (if struct is an object)
 * @param param Pointer to output parameter
 * @return unsigned int 0 if parameter was found, 1 otherwise
 */
unsigned int aos_jrpc_server_param_int64_get(cJSON *json, unsigned int pos,
                                             const char *name, int64_t *param);

/**
 * @brief Get FLOAT from parameter struct
 *
 * @param json Parameter struct
 * @param pos Expected parameter position (if struct is an array)
 * @param name Expected parameter name (if struct is an object)
 * @param param Pointer to output parameter
 * @return unsigned int 0 if parameter was found, 1 otherwise
 */
unsigned int aos_jrpc_server_param_float_get(cJSON *json, unsigned int pos,
                                             const char *name, float *param);

/**
 * @brief Get DOUBLE from parameter struct
 *
 * @param json Parameter struct
 * @param pos Expected parameter position (if struct is an array)
 * @param name Expected parameter name (if struct is an object)
 * @param param Pointer to output parameter
 * @return unsigned int 0 if parameter was found, 1 otherwise
 */
unsigned int aos_jrpc_server_param_double_get(cJSON *json, unsigned int pos,
                                              const char *name, double *param);

/**
 * @brief Get STRING from parameter struct
 *
 * @attention The ouput string is dynamically allocated and needs to be freed
 * after use
 * @param json Parameter struct
 * @param pos Expected parameter position (if struct is an array)
 * @param name Expected parameter name (if struct is an object)
 * @param param Pointer to output parameter
 * @return unsigned int 0 if parameter was found, 1 otherwise
 */
unsigned int aos_jrpc_server_param_str_get(cJSON *json, unsigned int pos,
                                           const char *name, char **param);

/**
 * @brief Get BOOL from parameter struct
 *
 * @param json Parameter struct
 * @param pos Expected parameter position (if struct is an array)
 * @param name Expected parameter name (if struct is an object)
 * @param param Pointer to output parameter
 * @return unsigned int 0 if parameter was found, 1 otherwise
 */
unsigned int aos_jrpc_server_param_bool_get(cJSON *json, unsigned int pos,
                                            const char *name, bool *param);

/**
 * @brief Get JSON ARRAY from parameter struct
 *
 * @attention The output json array is a reference to the parameter structure
 * and DOES NOT have to be freed
 * @param json Parameter struct
 * @param pos Expected parameter position (if struct is an array)
 * @param name Expected parameter name (if struct is an object)
 * @param param Pointer to output parameter
 * @return unsigned int 0 if parameter was found, 1 otherwise
 */
unsigned int aos_jrpc_server_param_array_get(cJSON *json, unsigned int pos,
                                             const char *name, cJSON **param);

/**
 * @brief Get JSON OBJECT from parameter struct
 *
 * @attention The output json object is a reference to the parameter structure
 * and DOES NOT have to be freed
 * @param json Parameter struct
 * @param pos Expected parameter position (if struct is an array)
 * @param name Expected parameter name (if struct is an object)
 * @param param Pointer to output parameter
 * @return unsigned int 0 if parameter was found, 1 otherwise
 */
unsigned int aos_jrpc_server_param_object_get(cJSON *json, unsigned int pos,
                                              const char *name, cJSON **param);

#ifdef __cplusplus
}
#endif