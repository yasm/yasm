# $Id$
if HAVE_PYTHON

PYBINDING_DEPS  = tools/python-yasm/bytecode.pxi
PYBINDING_DEPS += tools/python-yasm/coretype.pxi
PYBINDING_DEPS += tools/python-yasm/expr.pxi
PYBINDING_DEPS += tools/python-yasm/floatnum.pxi
PYBINDING_DEPS += tools/python-yasm/intnum.pxi
PYBINDING_DEPS += tools/python-yasm/symrec.pxi
PYBINDING_DEPS += tools/python-yasm/value.pxi

EXTRA_DIST += tools/python-yasm/setup.py
EXTRA_DIST += tools/python-yasm/yasm.pyx
EXTRA_DIST += $(PYBINDING_DEPS)

yasm_python.c: $(srcdir)/tools/python-yasm/yasm.pyx $(PYBINDING_DEPS)
	$(PYTHON) -c "from Pyrex.Compiler.Main import main; main(command_line=1)" \
		-o $@ `test -f tools/python-yasm/yasm.pyx || echo '$(srcdir)/'`tools/python-yasm/yasm.pyx

CLEANFILES += yasm_python.c

# Now the Python build magic...
python-setup.txt: Makefile
	echo "includes=${DEFAULT_INCLUDES} ${INCLUDES} ${DEFS}" > python-setup.txt
	echo "sources=${libyasm_a_SOURCES}" >> python-setup.txt
	echo "srcdir=${srcdir}" >> python-setup.txt
	echo "gcc=${GCC}" >> python-setup.txt

CLEANFILES += python-setup.txt

.python-build: python-setup.txt yasm_python.c ${libyasm_a_SOURCES}
	$(PYTHON) `test -f tools/python-yasm/setup.py || echo '$(srcdir)/'`tools/python-yasm/setup.py build
	touch .python-build
python-build: .python-build

CLEANFILES += .python-build

python-install: .python-build
	$(PYTHON) `test -f tools/python-yasm/setup.py || echo '$(srcdir)/'`tools/python-yasm/setup.py install "--install-lib=$(pythondir)"

python-uninstall:
	rm -f `$(PYTHON) -c "import sys;sys.path.insert(0, '${pythondir}'); import yasm; print yasm.__file__"`

else

python-build:
python-install:
python-uninstall:

endif
