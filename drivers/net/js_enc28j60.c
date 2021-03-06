/* Copyright (c) 2013, Eyal Birger
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * The name of the author may not be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "js/js_obj.h"
#include "js/js_utils.h"
#include "js/js_event.h"
#include "js/jsapi_decl.h"
#include "net/js_netif.h"
#include "drivers/net/enc28j60.h"
#include "boards/board.h"

int do_enc28j60_constructor(obj_t **ret, obj_t *this, int argc, obj_t *argv[])
{
    enc28j60_params_t params;
    const enc28j60_params_t *p = &params;
    netif_t *netif;

    /* XXX: ideally, we should receive an object with optional override info */
    if (argc == 1)
        p = &board.enc28j60_params;
    else if (argc != 4)
        return js_invalid_args(ret);
    else
    {
        params.spi_port = obj_get_int(argv[1]);
        params.cs = obj_get_int(argv[2]);
        params.intr = obj_get_int(argv[3]);
    }

    netif = enc28j60_new(p);
    return netif_obj_constructor(netif, ret, this, argc, argv);
}
