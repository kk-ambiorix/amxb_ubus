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

#include <amxc/amxc_string_split.h>

#include "amxb_ubus.h"

typedef struct _amxb_ubus_fcall {
    struct ubus_request_data req;
    amxb_ubus_t* amxb_ubus_ctx;
    uint64_t call_id;
    amxc_llist_it_t it;
} amxb_ubus_fcall_t;

static enum blobmsg_type amxb_ubus_var_type_to_ubus_type(int type) {
    enum blobmsg_type converter[AMXC_VAR_ID_CUSTOM_BASE] = {
        BLOBMSG_TYPE_UNSPEC,
        BLOBMSG_TYPE_STRING,
        BLOBMSG_TYPE_INT32,
        BLOBMSG_TYPE_INT32,
        BLOBMSG_TYPE_INT32,
        BLOBMSG_TYPE_INT64,
        BLOBMSG_TYPE_INT32,
        BLOBMSG_TYPE_INT32,
        BLOBMSG_TYPE_INT64,
        BLOBMSG_TYPE_INT64,
        BLOBMSG_TYPE_UNSPEC,
        BLOBMSG_TYPE_DOUBLE,
        BLOBMSG_TYPE_BOOL,
        BLOBMSG_TYPE_ARRAY,
        BLOBMSG_TYPE_TABLE,
        BLOBMSG_TYPE_UNSPEC,
        BLOBMSG_TYPE_UNSPEC,
    };

    if(type <= AMXC_VAR_ID_ANY) {
        return converter[type];
    } else {
        return BLOBMSG_TYPE_UNSPEC;
    }

}

static int isdot(int c) {
    return (c == '.') ? 1 : 0;
}

static int amxb_ubus_amxd_to_ubus_status(amxd_status_t rv) {
    int status[amxd_status_last] = {
        UBUS_STATUS_OK,                 // amxd_status_ok
        UBUS_STATUS_UNKNOWN_ERROR,      // amxd_status_unknown_error
        UBUS_STATUS_NOT_FOUND,          // amxd_status_object_not_found
        UBUS_STATUS_METHOD_NOT_FOUND,   // amxd_status_function_not_found
        UBUS_STATUS_NOT_FOUND,          // amxd_status_parameter_not_found
        UBUS_STATUS_NOT_SUPPORTED,      // amxd_status_function_not_implemented
        UBUS_STATUS_INVALID_COMMAND,    // amxd_status_invalid_function
        UBUS_STATUS_INVALID_ARGUMENT,   // amxd_status_invalid_function_argument
        UBUS_STATUS_INVALID_ARGUMENT,   // amxd_status_invalid_name
        UBUS_STATUS_INVALID_ARGUMENT,   // amxd_status_invalid_attr
        UBUS_STATUS_INVALID_ARGUMENT,   // amxd_status_invalid_value
        UBUS_STATUS_NOT_SUPPORTED,      // amxd_status_invalid_action
        UBUS_STATUS_INVALID_ARGUMENT,   // amxd_status_invalid_type
        UBUS_STATUS_UNKNOWN_ERROR,      // amxd_status_duplicate
        UBUS_STATUS_UNKNOWN_ERROR,      // amxd_status_deferred
        UBUS_STATUS_NOT_SUPPORTED,      // amxd_status_read_only
        UBUS_STATUS_INVALID_ARGUMENT,   // amxd_status_missing_key
        UBUS_STATUS_UNKNOWN_ERROR,      // amxd_status_file_not_found
        UBUS_STATUS_INVALID_ARGUMENT,   // amxd_status_invalid_arg
        UBUS_STATUS_UNKNOWN_ERROR,      // amxd_status_out_of_mem
        UBUS_STATUS_UNKNOWN_ERROR,      // amxd_status_recursion
        UBUS_STATUS_NOT_FOUND,          // amxd_status_invalid_path
        UBUS_STATUS_UNKNOWN_ERROR,      // amxd_status_invalid_expr
    };

    if(rv < sizeof(status) / sizeof(int)) {
        return status[rv];
    } else {
        return UBUS_STATUS_UNKNOWN_ERROR;
    }
}

