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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <signal.h>

#include <amxc/amxc.h>
#include <amxp/amxp.h>

#include <amxd/amxd_dm.h>

#include <amxb/amxb.h>

#include "test_amxb_ubus_list.h"

static amxb_bus_ctx_t* bus_ctx = NULL;

static const char* test_amxb_ubus_get_socket(void) {
    struct stat sb;
    if(stat("/var/run/ubus.sock", &sb) == 0) {
        return "ubus:/var/run/ubus.sock";
    }
    if(stat("/var/run/ubus/ubus.sock", &sb) == 0) {
        return "ubus:/var/run/ubus/ubus.sock";
    }
    return NULL;
}

int test_amxb_ubus_list_setup(UNUSED void** state) {
    amxc_string_t txt;
    const char* ubus_sock = test_amxb_ubus_get_socket();
    printf("Ubus socket = %s\n", ubus_sock);

    amxc_string_init(&txt, 0);

    amxc_string_reset(&txt);
    amxc_string_setf(&txt, "amxrt -u ubus: -B ../mod-amxb-test-ubus.so -A ../test_data/test_nemo.odl &");
    system(amxc_string_get(&txt, 0));

    // for some reason launching a native ubus application using the ubus
    // lua bingdings causes a segfault in the gitlab-ci pipeline test job.
    // so disable this for now
    //amxc_string_setf(&txt, "lua5.1 ../test_data/publisher.lua > test.txt &");
    //system(amxc_string_get(&txt, 0));

    amxc_string_clean(&txt);

    assert_int_equal(amxb_be_load("../mod-amxb-test-ubus.so"), 0);
    assert_int_equal(amxb_connect(&bus_ctx, ubus_sock), 0);

    sleep(1);

    return 0;
}

int test_amxb_ubus_list_teardown(UNUSED void** state) {
    amxb_disconnect(bus_ctx);
    amxb_free(&bus_ctx);

    system("killall amxrt");
    system("killall lua5.1");

    amxb_be_remove_all();
    unlink("test_config.odl");

    return 0;
}

static void test_list_cb(UNUSED const amxb_bus_ctx_t* bus_ctx,
                         UNUSED const amxc_var_t* const data,
                         UNUSED void* priv) {
}

void test_ubus_can_list_amx_object(UNUSED void** state) {
    int all_flags = AMXB_FLAG_FUNCTIONS | AMXB_FLAG_PARAMETERS | AMXB_FLAG_OBJECTS | AMXB_FLAG_INSTANCES;
    assert_int_equal(amxb_list(bus_ctx, "NeMo.Intf.", all_flags, test_list_cb, NULL), AMXB_STATUS_OK);
    assert_int_equal(amxb_list(bus_ctx, "NeMo.Intf", all_flags, test_list_cb, NULL), AMXB_STATUS_OK);
    assert_int_equal(amxb_list(bus_ctx, "NeMo.Intf.", all_flags | AMXB_FLAG_FIRST_LVL, test_list_cb, NULL), AMXB_STATUS_OK);
    assert_int_equal(amxb_list(bus_ctx, NULL, all_flags, test_list_cb, NULL), AMXB_STATUS_OK);
    assert_int_equal(amxb_list(bus_ctx, "", all_flags, test_list_cb, NULL), AMXB_STATUS_OK);

    // enable this two lines as soon as the problem with native ubus app using lua bindings is solved
    //assert_int_equal(amxb_list(bus_ctx, "com.", all_flags | AMXB_FLAG_FIRST_LVL, test_list_cb, NULL), AMXB_STATUS_OK);
    //assert_int_equal(amxb_list(bus_ctx, "com.broadcom.gpon_md", all_flags | AMXB_FLAG_FIRST_LVL, test_list_cb, NULL), AMXB_STATUS_OK);
}
