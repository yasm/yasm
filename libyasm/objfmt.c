/*
 * Object format interface
 *
 *  Copyright (C) 2003  Peter Johnson
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND OTHER CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR OTHER CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#define YASM_LIB_INTERNAL
#define YASM_ARCH_INTERNAL
#include "util.h"
/*@unused@*/ RCSID("$IdPath$");

#include "coretype.h"
#include "valparam.h"

#include "objfmt.h"


yasm_section *
yasm_objfmt_add_default_section(yasm_objfmt *objfmt, yasm_object *object)
{
    yasm_objfmt_base *of_base = (yasm_objfmt_base *)objfmt;
    yasm_section *retval;
    yasm_valparamhead vps;
    yasm_valparam *vp;

    vp = yasm_vp_create(yasm__xstrdup(of_base->module->default_section_name),
			NULL);
    yasm_vps_initialize(&vps);
    yasm_vps_append(&vps, vp);
    retval = yasm_objfmt_section_switch(objfmt, &vps, NULL, 0);
    yasm_vps_delete(&vps);

    return retval;
}
