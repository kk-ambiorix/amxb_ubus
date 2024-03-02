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

#include "amxb_ubus.h"

typedef struct _ubus_filter {
    uint32_t depth;
    const char* path;
    uint32_t path_len;
    amxc_var_t objects;
    uint32_t flags;
    uint32_t access;
    amxc_var_t amx_paths;
    amxb_request_t* request;
} ubus_filter_t;

static int32_t amxb_ubus_object_depth(const char* path) {
    uint32_t length = path == NULL ? 0 : strlen(path);
    int32_t depth = 0;
    for(uint32_t i = 0; i < length; i++) {
        if(path[i] == '.') {
            depth++;
        }
    }

    return depth;
}

static bool amxb_ubus_has_function(struct ubus_object_data* o, const char* func_name) {
    bool retval = false;
    struct blob_attr* cur;
    size_t rem = 0;
    (void) rem; // clang complains about not used variable

    when_null(o->signature, exit);
    blob_for_each_attr(cur, o->signature, rem) {
        (void) rem; // clang complains about not used variable
        if(strcmp(func_name, blobmsg_name(cur)) == 0) {
            retval = true;
            goto exit;
        }
    }

exit:
    return retval;
}

static void amxb_ubus_add_functions(struct ubus_object_data* o,
                                    ubus_filter_t* filter,
                                    const char* path,
                                    amxc_var_t* retval) {
    if(o->signature && ((filter->flags & AMXB_FLAG_FUNCTIONS) != 0)) {
        struct blob_attr* cur;
        size_t rem = 0;
        (void) rem; // clang complains about not used variable
        // but rem is needed to be able to use the following macro
        blob_for_each_attr(cur, o->signature, rem) {
            (void) rem; // clang complains about not used variable
            amxc_string_t func_name;
            amxc_string_init(&func_name, 0);
            amxc_string_setf(&func_name, "%s%s", path, blobmsg_name(cur));
            amxc_var_add(cstring_t, retval, amxc_string_get(&func_name, 0));
            amxc_string_clean(&func_name);
        }
    }
}

static void amxb_ubus_fill_list(amxb_bus_ctx_t* bus_ctx,
                                const char* path,
                                amxc_var_t* data,
                                const char* type,
                                bool fetch_names,
                                amxc_var_t* retval) {
    amxc_string_t name;
    amxc_string_init(&name, 0);

    amxc_var_t* type_data = GETP_ARG(data, type);
    amxc_var_for_each(d, type_data) {
        if(amxc_var_type_of(type_data) == AMXC_VAR_ID_HTABLE) {
            amxc_string_setf(&name, "%s%s", path, amxc_var_key(d));
        } else {
            amxc_string_setf(&name, "%s%s", path, GET_CHAR(d, NULL));
        }
        amxc_var_add(cstring_t, retval, amxc_string_get(&name, 0));
        if(fetch_names) {
            amxc_var_t tmp;
            amxc_var_init(&tmp);
            amxb_describe(bus_ctx, amxc_string_get(&name, 0), 0, &tmp, 5);
            amxc_string_setf(&name, "%s%s", path, GETP_CHAR(&tmp, "0.name"));
            amxc_var_add(cstring_t, retval, amxc_string_get(&name, 0));
            amxc_var_clean(&tmp);
        }
    }

    amxc_string_clean(&name);
}

