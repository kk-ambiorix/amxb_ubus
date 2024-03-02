/****************************************************************************
**
** SPDX-License-Identifier: BSD-2-Clause-Patent
**
** SPDX-FileCopyrightText: Copyright (c) 2023 SoftAtHome
**
** Redistribution and use in source and binary forms, with or without modification,
** are permitted provided that the following conditions are met:
**
** 1. Redistributions of source code must retain the above copyright notice,
** this list of conditions and the following disclaimer.
**
** 2. Redistributions in binary form must reproduce the above copyright notice,
** this list of conditions and the following disclaimer in the documentation
** and/or other materials provided with the distribution.
**
** Subject to the terms and conditions of this license, each copyright holder
** and contributor hereby grants to those receiving rights under this license
** a perpetual, worldwide, non-exclusive, no-charge, royalty-free, irrevocable
** (except for failure to satisfy the conditions of this license) patent license
** to make, have made, use, offer to sell, sell, import, and otherwise transfer
** this software, where such license applies only to those patent claims, already
** acquired or hereafter acquired, licensable by such copyright holder or contributor
** that are necessarily infringed by:
**
** (a) their Contribution(s) (the licensed copyrights of copyright holders and
** non-copyrightable additions of contributors, in source or binary form) alone;
** or
**
** (b) combination of their Contribution(s) with the work of authorship to which
** such Contribution(s) was added by such copyright holder or contributor, if,
** at the time the Contribution is added, such addition causes such combination
** to be necessarily infringed. The patent license shall not apply to any other
** combinations which include the Contribution.
**
** Except as expressly stated above, no rights or licenses from any copyright
** holder or contributor is granted under this license, whether expressly, by
** implication, estoppel or otherwise.
**
** DISCLAIMER
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
** AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
** ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE
** LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
** SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
** CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
** OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
** USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
****************************************************************************/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdlib.h>

#include <amxc/amxc_string_split.h>

#include "amxb_ubus.h"

static int amxb_ubus_status_to_amxd_status(int rv, amxc_var_t* result) {
    int retval = amxd_status_ok;
    amxc_var_t* error = GETI_ARG(result, 2);

    int status[] = {
        amxd_status_ok,                        // UBUS_STATUS_OK
        amxd_status_invalid_function,          // UBUS_STATUS_INVALID_COMMAND
        amxd_status_invalid_arg,               // UBUS_STATUS_INVALID_ARGUMENT
        amxd_status_function_not_found,        // UBUS_STATUS_METHOD_NOT_FOUND
        amxd_status_object_not_found,          // UBUS_STATUS_NOT_FOUND,
        amxd_status_unknown_error,             // UBUS_STATUS_NO_DATA
        amxd_status_permission_denied,         // UBUS_STATUS_PERMISSION_DENIED
        amxd_status_timeout,                   // UBUS_STATUS_TIMEOUT
        amxd_status_not_supported,             // UBUS_STATUS_NOT_SUPPORTED
        amxd_status_unknown_error,             // UBUS_STATUS_UNKNOWN_ERROR
        amxd_status_unknown_error,             // UBUS_STATUS_CONNECTION_FAILED
    };

    if((error != NULL) && (GET_ARG(error, "amxd-error-code") != NULL)) {
        // to be able to pass the amxd error code 3 results are passed over ubus
        // 1 - the function return value
        // 2 - the function out arguments
        // 3 - the amxd error code
        retval = GET_UINT32(error, "amxd-error-code");
        amxc_var_delete(&error);
        // if the error code was found, check if nr 2 is an empty table,
        // if an empty table remove it from the result.
        error = GETI_ARG(result, 1);
        if((amxc_var_type_of(error) == AMXC_VAR_ID_HTABLE) &&
           amxc_htable_is_empty(amxc_var_constcast(amxc_htable_t, error))) {
            amxc_var_delete(&error);
        }
    } else {
        if((rv >= 0) && (rv < (int) (sizeof(status) / sizeof(int)))) {
            retval = status[rv];
        } else {
            retval = AMXB_ERROR_BUS_UNKNOWN;
        }
    }

    return retval;
}


static void amxb_ubus_transform_args(amxc_var_t* dest, amxc_var_t* args, const char* method) {
    amxc_var_set_type(dest, AMXC_VAR_ID_HTABLE);
    if(method[0] != '_') {
        amxc_var_add_key(cstring_t, dest, "method", method);
        if(args != NULL) {
            amxc_var_set_key(dest, "args", args, AMXC_VAR_FLAG_COPY);
        } else {
            amxc_var_add_key(amxc_htable_t, dest, "args", NULL);
        }
    } else {
        if(args != NULL) {
            amxc_var_copy(dest, args);
        }
    }
}

