# $Id$
from tests import TestCase, add
from yasm import SymbolTable

class TSymbolTable(TestCase):
    def setUp(self):
        self.symtab = SymbolTable()
    def test_keys(self):
        self.assertEquals(len(self.symtab.keys()), 0)
        self.symtab.declare("foo", None, 0)
        keys = self.symtab.keys()
        self.assertEquals(len(keys), 1)
        self.assertEquals(keys[0], "foo")
add(TSymbolTable)

class TSymbolAttr(TestCase):
    def setUp(self):
        self.symtab = SymbolTable()
        self.declsym = self.symtab.declare("foo", None, 0)

    def test_visibility(self):
        sym = self.symtab.declare("local1", None, 0)
        self.assertEquals(sym.visibility, set())
        sym = self.symtab.declare("local2", '', 0)
        self.assertEquals(sym.visibility, set())
        sym = self.symtab.declare("local3", 'local', 0)
        self.assertEquals(sym.visibility, set())
        sym = self.symtab.declare("global", 'global', 0)
        self.assertEquals(sym.visibility, set(['global']))
        sym = self.symtab.declare("common", 'common', 0)
        self.assertEquals(sym.visibility, set(['common']))
        sym = self.symtab.declare("extern", 'extern', 0)
        self.assertEquals(sym.visibility, set(['extern']))
        sym = self.symtab.declare("dlocal", 'dlocal', 0)
        self.assertEquals(sym.visibility, set(['dlocal']))

        self.assertRaises(ValueError,
                          lambda: self.symtab.declare("extern2", 'foo', 0))
    def test_name(self):
        self.assertEquals(self.declsym.name, "foo")

    def test_equ(self):
        self.assertRaises(AttributeError, lambda: self.declsym.equ)

    def test_label(self):
        self.assertRaises(AttributeError, lambda: self.declsym.label)

    def test_is_special(self):
        self.assertEquals(self.declsym.is_special, False)

    def test_is_curpos(self):
        self.assertEquals(self.declsym.is_curpos, False)

add(TSymbolAttr)