static void amxb_ubus_func_return(amxb_ubus_t* amxb_ubus_ctx,
                                  amxc_var_t* ret) {
    switch(amxc_var_type_of(ret)) {
    case AMXC_VAR_ID_HTABLE:
        amxb_ubus_format_blob_table(amxc_var_constcast(amxc_htable_t, ret),
                                    &amxb_ubus_ctx->b);
        break;
    case AMXC_VAR_ID_LIST: {
        void* c = blobmsg_open_array(&amxb_ubus_ctx->b, "retval");
        amxb_ubus_format_blob_array(amxc_var_constcast(amxc_llist_t, ret),
                                    &amxb_ubus_ctx->b);
        blobmsg_close_array(&amxb_ubus_ctx->b, c);
    }
    break;
    default:
        amxb_ubus_format_blob(ret, "retval", &amxb_ubus_ctx->b);
        break;
    }
}

static void amxb_ubus_build_messages(amxb_ubus_fcall_t* fcall,
                                     amxc_var_t* ret,
                                     amxc_var_t* args,
                                     amxc_var_t* status) {
    // Each ambiorix function must return 3 messages:
    // 1. the function return value, this can be of any type
    // 2. the function out arguments, this must be a hash table, if no out
    //    arguments are available, an empty hash table must be send. ubus doesn't
    //    support NULL or void
    // 3. the amxd status code, in a hash table with name "amxd-error-code"

    if(args == NULL) {
        amxc_var_new(&args);
        amxc_var_set_type(args, AMXC_VAR_ID_HTABLE);
    } else if(amxc_var_type_of(args) != AMXC_VAR_ID_HTABLE) {
        amxc_var_set_type(args, AMXC_VAR_ID_HTABLE);
    }

    blob_buf_init(&fcall->amxb_ubus_ctx->b, 0);
    amxb_ubus_func_return(fcall->amxb_ubus_ctx, ret);
    ubus_send_reply(fcall->amxb_ubus_ctx->ubus_ctx, &fcall->req, fcall->amxb_ubus_ctx->b.head);

    blob_buf_init(&fcall->amxb_ubus_ctx->b, 0);
    amxb_ubus_func_return(fcall->amxb_ubus_ctx, args);
    ubus_send_reply(fcall->amxb_ubus_ctx->ubus_ctx, &fcall->req, fcall->amxb_ubus_ctx->b.head);

    blob_buf_init(&fcall->amxb_ubus_ctx->b, 0);
    amxb_ubus_format_blob(status, "amxd-error-code", &fcall->amxb_ubus_ctx->b);
    ubus_send_reply(fcall->amxb_ubus_ctx->ubus_ctx, &fcall->req, fcall->amxb_ubus_ctx->b.head);

    amxc_var_take_it(args);
    amxc_var_delete(&args);
}

static void amxb_ubus_exec_done(const amxc_var_t* const data,
                                void* const priv) {
    amxb_ubus_fcall_t* fcall = (amxb_ubus_fcall_t*) priv;

    amxc_var_t* ret = GET_ARG(data, "retval");
    amxc_var_t* args = GET_ARG(data, "args");
    amxc_var_t* status = GET_ARG(data, "status");

    int rv = amxb_ubus_amxd_to_ubus_status((amxd_status_t) GET_UINT32(status, NULL));
    amxb_ubus_build_messages(fcall, ret, args, status);
    ubus_complete_deferred_request(fcall->amxb_ubus_ctx->ubus_ctx, &fcall->req, rv);

    amxc_llist_it_take(&fcall->it);
    free(fcall);
}

static void amxb_ubus_call_func(const amxc_var_t* const data,
                                void* const priv) {
    amxb_ubus_fcall_t* fcall = (amxb_ubus_fcall_t*) priv;
    const char* method = GET_CHAR(data, "method");
    const char* path = GET_CHAR(data, "path");
    amxc_var_t* args = GET_ARG(data, "args");
    amxb_ubus_t* amxb_ubus_ctx = fcall->amxb_ubus_ctx;
    amxd_status_t retval = amxd_status_ok;
    amxc_var_t status;

    amxd_object_t* root_obj = amxd_dm_get_root(amxb_ubus_ctx->dm);
    amxd_object_t* dm_obj = amxd_object_findf(root_obj, "%s", path);

    amxc_var_t ret;
    amxc_var_init(&ret);
    amxc_var_init(&status);
    retval = amxd_object_invoke_function(dm_obj, method, args, &ret);
    amxc_var_set(uint32_t, &status, retval);

    switch(retval) {
    default:
        amxb_ubus_build_messages(fcall, &ret, args, &status);
        ubus_complete_deferred_request(fcall->amxb_ubus_ctx->ubus_ctx,
                                       &fcall->req,
                                       amxb_ubus_amxd_to_ubus_status(retval));

        amxd_function_deferred_remove(fcall->call_id);
        amxc_llist_it_take(&fcall->it);
        free(fcall);
        break;
    case amxd_status_deferred: {
        uint64_t call_id = amxc_var_constcast(uint64_t, &ret);
        amxd_function_deferred_remove(fcall->call_id);

        fcall->call_id = call_id;

        amxd_function_set_deferred_cb(call_id, amxb_ubus_exec_done, fcall);
    }
    break;
    }

    amxc_var_clean(&status);
    amxc_var_clean(&ret);
}