static void amxb_ubus_convert_out_args(amxc_var_t* result) {
    amxc_var_t* out_args = GETI_ARG(result, 1);
    amxc_var_t* args = GET_ARG(out_args, "args");
    amxc_var_t copy_var;

    amxc_var_init(&copy_var);

    when_null(args, exit);

    amxc_var_move(&copy_var, args);
    amxc_var_move(out_args, &copy_var);

exit:
    amxc_var_clean(&copy_var);
    return;
}

static void amxb_ubus_request_done(struct ubus_request* req, int ret) {
    amxb_ubus_request_t* amxb_ubus_req = (amxb_ubus_request_t*) req->priv;
    amxb_request_t* request = amxb_ubus_req->request;
    amxc_var_t* var_method_name = GET_ARG(&amxb_ubus_req->args, "method");
    const char* method = GET_CHAR(var_method_name, NULL);
    const amxb_bus_ctx_t* ctx = amxb_request_get_ctx(request);
    amxb_ubus_t* amxb_ubus_ctx = (amxb_ubus_t*) ctx->bus_ctx;

    if((ret == UBUS_STATUS_METHOD_NOT_FOUND) && (method != NULL) && (method[0] != '_')) {
        const amxc_htable_t* htable = amxc_var_constcast(amxc_htable_t, &amxb_ubus_req->args);
        blob_buf_init(&amxb_ubus_ctx->b, 0);

        amxb_ubus_format_blob_table(htable, &amxb_ubus_ctx->b);

        ret = ubus_invoke_async(amxb_ubus_ctx->ubus_ctx,
                                amxb_ubus_req->id,
                                "_exec",
                                amxb_ubus_ctx->b.head,
                                amxb_ubus_req->ubus_req);

        amxb_ubus_req->ubus_req->data_cb = amxb_ubus_result_data;
        amxb_ubus_req->ubus_req->complete_cb = amxb_ubus_request_done;
        amxb_ubus_req->ubus_req->priv = amxb_ubus_req;
        amxb_ubus_req->converted = true;

        amxc_var_set(cstring_t, var_method_name, "_exec");
        ubus_complete_request_async(amxb_ubus_ctx->ubus_ctx, amxb_ubus_req->ubus_req);

        if(ret == 0) {
            return;
        }
    }

    ret = amxb_ubus_status_to_amxd_status(ret, request->result);
    request->bus_retval = ret;
    if(amxb_ubus_req->converted) {
        amxb_ubus_convert_out_args(request->result);
    }

    if(request->done_fn != NULL) {
        request->done_fn(ctx, request, ret, request->priv);
    }
}

/*
   When using key object paths, which are not known by ubusd, try to find the
   largest possible path, known by ubusd.

   When one of the internal data model methods are called (_get, _set, ....) update
   the rel_path argument or add it when not available.

   If not one of the internal data model methods, convert to `_exec` method
 */
static int amxb_ubus_convert(amxb_ubus_t* amxb_ubus_ctx,
                             amxb_invoke_t* invoke_ctx,
                             amxc_var_t* args) {
    amxd_path_t path;
    char* object = NULL;
    char* method = invoke_ctx->method;
    amxc_string_t rel_path;
    int retval = -1;

    amxc_string_init(&rel_path, 64);
    amxd_path_init(&path, NULL);
    amxd_path_setf(&path, true, "%s", invoke_ctx->object);

    free(invoke_ctx->object);
    invoke_ctx->object = NULL;
    invoke_ctx->method = NULL;

    when_failed(amxb_ubus_get_longest_path(amxb_ubus_ctx, &path, &rel_path), exit);

    object = strdup(amxd_path_get(&path, 0));

    if(method[0] == '_') {
        amxc_var_t* vrel_path = GET_ARG(args, "rel_path");
        if(vrel_path == NULL) {
            amxc_var_add_key(cstring_t, args, "rel_path", amxc_string_get(&rel_path, 0));
        } else {
            amxc_string_appendf(&rel_path, "%s", GET_CHAR(vrel_path, NULL));
            amxc_var_set(cstring_t, vrel_path, amxc_string_get(&rel_path, 0));
        }
        invoke_ctx->method = method;
        method = NULL;
    } else {
        amxc_var_add_key(cstring_t, args, "rel_path", amxc_string_get(&rel_path, 0));
        invoke_ctx->method = strdup("_exec");
    }
    invoke_ctx->object = object;
    object = NULL;

    retval = 0;

exit:
    free(object);
    free(method);
    amxc_string_clean(&rel_path);
    amxd_path_clean(&path);
    return retval;
}

