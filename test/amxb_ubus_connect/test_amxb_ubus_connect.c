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
#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <setjmp.h>
#include <cmocka.h>
#include <sys/stat.h>
#include <unistd.h>

#include <amxc/amxc_variant.h>
#include <amxc/amxc_htable.h>
#include <amxc/amxc_lqueue.h>

#include <amxp/amxp_signal.h>
#include <amxp/amxp_slot.h>

#include <amxd/amxd_dm.h>

#include <amxb/amxb_error.h>
#include <amxb/amxb.h>

#include "amxb_ubus.h"
#include "test_amxb_ubus_connect.h"

#include <amxc/amxc_macros.h>
static amxp_signal_mngr_t* sigmngr = NULL;

static const char* test_amxb_ubus_get_socket(void) {
    struct stat sb;
    if(stat("/var/run/ubus.sock", &sb) == 0) {
        return "/var/run/ubus.sock";
    }
    if(stat("/var/run/ubus/ubus.sock", &sb) == 0) {
        return "/var/run/ubus/ubus.sock";
    }
    return NULL;
}

void test_amxb_ubus_connect_disconnect(UNUSED void** state) {
    void* bus_ctx1 = NULL;
    void* bus_ctx2 = NULL;
    const char* ubus_sock = test_amxb_ubus_get_socket();
    printf("Ubus socket = %s\n", ubus_sock);

    amxp_sigmngr_new(&sigmngr);

    bus_ctx1 = amxb_ubus_connect(NULL, NULL, NULL, sigmngr);
    assert_ptr_not_equal(bus_ctx1, NULL);
    assert_int_equal(amxb_ubus_disconnect(bus_ctx1), 0);
    amxb_ubus_free(bus_ctx1);

    bus_ctx1 = amxb_ubus_connect("", NULL, NULL, sigmngr);
    assert_ptr_equal(bus_ctx1, NULL);

    bus_ctx1 = amxb_ubus_connect("", "", NULL, sigmngr);
    assert_ptr_equal(bus_ctx1, NULL);

    bus_ctx1 = amxb_ubus_connect("localhost", NULL, NULL, sigmngr);
    assert_ptr_equal(bus_ctx1, NULL);

    bus_ctx1 = amxb_ubus_connect("localhost", "", NULL, sigmngr);
    assert_ptr_equal(bus_ctx1, NULL);

    bus_ctx1 = amxb_ubus_connect("localhost", "1970", NULL, sigmngr);
    assert_ptr_equal(bus_ctx1, NULL);

    bus_ctx1 = amxb_ubus_connect(NULL, NULL, ubus_sock, sigmngr);
    assert_ptr_not_equal(bus_ctx1, NULL);

    bus_ctx2 = amxb_ubus_connect("localhost", NULL, NULL, sigmngr);
    assert_ptr_equal(bus_ctx2, NULL);

    bus_ctx2 = amxb_ubus_connect(NULL, NULL, ubus_sock, sigmngr);
    assert_ptr_not_equal(bus_ctx2, NULL);

    assert_ptr_not_equal(bus_ctx1, bus_ctx2);

    assert_int_equal(amxb_ubus_disconnect(bus_ctx1), 0);
    assert_int_equal(amxb_ubus_disconnect(bus_ctx2), 0);
    amxb_ubus_free(bus_ctx1);
    amxb_ubus_free(bus_ctx2);
    amxp_sigmngr_delete(&sigmngr);
}

void test_amxb_ubus_get_fd(UNUSED void** state) {
    void* bus_ctx = NULL;
    amxp_sigmngr_new(&sigmngr);
    const char* ubus_sock = test_amxb_ubus_get_socket();

    bus_ctx = amxb_ubus_connect(NULL, NULL, ubus_sock, sigmngr);
    assert_true(amxb_ubus_get_fd(bus_ctx) > -1);
    assert_int_equal(amxb_ubus_disconnect(bus_ctx), 0);
    amxb_ubus_free(bus_ctx);

    amxp_sigmngr_delete(&sigmngr);
}

void test_amxb_info(UNUSED void** state) {
    amxb_be_info_t* info = NULL;

    info = amxb_be_info();

    assert_ptr_not_equal(info, NULL);
    assert_ptr_not_equal(info->min_supported, NULL);
    assert_ptr_not_equal(info->max_supported, NULL);
    assert_ptr_not_equal(info->be_version, NULL);
    assert_ptr_not_equal(info->name, NULL);
}
