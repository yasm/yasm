# $Id: Makefile,v 1.5 2001/05/30 23:35:55 peter Exp $
# Makefile
#
#    Copyright (C) 2001  Peter Johnson
#
#    This file is part of YASM.
#
#    YASM is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    YASM is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program; if not, write to the Free Software
#    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#
# TODO:
#  This currently requires GNU Make.
#  Should use autoconf/automake.
#  Distribution target (eventually :).
#
CFLAGS = -g -Wall -Wstrict-prototypes -ansi -pedantic -Iinclude

OBJS_BASE = \
	bison.tab.o \
	lex.yy.o \
	symrec.o \
	bytecode.o \
	errwarn.o \
	main.o \
	$E
OBJS = $(addprefix obj/, $(OBJS_BASE))

.PHONY: all clean

all: yasm

yasm: $(OBJS)
	gcc -o $@ $(OBJS)

obj/%.o: src/%.c
	gcc $(CFLAGS) -c $< -o $@

src/bison.tab.c: src/bison.y
	bison -d -t $< -o $@
	mv src/bison.tab.h include/

src/lex.yy.c: src/token.l
	flex -o$@ $<

src/bison.y src/token.l: src/instrs.dat src/token.l.in src/bison.y.in src/gen_instr.pl
	src/gen_instr.pl -i src/instrs.dat -t src/token.l -g src/bison.y

clean:
	rm -f obj/*.o
	rm -f src/bison.tab.c
	rm -f include/bison.tab.h
	rm -f src/lex.yy.c
	rm -f src/bison.y src/token.l