int PRIVATE amxb_ubus_get_longest_path(amxb_ubus_t* amxb_ubus_ctx,
                                       amxd_path_t* path,
                                       amxc_string_t* rel_path) {
    int retval = -1;
    char* part = NULL;
    const char* object = NULL;
    uint32_t id = 0;

    do{
        object = amxd_path_get(path, 0);
        when_str_empty(object, exit);
        retval = ubus_lookup_id(amxb_ubus_ctx->ubus_ctx, object, &id);
        if((retval == 0) && (id != 0)) {
            break;
        }
        free(part);
        part = amxd_path_get_last(path, true);
        amxc_string_prependf(rel_path, "%s", part);
    } while (part != NULL);

exit:
    free(part);
    return retval;
}

void PRIVATE amxb_ubus_result_data(struct ubus_request* req,
                                   AMXB_UNUSED int type,
                                   struct blob_attr* msg) {
    amxb_ubus_request_t* amxb_ubus_req = (amxb_ubus_request_t*) req->priv;
    amxb_request_t* request = amxb_ubus_req->request;

    amxc_var_t* result = request->result;
    amxc_var_t* item = NULL;
    amxc_var_t* rv = NULL;
    const amxb_bus_ctx_t* ctx = NULL;

    amxc_var_new(&item);
    ctx = amxb_request_get_ctx(request);

    if((msg != NULL) && (result != NULL)) {
        amxc_var_set_type(item, AMXC_VAR_ID_HTABLE);
        amxb_ubus_parse_blob_table(item, (struct blob_attr*) blob_data(msg), blob_len(msg));
        rv = GET_ARG(item, "retval");
        if(rv == NULL) {
            amxc_var_set_index(result, -1, item, AMXC_VAR_FLAG_DEFAULT);
        } else {
            amxc_var_take_it(rv);
            amxc_var_set_index(result, -1, rv, AMXC_VAR_FLAG_DEFAULT);
            amxc_var_delete(&item);
            item = rv;
        }
    } else {
        amxc_var_delete(&item);
    }

    if((request->cb_fn != NULL) && (item != NULL)) {
        request->cb_fn(ctx, item, request->priv);
    }
}

int PRIVATE amxb_ubus_invoke_base(amxb_ubus_t* amxb_ubus_ctx,
                                  const char* object,
                                  amxc_var_t* args,
                                  amxb_request_t* request,
                                  uint32_t* id) {
    int retval = -1;

    // lookup id can cause the ubus blob msg to be overwritten
    // first do the lookup and then initialize the blob
    retval = ubus_lookup_id(amxb_ubus_ctx->ubus_ctx, object, id);
    if(retval != 0) {
        goto exit;
    }

    blob_buf_init(&amxb_ubus_ctx->b, 0);

    if(args != NULL) {
        when_true(amxc_var_type_of(args) != AMXC_VAR_ID_HTABLE, exit);
        const amxc_htable_t* htable = amxc_var_constcast(amxc_htable_t, args);
        when_failed(amxb_ubus_format_blob_table(htable, &amxb_ubus_ctx->b), exit);
    }

    amxc_var_set_type(request->result, AMXC_VAR_ID_LIST);

exit:
    return retval;
}

int PRIVATE amxb_ubus_invoke(void* const ctx,
                             amxb_invoke_t* invoke_ctx,
                             amxc_var_t* args,
                             amxb_request_t* request,
                             int timeout) {
    amxb_ubus_t* amxb_ubus_ctx = (amxb_ubus_t*) ctx;
    uint32_t id = 0;
    int ret = amxd_status_unknown_error;
    amxb_ubus_request_t* amxb_ubus_req = NULL;

    when_str_empty(invoke_ctx->object, exit);
    amxb_ubus_req = (amxb_ubus_request_t*) calloc(1, sizeof(amxb_ubus_request_t));
    when_null(amxb_ubus_req, exit);
    amxc_var_init(&amxb_ubus_req->args);
    amxb_ubus_transform_args(&amxb_ubus_req->args, args, invoke_ctx->method);

    if(amxb_ubus_invoke_base(amxb_ubus_ctx,
                             invoke_ctx->object,
                             args,
                             request,
                             &id) != 0) {
        when_failed(amxb_ubus_convert(amxb_ubus_ctx, invoke_ctx, &amxb_ubus_req->args), exit);
        ret = amxb_ubus_invoke_base(amxb_ubus_ctx,
                                    invoke_ctx->object,
                                    &amxb_ubus_req->args,
                                    request,
                                    &id);
        ret = amxb_ubus_status_to_amxd_status(ret, request->result);
        request->bus_retval = ret;
        amxb_ubus_req->converted = true;
        when_failed(ret, exit);
    }

    amxb_ubus_req->request = request;
    request->bus_data = amxb_ubus_req;

    ret = ubus_invoke(amxb_ubus_ctx->ubus_ctx,
                      id,
                      invoke_ctx->method,
                      amxb_ubus_ctx->b.head,
                      amxb_ubus_result_data,
                      amxb_ubus_req,
                      timeout * 1000);

    if((ret == UBUS_STATUS_METHOD_NOT_FOUND) && (invoke_ctx->method[0] != '_')) {
        const amxc_htable_t* htable = NULL;
        htable = amxc_var_constcast(amxc_htable_t, &amxb_ubus_req->args);
        blob_buf_init(&amxb_ubus_ctx->b, 0);

        amxb_ubus_format_blob_table(htable, &amxb_ubus_ctx->b);

        ret = ubus_invoke(amxb_ubus_ctx->ubus_ctx,
                          id,
                          "_exec",
                          amxb_ubus_ctx->b.head,
                          amxb_ubus_result_data,
                          amxb_ubus_req,
                          timeout * 1000);
    }

    ret = amxb_ubus_status_to_amxd_status(ret, request->result);
    request->bus_retval = ret;
    if(amxb_ubus_req->converted) {
        amxb_ubus_convert_out_args(request->result);
    }

exit:
    if(amxb_ubus_req != NULL) {
        amxc_var_clean(&amxb_ubus_req->args);
    }
    free(amxb_ubus_req);
    return ret;
}