/*
** UBus doesn't respect the order of processing incoming messages.

   When a process has sent a request (ubus message) to another process and is waiting
   for response (synchronous) it is possible that other requests are received.

   These incoming requests are added to a queue and are handled after the response is
   received, but before the synchronous ubus call returns. This can break the order in
   which messages are processed.

   To ensure that all messages are handled in the correct order, all func handlers
   are called deferred from the eventloop.
*/
static int amxb_ubus_func_handler(struct ubus_context* ctx,
                                  struct ubus_object* obj,
                                  struct ubus_request_data* req,
                                  const char* method,
                                  struct blob_attr* msg) {
    int status = UBUS_STATUS_OK;
    amxb_ubus_object_t* amxb_ubus_obj = amxc_container_of(obj, amxb_ubus_object_t, ubus_obj);
    amxb_ubus_t* amxb_ubus_ctx = amxb_ubus_obj->amxb_ubus_ctx;
    amxb_ubus_fcall_t* fcall = NULL;
    amxd_object_t* root_obj = amxd_dm_get_root(amxb_ubus_ctx->dm);
    amxd_object_t* dm_obj = amxd_object_findf(root_obj, "%s", amxb_ubus_obj->ubus_path);
    amxd_function_t* func = amxd_object_get_function(dm_obj, method);

    amxc_var_t data;
    amxc_var_t ret;
    amxc_var_t* args = NULL;
    amxc_var_init(&data);
    amxc_var_init(&ret);

    when_null_status(dm_obj, exit, status = UBUS_STATUS_NOT_FOUND);
    when_null_status(func, exit, status = UBUS_STATUS_METHOD_NOT_FOUND);

    amxc_var_set_type(&data, AMXC_VAR_ID_HTABLE);

    amxc_var_add_key(cstring_t, &data, "method", method);
    amxc_var_add_key(cstring_t, &data, "path", amxb_ubus_obj->ubus_path);
    args = amxc_var_add_key(amxc_htable_t, &data, "args", NULL);
    amxb_ubus_parse_blob_table(args, (struct blob_attr*) blob_data(msg), blob_len(msg));

    fcall = (amxb_ubus_fcall_t*) calloc(1, sizeof(amxb_ubus_fcall_t));
    amxd_function_defer(func, &fcall->call_id, &ret, NULL, NULL);
    fcall->amxb_ubus_ctx = amxb_ubus_ctx;
    amxc_llist_append(&amxb_ubus_ctx->pending_reqs, &fcall->it);

    ubus_defer_request(ctx, req, &fcall->req);

    status = amxp_sigmngr_deferred_call(NULL, amxb_ubus_call_func, &data, fcall);

exit:
    amxc_var_clean(&data);
    amxc_var_clean(&ret);
    return status;
}

static void amxb_register_add_function(amxd_object_t* const object,
                                       const char* name,
                                       struct ubus_method* ubus_method) {
    amxd_function_t* func = amxd_object_get_function(object, name);
    size_t n_args = 0;
    int index = 0;
    struct blobmsg_policy* args = NULL;

    when_null(func, exit);
    when_null(ubus_method, exit);

    ubus_method->name = func->name;
    ubus_method->handler = amxb_ubus_func_handler;

    n_args = amxc_llist_size(&func->args);
    args = (struct blobmsg_policy*) calloc(n_args, sizeof(struct blobmsg_policy));
    when_null(args, exit);

    amxc_llist_for_each(it, (&func->args)) {
        amxd_func_arg_t* amxd_arg = amxc_llist_it_get_data(it, amxd_func_arg_t, it);
        args[index].name = amxd_arg->name;
        args[index].type = amxb_ubus_var_type_to_ubus_type(amxd_arg->type);
        index++;
    }

    ubus_method->policy = args;
    ubus_method->n_policy = n_args;

exit:
    return;
}