static int amxb_ubus_list_object_describe(amxb_bus_ctx_t* bus_ctx,
                                          const char* object,
                                          ubus_filter_t* filter) {
    int rv = 0;
    amxc_var_t tmp;
    amxc_string_t path;
    amxc_var_t* subojects = NULL;
    amxc_var_t output;

    amxc_string_init(&path, 0);
    amxc_var_init(&tmp);

    rv = amxb_describe(bus_ctx, object, filter->flags, &tmp, 5);
    when_failed(rv, exit);

    amxc_var_init(&output);
    amxc_var_set_type(&output, AMXC_VAR_ID_LIST);
    amxc_var_add(cstring_t, &output, object);

    if(GETP_UINT32(&tmp, "0.type_id") != 2) {
        if((filter->flags & AMXB_FLAG_PARAMETERS) != 0) {
            amxb_ubus_fill_list(bus_ctx, object, &tmp, "0.parameters", false, &output);
        }
        if((filter->flags & AMXB_FLAG_FUNCTIONS) != 0) {
            amxb_ubus_fill_list(bus_ctx, object, &tmp, "0.functions", false, &output);
        }
        if((filter->flags & AMXB_FLAG_OBJECTS) != 0) {
            if(filter->flags & AMXB_FLAG_FIRST_LVL) {
                amxb_ubus_fill_list(bus_ctx, object, &tmp, "0.objects", false, &output);
            } else {
                subojects = GETP_ARG(&tmp, "0.objects");
            }
        }
    } else {
        if((filter->flags & AMXB_FLAG_INSTANCES) != 0) {
            if(filter->flags & AMXB_FLAG_FIRST_LVL) {
                amxb_ubus_fill_list(bus_ctx, object, &tmp, "0.instances", true, &output);
            } else {
                subojects = GETP_ARG(&tmp, "0.instances");
            }
        }
    }

    if(filter->request->cb_fn) {
        filter->request->cb_fn(bus_ctx, &output, filter->request->priv);
    }

    // remove all items, they are not needed anymore
    amxc_var_clean(&output);

    amxc_var_for_each(subobj, subojects) {
        amxc_string_setf(&path, "%s%s.", object, GET_CHAR(subobj, NULL));
        amxb_ubus_list_object_describe(bus_ctx, amxc_string_get(&path, 0), filter);
    }

exit:
    amxc_var_clean(&tmp);
    amxc_string_clean(&path);
    return rv;
}

static int amxb_ubus_add(const char* recv_path,
                         struct ubus_object_data* o,
                         ubus_filter_t* filter,
                         amxc_var_t* output,
                         bool functions) {
    int retval = -1;
    if(amxb_ubus_has_function(o, "_describe")) {
        amxc_var_add(cstring_t, &filter->amx_paths, recv_path);
    } else {
        amxc_var_add(cstring_t, output, recv_path);
        if(functions) {
            amxb_ubus_add_functions(o, filter, recv_path, output);
        }
        retval = 0;
    }

    return retval;
}

static void amxb_ubus_path_to_depth(amxd_path_t* path,
                                    ubus_filter_t* filter,
                                    uint32_t depth) {
    while(depth > filter->depth) {
        char* part = amxd_path_get_last(path, true);
        free(part);
        depth--;
    }
}

// In this callback function which is passed to ubus_lookup it is not allowed
// to send other requests to ubus.
// When doing this, the response of the ubus_lookup is often incomplete.
// Therefor when ambiorix objects are detected, they are added to a list
// and when the ubus_lookup has finished, the ambiorix objects are inspected
// using _describe object method.
static void amxb_ubus_objects_cb(AMXB_UNUSED struct ubus_context* c,
                                 struct ubus_object_data* o,
                                 void* p) {
    ubus_filter_t* filter = (ubus_filter_t*) p;
    const char* recv_path = NULL;
    amxd_path_t path;
    amxc_var_t retval;
    amxc_var_t tmp;
    uint32_t depth = 0;
    amxb_bus_ctx_t* bus_ctx = amxb_request_get_ctx(filter->request);
    bool functions = true;

    amxc_var_init(&retval);
    amxc_var_init(&tmp);
    amxc_var_set_type(&retval, AMXC_VAR_ID_LIST);
    amxd_path_init(&path, NULL);
    amxd_path_setf(&path, true, "%s", o->path);
    recv_path = amxd_path_get(&path, AMXD_OBJECT_TERMINATE);
    depth = amxd_path_get_depth(&path);

    if((filter->path_len != 0) &&
       (strncmp(recv_path, filter->path, filter->path_len) != 0)) {
        goto exit;
    }

    if(depth < filter->depth) {
        goto exit;
    }

    if(depth > filter->depth) {
        if((filter->flags & AMXB_FLAG_FIRST_LVL) != 0) {
            if(amxb_ubus_has_function(o, "_describe")) {
                amxb_ubus_path_to_depth(&path, filter, depth);
            } else {
                functions = false;
                amxb_ubus_path_to_depth(&path, filter, filter->path == NULL? depth:depth - 1);
            }
            recv_path = amxd_path_get(&path, AMXD_OBJECT_TERMINATE);
        }
    }

    if(GET_ARG(&filter->objects, recv_path) != NULL) {
        goto exit;
    }

    if(amxb_ubus_add(recv_path, o, filter, &retval, functions) == 0) {
        if(filter->request->cb_fn) {
            filter->request->cb_fn(bus_ctx, &retval, filter->request->priv);
        }
    }
    amxc_var_add_key(bool, &filter->objects, recv_path, true);

exit:
    amxc_var_clean(&tmp);
    amxc_var_clean(&retval);
    amxd_path_clean(&path);
}