int PRIVATE amxb_ubus_async_invoke(void* const ctx,
                                   amxb_invoke_t* invoke_ctx,
                                   amxc_var_t* args,
                                   amxb_request_t* request) {
    amxb_ubus_t* amxb_ubus_ctx = (amxb_ubus_t*) ctx;
    uint32_t id = 0;
    int ret = -1;
    amxb_ubus_request_t* amxb_ubus_req = NULL;

    when_str_empty(invoke_ctx->object, exit);
    amxb_ubus_req = (amxb_ubus_request_t*) calloc(1, sizeof(amxb_ubus_request_t));
    when_null(amxb_ubus_req, exit);
    amxc_var_init(&amxb_ubus_req->args);
    amxb_ubus_transform_args(&amxb_ubus_req->args, args, invoke_ctx->method);

    if(amxb_ubus_invoke_base(amxb_ubus_ctx,
                             invoke_ctx->object,
                             args,
                             request,
                             &id) != 0) {
        when_failed(amxb_ubus_convert(amxb_ubus_ctx, invoke_ctx, &amxb_ubus_req->args), exit);
        when_failed(amxb_ubus_invoke_base(amxb_ubus_ctx,
                                          invoke_ctx->object,
                                          &amxb_ubus_req->args,
                                          request,
                                          &id),
                    exit);
    }

    amxb_ubus_req->request = request;
    amxb_ubus_req->id = id;
    request->bus_data = amxb_ubus_req;

    amxb_ubus_req->ubus_req = (struct ubus_request*) calloc(1, sizeof(struct ubus_request));
    when_null(amxb_ubus_req->ubus_req, exit);

    ret = ubus_invoke_async(amxb_ubus_ctx->ubus_ctx,
                            id,
                            invoke_ctx->method,
                            amxb_ubus_ctx->b.head,
                            amxb_ubus_req->ubus_req);

    amxb_ubus_req->ubus_req->data_cb = amxb_ubus_result_data;
    amxb_ubus_req->ubus_req->complete_cb = amxb_ubus_request_done;
    amxb_ubus_req->ubus_req->priv = amxb_ubus_req;

    ubus_complete_request_async(amxb_ubus_ctx->ubus_ctx, amxb_ubus_req->ubus_req);

exit:
    if(ret != 0) {
        if(amxb_ubus_req != NULL) {
            amxc_var_clean(&amxb_ubus_req->args);
            free(amxb_ubus_req->ubus_req);
        }
        free(amxb_ubus_req);
        request->bus_data = NULL;
    }
    return ret;
}

int PRIVATE amxb_ubus_wait_request(AMXB_UNUSED void* const ctx,
                                   amxb_request_t* request,
                                   int timeout) {
    amxb_ubus_t* amxb_ubus_ctx = (amxb_ubus_t*) ctx;
    amxb_ubus_request_t* amxb_ubus_req = (amxb_ubus_request_t*) request->bus_data;
    struct ubus_request* ubus_req = amxb_ubus_req->ubus_req;

    return ubus_complete_request(amxb_ubus_ctx->ubus_ctx, ubus_req, timeout);
}

int PRIVATE amxb_ubus_close_request(void* const ctx,
                                    amxb_request_t* request) {
    amxb_ubus_t* amxb_ubus_ctx = (amxb_ubus_t*) ctx;
    amxb_ubus_request_t* amxb_ubus_req = (amxb_ubus_request_t*) request->bus_data;

    if(amxb_ubus_req != NULL) {
        struct ubus_request* ubus_req = amxb_ubus_req->ubus_req;
        ubus_abort_request(amxb_ubus_ctx->ubus_ctx, ubus_req);
        amxc_var_clean(&amxb_ubus_req->args);
        free(ubus_req);
        free(amxb_ubus_req);
        request->bus_data = NULL;
    }
    return 0;
}