static bool amxb_ubus_filter_object(amxd_object_t* const object,
                                    AMXB_UNUSED int32_t depth,
                                    AMXB_UNUSED void* priv) {
    bool retval = true;
    amxd_object_t* parent = NULL;
    when_true(amxd_object_get_type(object) == amxd_object_root, exit);

    if(amxd_object_is_attr_set(object, amxd_oattr_private)) {
        retval = false;
        goto exit;
    }

    parent = amxd_object_get_parent(object);
    if(amxd_object_get_type(object) != amxd_object_instance) {
        if(amxd_object_get_type(parent) == amxd_object_template) {
            retval = false;
            goto exit;
        }
    }

exit:
    return retval;
}

static void amxb_ubus_send_notification(const char* const sig_name,
                                        const amxc_var_t* const data,
                                        void* const priv) {
    amxb_ubus_object_t* amxb_ubus_obj = (amxb_ubus_object_t*) priv;
    amxb_ubus_t* amxb_ubus_ctx = amxb_ubus_obj->amxb_ubus_ctx;

    blob_buf_init(&amxb_ubus_ctx->b, 0);
    amxb_ubus_format_blob_table(amxc_var_constcast(amxc_htable_t, data),
                                &amxb_ubus_ctx->b);
    ubus_notify(amxb_ubus_ctx->ubus_ctx,
                &amxb_ubus_obj->ubus_obj,
                sig_name, amxb_ubus_ctx->b.head,
                -1);
}

static void amxb_ubus_subcribe(AMXB_UNUSED struct ubus_context* ctx,
                               struct ubus_object* obj) {
    amxb_ubus_object_t* amxb_ubus_obj = amxc_container_of(obj,
                                                          amxb_ubus_object_t,
                                                          ubus_obj);
    amxb_ubus_t* amxb_ubus_ctx = amxb_ubus_obj->amxb_ubus_ctx;
    amxd_object_t* root_obj = amxd_dm_get_root(amxb_ubus_ctx->dm);
    amxd_object_t* dm_obj = amxd_object_findf(root_obj, "%s",
                                              amxb_ubus_obj->ubus_path);
    when_null(dm_obj, exit);

    if(amxb_ubus_obj->ubus_obj.has_subscribers) {
        amxc_string_t expression;
        char* path = amxd_object_get_path(dm_obj, AMXD_OBJECT_INDEXED);

        amxc_string_init(&expression, 0);
        amxc_string_appendf(&expression, "path starts with \"%s.\"", path);
        amxp_slot_connect_filtered(&amxb_ubus_ctx->dm->sigmngr,
                                   ".*",
                                   amxc_string_get(&expression, 0),
                                   amxb_ubus_send_notification,
                                   amxb_ubus_obj);
        amxc_string_clean(&expression);
        free(path);
    } else {
        amxp_slot_disconnect_with_priv(&amxb_ubus_ctx->dm->sigmngr,
                                       amxb_ubus_send_notification,
                                       amxb_ubus_obj);
    }

exit:
    return;
}

static void amxb_ubus_build_obj(amxb_ubus_t* amxb_ubus_ctx,
                                amxb_ubus_object_t* obj,
                                amxd_object_t* const object) {
    struct ubus_method* methods = NULL;
    const amxc_llist_t* lfuncs = NULL;
    amxc_var_t funcs;
    int index = 0;
    amxc_var_init(&funcs);

    obj->ubus_obj.subscribe_cb = amxb_ubus_subcribe;
    obj->amxb_ubus_ctx = amxb_ubus_ctx;
    obj->ubus_path = amxd_object_get_path(object, AMXD_OBJECT_INDEXED);

    amxd_object_list_functions(object, &funcs, amxd_dm_access_protected);
    lfuncs = amxc_var_constcast(amxc_llist_t, &funcs);

    obj->ubus_obj.name = obj->ubus_path;
    obj->ubus_obj.n_methods = amxc_llist_size(lfuncs);
    if(obj->ubus_obj.n_methods > 0) {
        methods = (struct ubus_method*) calloc(obj->ubus_obj.n_methods, sizeof(struct ubus_method));
    }
    obj->ubus_obj.methods = methods;
    obj->ubus_obj.type->name = obj->ubus_path;
    obj->ubus_obj.type->id = 0;
    obj->ubus_obj.type->n_methods = obj->ubus_obj.n_methods;
    obj->ubus_obj.type->methods = obj->ubus_obj.methods;

    amxc_llist_for_each(it, lfuncs) {
        const char* func_name = amxc_var_constcast(cstring_t, amxc_var_from_llist_it(it));
        amxb_register_add_function(object,
                                   func_name,
                                   &methods[index]);
        index++;
    }

    amxc_var_clean(&funcs);
}