static int amxb_ubus_list_native(amxb_ubus_t* amxb_ubus_ctx,
                                 const char* object,
                                 ubus_filter_t* filter) {
    int rv = 0;

    if(object[strlen(object) - 1] == '.') {
        amxc_string_t path;

        filter->path = object;
        filter->path_len = strlen(object);
        filter->depth = amxb_ubus_object_depth(object);

        amxc_string_init(&path, 0);
        amxc_string_setf(&path, "%s", object);
        amxc_string_set_at(&path, strlen(object) - 1, "*", 1, amxc_string_overwrite);
        rv = ubus_lookup(amxb_ubus_ctx->ubus_ctx,
                         amxc_string_get(&path, 0),
                         amxb_ubus_objects_cb,
                         filter);
        amxc_string_clean(&path);
    }

    return rv;
}

int amxb_ubus_list(void* const ctx,
                   const char* object,
                   uint32_t flags,
                   uint32_t access,
                   amxb_request_t* request) {
    amxb_bus_ctx_t* bus_ctx = NULL;
    amxb_ubus_t* amxb_ubus_ctx = (amxb_ubus_t*) ctx;
    int rv = -1;
    ubus_filter_t filter;

    memset(&filter, 0, sizeof(ubus_filter_t));
    filter.flags = flags;
    filter.access = access;
    filter.request = request;
    amxc_var_set_type(&filter.amx_paths, AMXC_VAR_ID_LIST);
    amxc_var_set_type(&filter.objects, AMXC_VAR_ID_HTABLE);

    bus_ctx = amxb_request_get_ctx(request);
    if((object == NULL) || (*object == 0)) {
        filter.depth = 1;
        filter.flags &= ~(AMXB_FLAG_OBJECTS | AMXB_FLAG_INSTANCES);
        rv = ubus_lookup(amxb_ubus_ctx->ubus_ctx, NULL, amxb_ubus_objects_cb, &filter);
        filter.flags &= ~(AMXB_FLAG_PARAMETERS | AMXB_FLAG_FUNCTIONS);
        amxc_var_for_each(path, &filter.amx_paths) {
            const char* p = GET_CHAR(path, NULL);
            amxb_ubus_list_object_describe(bus_ctx, p, &filter);
        }
        request->cb_fn(bus_ctx, NULL, request->priv);
    } else {
        filter.flags |= (AMXB_FLAG_OBJECTS | AMXB_FLAG_INSTANCES);
        rv = amxb_ubus_list_object_describe(bus_ctx, object, &filter);

        if(rv != 0) {
            filter.flags &= ~(AMXB_FLAG_OBJECTS | AMXB_FLAG_INSTANCES);
            rv = amxb_ubus_list_native(amxb_ubus_ctx, object, &filter);
            amxc_var_for_each(path, &filter.amx_paths) {
                const char* p = GET_CHAR(path, NULL);
                amxb_ubus_list_object_describe(bus_ctx, p, &filter);
            }
        }
        request->cb_fn(bus_ctx, NULL, request->priv);
    }

    amxc_var_clean(&filter.objects);
    amxc_var_clean(&filter.amx_paths);
    amxb_close_request(&request);
    return rv;
}
