/* $IdPath$
 * Preprocessor module interface header file
 *
 *  Copyright (C) 2001  Peter Johnson
 *
 *  This file is part of YASM.
 *
 *  YASM is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  YASM is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef YASM_PREPROC_H
#define YASM_PREPROC_H

/* Interface to the preprocesor module(s) */
struct preproc {
    /* one-line description of the preprocessor */
    const char *name;

    /* keyword used to select preprocessor on the command line */
    const char *keyword;

    /* Initializes preprocessor.
     *
     * The preprocessor needs access to the object format module to find out
     * any output format specific macros.
     *
     * This function also takes the FILE * to the initial starting file, but
     * not the filename (which is in a global variable and is not
     * preprocessor-specific).
     */
    void (*initialize) (FILE *f);

    /* Gets more preprocessed source code (up to max_size bytes) into buf.
     * Note that more than a single line may be returned in buf. */
    size_t (*input) (char *buf, size_t max_size);
};

/* Available preprocessors */
extern preproc raw_preproc;

#endif