static void amxb_ubus_register_object(amxd_object_t* const object,
                                      AMXB_UNUSED int32_t depth,
                                      void* priv) {
    amxb_ubus_object_t* obj = NULL;
    amxb_ubus_t* amxb_ubus_ctx = (amxb_ubus_t*) priv;

    when_true(object->type == amxd_object_root, exit);

    obj = (amxb_ubus_object_t*) calloc(1, sizeof(amxb_ubus_object_t));
    when_null(obj, exit);
    obj->ubus_obj.type = (struct ubus_object_type*) calloc(1, sizeof(struct ubus_object_type));
    if(obj->ubus_obj.type == NULL) {
        free(obj);
        goto exit;
    }

    amxb_ubus_build_obj(amxb_ubus_ctx, obj, object);
    if(ubus_add_object(amxb_ubus_ctx->ubus_ctx, &obj->ubus_obj) != UBUS_STATUS_OK) {
        amxb_ubus_obj_it_free(&obj->it);
        goto exit;
    }

    amxc_llist_append(&amxb_ubus_ctx->registered_objs, &obj->it);

exit:
    return;
}

static void amxb_ubus_register_add(const char* const sig_name,
                                   const amxc_var_t* const data,
                                   void* const priv) {
    amxb_ubus_t* amxb_ubus_ctx = (amxb_ubus_t*) priv;
    amxd_object_t* object = amxd_dm_signal_get_object(amxb_ubus_ctx->dm, data);

    if(strcmp(sig_name, "dm:instance-added") == 0) {
        uint32_t index = amxc_var_constcast(uint32_t,
                                            amxc_var_get_key(data, "index", 0));
        object = amxd_object_get_instance(object, NULL, index);
    }

    if(object != NULL) {
        amxb_ubus_register_object(object, 0, amxb_ubus_ctx);
    }
}

static void amxb_ubus_register_tree(AMXB_UNUSED const char* const sig_name,
                                    const amxc_var_t* const data,
                                    void* const priv) {
    amxb_ubus_t* amxb_ubus_ctx = (amxb_ubus_t*) priv;
    amxd_object_t* object = amxd_dm_signal_get_object(amxb_ubus_ctx->dm, data);

    if(object != NULL) {
        amxb_ubus_register_object(object, 0, amxb_ubus_ctx);
        amxd_object_hierarchy_walk(object,
                                   amxd_direction_down,
                                   amxb_ubus_filter_object,
                                   amxb_ubus_register_object,
                                   INT32_MAX,
                                   amxb_ubus_ctx);
    }
}

static void amxb_ubus_register_remove(const char* const sig_name,
                                      const amxc_var_t* const data,
                                      void* const priv) {
    amxb_ubus_t* amxb_ubus_ctx = (amxb_ubus_t*) priv;
    amxd_dm_t* dm = amxb_ubus_ctx->dm;
    const char* object_path = GET_CHAR(data, "path");
    amxc_string_t full_path;

    amxc_string_init(&full_path, 0);
    if(strcmp(sig_name, "dm:instance-removed") == 0) {
        uint32_t index = amxc_var_constcast(uint32_t,
                                            amxc_var_get_key(data, "index", 0));
        amxc_string_setf(&full_path, "%s%d", object_path, index);
    } else {
        amxc_string_setf(&full_path, "%s", object_path);
        amxc_string_trimr(&full_path, isdot);
    }

    amxc_llist_for_each(it, (&amxb_ubus_ctx->registered_objs)) {
        amxb_ubus_object_t* obj = amxc_llist_it_get_data(it,
                                                         amxb_ubus_object_t,
                                                         it);
        if(strcmp(obj->ubus_obj.name, amxc_string_get(&full_path, 0)) == 0) {
            ubus_remove_object(amxb_ubus_ctx->ubus_ctx, &obj->ubus_obj);
            amxp_slot_disconnect_with_priv(&dm->sigmngr, amxb_ubus_send_notification, obj);
            amxb_ubus_obj_it_free(it);
            break;
        }
    }

    amxc_string_clean(&full_path);
}

