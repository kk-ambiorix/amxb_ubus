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

#include <stdlib.h>

#include "amxb_ubus.h"

static int amxb_ubus_receive_notification(struct ubus_context* ctx,
                                          AMXB_UNUSED struct ubus_object* obj,
                                          AMXB_UNUSED struct ubus_request_data* req,
                                          const char* method,
                                          struct blob_attr* msg) {
    int status = UBUS_STATUS_UNKNOWN_ERROR;
    struct ubus_subscriber* s = NULL;
    amxb_ubus_t* amxb_ubus_ctx = NULL;
    amxb_ubus_sub_t* amxb_ubus_sub = NULL;
    amxc_htable_t* subscribers = NULL;
    amxc_var_t notification;
    const char* subscriber_object = NULL;

    amxc_var_init(&notification);
    amxc_var_set_type(&notification, AMXC_VAR_ID_HTABLE);

    s = container_of(obj, struct ubus_subscriber, obj);
    amxb_ubus_sub = container_of(s, amxb_ubus_sub_t, sub);
    subscribers = container_of(amxb_ubus_sub->it.ait->array, amxc_htable_t, table);
    amxb_ubus_ctx = container_of(subscribers, amxb_ubus_t, subscribers);

    subscriber_object = amxc_htable_it_get_key(&amxb_ubus_sub->it);
    when_true(amxb_ubus_ctx->ubus_ctx != ctx, exit);

    amxb_ubus_parse_blob_table(&notification, (struct blob_attr*) blob_data(msg), blob_len(msg));
    amxc_var_add_key(cstring_t, &notification, "notification", method);

    amxp_sigmngr_emit_signal(amxb_ubus_ctx->sigmngr, subscriber_object, &notification);

    status = UBUS_STATUS_OK;

exit:
    amxc_var_clean(&notification);
    return status;
}

int PRIVATE amxb_ubus_subscribe(void* const ctx,
                                const char* object) {
    amxb_ubus_t* amxb_ubus_ctx = (amxb_ubus_t*) ctx;
    amxc_htable_it_t* it = NULL;
    amxb_ubus_sub_t* amxb_ubus_sub = NULL;
    amxc_string_t rel_path;
    amxd_path_t path;
    int ret = -1;
    uint32_t id = 0;

    amxc_string_init(&rel_path, 64);
    amxd_path_init(&path, NULL);
    amxd_path_setf(&path, true, "%s", object);

    when_failed(amxb_ubus_get_longest_path(amxb_ubus_ctx, &path, &rel_path), exit);

    ret = ubus_lookup_id(amxb_ubus_ctx->ubus_ctx, amxd_path_get(&path, 0), &id);
    when_true(ret != 0, exit);

    it = amxc_htable_get(&amxb_ubus_ctx->subscribers, amxd_path_get(&path, 0));
    when_not_null(it, exit);

    amxb_ubus_sub = (amxb_ubus_sub_t*) calloc(1, sizeof(amxb_ubus_sub_t));
    amxc_htable_insert(&amxb_ubus_ctx->subscribers,
                       object,
                       &amxb_ubus_sub->it);

    ret = ubus_register_subscriber(amxb_ubus_ctx->ubus_ctx,
                                   &amxb_ubus_sub->sub);
    when_true(ret != 0, exit);
    amxb_ubus_sub->sub.cb = amxb_ubus_receive_notification;

    ret = ubus_subscribe(amxb_ubus_ctx->ubus_ctx, &amxb_ubus_sub->sub, id);

exit:
    amxc_string_clean(&rel_path);
    amxd_path_clean(&path);
    return ret;
}

int PRIVATE amxb_ubus_unsubscribe(void* const ctx,
                                  const char* object) {
    amxb_ubus_t* amxb_ubus_ctx = (amxb_ubus_t*) ctx;
    amxb_ubus_sub_t* amxb_ubus_sub = NULL;
    amxc_htable_it_t* it = NULL;
    amxc_string_t rel_path;
    amxd_path_t path;
    int ret = 0;
    uint32_t id = 0;

    amxc_string_init(&rel_path, 64);
    amxd_path_init(&path, NULL);
    amxd_path_setf(&path, true, "%s", object);

    amxb_ubus_get_longest_path(amxb_ubus_ctx, &path, &rel_path);

    it = amxc_htable_get(&amxb_ubus_ctx->subscribers, object);
    when_null(it, leave);
    amxb_ubus_sub = amxc_htable_it_get_data(it, amxb_ubus_sub_t, it);

    ret = ubus_lookup_id(amxb_ubus_ctx->ubus_ctx, amxd_path_get(&path, 0), &id);
    when_true(ret != 0, exit);

    ubus_unsubscribe(amxb_ubus_ctx->ubus_ctx, &amxb_ubus_sub->sub, id);

exit:
    ret = ubus_unregister_subscriber(amxb_ubus_ctx->ubus_ctx, &amxb_ubus_sub->sub);

    amxc_htable_it_clean(it, NULL);
    amxp_timer_delete(&amxb_ubus_sub->reactivate);
    free(amxb_ubus_sub);

leave:
    amxc_string_clean(&rel_path);
    amxd_path_clean(&path);

    return ret;
}

