/* $IdPath$
 * YASM module loader header file
 *
 *  Copyright (C) 2002  Peter Johnson
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
#ifndef YASM_MODULE_H
#define YASM_MODULE_H

void unload_modules(void);
/*@dependent@*/ /*@null@*/ void *get_module_data(const char *keyword,
						 const char *symbol);

#define load_arch(keyword)	get_module_data(keyword, "arch")
#define load_dbgfmt(keyword)	get_module_data(keyword, "dbgfmt")
#define load_objfmt(keyword)	get_module_data(keyword, "objfmt")
#define load_optimizer(keyword)	get_module_data(keyword, "optimizer")
#define load_parser(keyword)	get_module_data(keyword, "parser")
#define load_preproc(keyword)	get_module_data(keyword, "preproc")

#endif
