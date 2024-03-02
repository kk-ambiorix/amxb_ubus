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

typedef union _ubus_double {
    double d;
    uint64_t u64;
} ubus_double_t;

static void amxb_ubus_set_var(amxc_var_t* var,
                              struct blob_attr* attr,
                              void* data,
                              int len) {
    switch(blob_id(attr)) {
    case BLOBMSG_TYPE_INT8:
        amxc_var_set(int8_t, var, *(int8_t*) data);
        break;

    case BLOBMSG_TYPE_INT16:
        amxc_var_set(int16_t, var, (int16_t) be16_to_cpu(*(uint16_t*) data));
        break;

    case BLOBMSG_TYPE_INT32:
        amxc_var_set(int32_t, var, (int32_t) be32_to_cpu(*(uint32_t*) data));
        break;

    case BLOBMSG_TYPE_INT64:
        amxc_var_set(int64_t, var, (int64_t) be64_to_cpu(*(uint64_t*) data));
        break;

    case BLOBMSG_TYPE_DOUBLE: {
        ubus_double_t v;
        v.u64 = be64_to_cpu(*(uint64_t*) data);
        amxc_var_set(double, var, v.d);
    }
    break;

    case BLOBMSG_TYPE_STRING:
        amxc_var_set(cstring_t, var, (const char*) data);
        break;

    case BLOBMSG_TYPE_ARRAY:
        amxc_var_set_type(var, AMXC_VAR_ID_LIST);
        amxb_ubus_parse_blob_array(var, (struct blob_attr*) data, len);
        break;

    case BLOBMSG_TYPE_TABLE:
        amxc_var_set_type(var, AMXC_VAR_ID_HTABLE);
        amxb_ubus_parse_blob_table(var, (struct blob_attr*) data, len);
        break;

    default:
        break;
    }
}

int PRIVATE amxb_ubus_parse_blob(amxc_var_t* var,
                                 struct blob_attr* attr,
                                 bool table) {
    int len;
    int off = 0;
    void* data;

    if(!blobmsg_check_attr(attr, false)) {
        return 0;
    }

    if(table) {
        if(blobmsg_name(attr)[0] != 0) {
            var = amxc_var_add_new_key(var, blobmsg_name(attr));
            off++;
        } else {
            return 0;
        }
    }

    data = blobmsg_data(attr);
    len = blobmsg_data_len(attr);
    amxb_ubus_set_var(var, attr, data, len);

    return off + 1;
}

int PRIVATE amxb_ubus_parse_blob_table(amxc_var_t* var,
                                       struct blob_attr* attr,
                                       int len) {
    int rem = len;
    struct blob_attr* pos;

    __blob_for_each_attr(pos, attr, rem) {
        amxb_ubus_parse_blob(var, pos, true);
    }

    return 1;
}


int PRIVATE amxb_ubus_parse_blob_array(amxc_var_t* var,
                                       struct blob_attr* attr,
                                       int len) {
    int rem = len;
    struct blob_attr* pos;

    __blob_for_each_attr(pos, attr, rem) {
        amxc_var_t* item = amxc_var_add_new(var);
        amxb_ubus_parse_blob(item, pos, false);
    }

    return 1;
}

int PRIVATE amxb_ubus_format_blob(amxc_var_t* data,
                                  const char* key,
                                  struct blob_buf* b) {
    void* c;

    switch(amxc_var_type_of(data)) {
    case AMXC_VAR_ID_BOOL: {
        blobmsg_add_u8(b, key, (uint8_t) amxc_var_constcast(bool, data));
    }
    break;

    case AMXC_VAR_ID_INT8: {
        blobmsg_add_u8(b, key, (uint8_t) amxc_var_constcast(int8_t, data));
    }
    break;
    case AMXC_VAR_ID_UINT8: {
        if(amxc_var_constcast(uint8_t, data) > INT8_MAX) {
            blobmsg_add_u16(b, key, (uint16_t) amxc_var_constcast(uint8_t, data));
        } else {
            blobmsg_add_u8(b, key, (uint8_t) amxc_var_constcast(uint8_t, data));
        }
    }
    break;
    case AMXC_VAR_ID_INT16: {
        blobmsg_add_u16(b, key, (uint16_t) amxc_var_constcast(int16_t, data));
    }
    break;
    case AMXC_VAR_ID_UINT16: {
        if(amxc_var_constcast(uint16_t, data) > INT16_MAX) {
            blobmsg_add_u32(b, key, (uint32_t) amxc_var_constcast(uint16_t, data));
        } else {
            blobmsg_add_u16(b, key, (uint16_t) amxc_var_constcast(uint16_t, data));
        }
    }
    break;
    case AMXC_VAR_ID_INT32: {
        blobmsg_add_u32(b, key, (uint32_t) amxc_var_constcast(int32_t, data));
    }
    break;
    case AMXC_VAR_ID_UINT32: {
        if(amxc_var_constcast(uint32_t, data) > INT32_MAX) {
            blobmsg_add_u64(b, key, (uint64_t) amxc_var_constcast(uint32_t, data));
        } else {
            blobmsg_add_u32(b, key, (uint32_t) amxc_var_constcast(uint32_t, data));
        }
    }
    break;
    case AMXC_VAR_ID_INT64: {
        blobmsg_add_u64(b, key, (uint64_t) amxc_var_constcast(int64_t, data));
    }
    break;
    case AMXC_VAR_ID_UINT64: {
        blobmsg_add_u64(b, key, (uint64_t) amxc_var_constcast(uint64_t, data));
    }
    break;

    case AMXC_VAR_ID_HTABLE: {
        const amxc_htable_t* table = amxc_var_constcast(amxc_htable_t, data);
        c = blobmsg_open_table(b, key);
        amxb_ubus_format_blob_table(table, b);
        blobmsg_close_table(b, c);
    }
    break;

    case AMXC_VAR_ID_LIST: {
        const amxc_llist_t* list = amxc_var_constcast(amxc_llist_t, data);
        c = blobmsg_open_array(b, key);
        amxb_ubus_format_blob_array(list, b);
        blobmsg_close_array(b, c);
    }
    break;

    default: {
        char* string = amxc_var_dyncast(cstring_t, data);
        blobmsg_add_string(b, key, string == NULL ? "" : string);
        free(string);
    }
    break;
    }

    return 0;
}

int PRIVATE amxb_ubus_format_blob_table(const amxc_htable_t* table,
                                        struct blob_buf* b) {
    int retval = -1;

    amxc_htable_for_each(it, table) {
        const char* key = amxc_htable_it_get_key(it);
        amxc_var_t* data = amxc_var_from_htable_it(it);
        when_failed(amxb_ubus_format_blob(data, key, b), exit);
    }

    retval = 0;

exit:
    return retval;
}

int PRIVATE amxb_ubus_format_blob_array(const amxc_llist_t* list,
                                        struct blob_buf* b) {
    int retval = -1;

    amxc_llist_for_each(it, list) {
        amxc_var_t* data = amxc_var_from_llist_it(it);
        when_failed(amxb_ubus_format_blob(data, NULL, b), exit);
    }

    retval = 0;

exit:
    return retval;
}
