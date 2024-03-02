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
#include <amxd/amxd_object.h>
#include <amxd/amxd_transaction.h>
#include <amxo/amxo.h>

#include <amxb/amxb.h>
#include <amxb/amxb_register.h>

#include "test_amxb_ubus_register.h"

static amxb_bus_ctx_t* bus_ctx = NULL;
static amxd_dm_t dm;
static amxo_parser_t parser;

static void handle_events(void) {
    printf("Handling events ");
    while(amxp_signal_read() == 0) {
        printf(".");
    }
    printf("\n");
}

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

int test_amxb_ubus_register_setup(UNUSED void** state) {
    const char* ubus_sock = test_amxb_ubus_get_socket();
    printf("Ubus socket = %s\n", ubus_sock);

    assert_int_equal(amxd_dm_init(&dm), 0);
    assert_int_equal(amxo_parser_init(&parser), 0);

    assert_int_equal(amxb_be_load("../mod-amxb-test-ubus.so"), 0);
    assert_int_equal(amxb_connect(&bus_ctx, ubus_sock), 0);

    amxo_parser_parse_file(&parser, "../test_data/test_nemo.odl", amxd_dm_get_root(&dm));

    return 0;
}

int test_amxb_ubus_register_teardown(UNUSED void** state) {
    amxd_dm_clean(&dm);
    amxo_parser_clean(&parser);

    amxb_disconnect(bus_ctx);
    amxb_free(&bus_ctx);

    amxb_be_remove_all();

    return 0;
}

void test_ubus_can_register_dm(UNUSED void** state) {
    assert_int_equal(amxb_register(bus_ctx, &dm), 0);
}

void test_ubus_new_objects_are_registered(UNUSED void** state) {
    amxd_trans_t transaction;

    assert_int_equal(amxd_trans_init(&transaction), 0);
    assert_int_equal(amxd_trans_select_pathf(&transaction, "NeMo.Intf."), 0);
    assert_int_equal(amxd_trans_add_inst(&transaction, 99, "Inst1"), 0);
    assert_int_equal(amxd_trans_apply(&transaction, &dm), 0);

    handle_events();

    assert_int_equal(system("ubus list NeMo.Intf.99"), 0);

    amxd_trans_clean(&transaction);
}

void test_ubus_new_root_objects_are_registered(UNUSED void** state) {
    amxd_object_t* object = NULL;

    assert_int_equal(amxd_object_new(&object, amxd_object_singleton, "test_object"), 0);
    assert_ptr_not_equal(object, NULL);
    assert_int_equal(amxd_dm_add_root_object(&dm, object), 0);

    handle_events();

    assert_int_equal(system("ubus list test_object"), 0);
}

void test_ubus_removed_objects_are_unregistered(UNUSED void** state) {
    amxd_trans_t transaction;

    assert_int_equal(amxd_trans_init(&transaction), 0);
    assert_int_equal(amxd_trans_select_pathf(&transaction, "NeMo.Intf."), 0);
    assert_int_equal(amxd_trans_del_inst(&transaction, 99, "Inst1"), 0);
    assert_int_equal(amxd_trans_apply(&transaction, &dm), 0);

    handle_events();

    assert_int_not_equal(system("ubus list NeMo.Intf.99"), 0);

    amxd_trans_clean(&transaction);
}