static void amxb_ubus_register_dm(UNUSED const char* const sig_name,
                                  UNUSED const amxc_var_t* const data,
                                  void* const priv) {
    amxb_ubus_t* amxb_ubus_ctx = (amxb_ubus_t*) priv;
    amxd_dm_t* dm = amxb_ubus_ctx->dm;

    amxd_object_hierarchy_walk(amxd_dm_get_root(dm),
                               amxd_direction_down,
                               amxb_ubus_filter_object,
                               amxb_ubus_register_object,
                               INT32_MAX,
                               amxb_ubus_ctx);

    amxp_slot_disconnect_with_priv(&dm->sigmngr, amxb_ubus_register_dm, amxb_ubus_ctx);

    amxp_slot_connect(&dm->sigmngr,
                      "dm:root-added",
                      NULL,
                      amxb_ubus_register_tree,
                      amxb_ubus_ctx);

    amxp_slot_connect(&dm->sigmngr,
                      "dm:root-removed",
                      NULL,
                      amxb_ubus_register_remove,
                      amxb_ubus_ctx);

    amxp_slot_connect(&dm->sigmngr,
                      "dm:object-added",
                      NULL,
                      amxb_ubus_register_add,
                      amxb_ubus_ctx);
    amxp_slot_connect(&dm->sigmngr,
                      "dm:instance-added",
                      NULL,
                      amxb_ubus_register_add,
                      amxb_ubus_ctx);
    amxp_slot_connect(&dm->sigmngr,
                      "dm:object-removed",
                      NULL,
                      amxb_ubus_register_remove,
                      amxb_ubus_ctx);
    amxp_slot_connect(&dm->sigmngr,
                      "dm:instance-removed",
                      NULL,
                      amxb_ubus_register_remove,
                      amxb_ubus_ctx);
}

void PRIVATE amxb_ubus_cancel_requests(amxb_ubus_t* amxb_ubus_ctx) {
    amxc_var_t ret;
    amxc_var_init(&ret);
    amxc_llist_for_each(it, &amxb_ubus_ctx->pending_reqs) {
        amxb_ubus_fcall_t* fcall = amxc_container_of(it, amxb_ubus_fcall_t, it);
        blob_buf_init(&amxb_ubus_ctx->b, 0);
        amxb_ubus_func_return(fcall->amxb_ubus_ctx, &ret);
        ubus_send_reply(amxb_ubus_ctx->ubus_ctx, &fcall->req, amxb_ubus_ctx->b.head);

        ubus_complete_deferred_request(amxb_ubus_ctx->ubus_ctx,
                                       &fcall->req,
                                       UBUS_STATUS_UNKNOWN_ERROR);

        amxd_function_set_deferred_cb(fcall->call_id, NULL, NULL);
        amxd_function_deferred_remove(fcall->call_id);
        amxc_llist_it_take(it);
        free(fcall);
    }

    amxc_var_clean(&ret);
}

void PRIVATE amxb_ubus_obj_it_free(amxc_llist_it_t* it) {
    amxb_ubus_object_t* obj = amxc_llist_it_get_data(it,
                                                     amxb_ubus_object_t,
                                                     it);
    struct ubus_method* methods = (struct ubus_method*) obj->ubus_obj.methods;

    amxc_llist_it_take(it);
    for(int i = 0; i < obj->ubus_obj.n_methods; i++) {
        struct blobmsg_policy* policy =
            (struct blobmsg_policy*) methods[i].policy;
        free(policy);
        methods[i].policy = NULL;
    }


    amxp_slot_disconnect_with_priv(&obj->amxb_ubus_ctx->dm->sigmngr,
                                   amxb_ubus_send_notification,
                                   obj);

    free(obj->ubus_path);
    free(methods);
    free(obj->ubus_obj.type);
    free(obj);
}

int PRIVATE amxb_ubus_register(void* const ctx,
                               amxd_dm_t* const dm) {
    int status = UBUS_STATUS_UNKNOWN_ERROR;
    amxb_ubus_t* amxb_ubus_ctx = (amxb_ubus_t*) ctx;
    const amxc_var_t* cfg_ros = amxb_ubus_get_config_option("register-on-start-event");

    when_not_null(amxb_ubus_ctx->dm, exit);
    amxb_ubus_ctx->dm = dm;

    if(amxc_var_dyncast(bool, cfg_ros)) {
        amxp_slot_connect(&dm->sigmngr,
                          "app:start",
                          NULL,
                          amxb_ubus_register_dm,
                          amxb_ubus_ctx);
    } else {
        amxb_ubus_register_dm(NULL, NULL, ctx);
    }

    status = UBUS_STATUS_OK;

exit:
    return status;
}
