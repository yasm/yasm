/* $IdPath$
 * YASM optimizer module interface header file
 *
 *  Copyright (C) 2001  Peter Johnson
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
#ifndef YASM_OPTIMIZER_H
#define YASM_OPTIMIZER_H

/** Version number of #yasm_optimizer interface.  Any functional change to the
 * #yasm_optimizer interface should simultaneously increment this number.  This
 * version should be checked by #yasm_optimizer loaders to verify that the
 * expected version (the version defined by its libyasm header files) matches
 * the loaded module version (the version defined by the module's libyasm
 * header files).  Doing this will ensure that the module version's function
 * definitions match the module loader's function definitions.  The version
 * number must never be decreased.
 */
#define YASM_OPTIMIZER_VERSION	0

/* Interface to the optimizer module(s) */
struct yasm_optimizer {
    /** Version (see #YASM_OPTIMIZER_VERSION).  Should always be set to
     * #YASM_OPTIMIZER_VERSION by the module source and checked against
     * #YASM_OPTIMIZER_VERSION by the module loader.
     */
    unsigned int version;

    /* one-line description of the optimizer */
    const char *name;

    /* keyword used to select optimizer on the command line */
    const char *keyword;

    /* Main entrance point for the optimizer.
     *
     * This function takes the unoptimized linked list of sections and
     * optimizes it.  If successful, the sections are ready for output to an
     * object file.  (A failure is indicated by calling ErrorAt() from within
     * this function).
     */
    void (*optimize) (yasm_sectionhead *sections);
};

#endif
