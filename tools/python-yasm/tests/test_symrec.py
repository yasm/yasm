# $Id$
from tests import TestCase, add
from yasm import SymbolTable

class TSymbolTable(TestCase):
    def test_keys(self):
        symtab = SymbolTable()
        self.failUnlessEqual(len(symtab.keys()), 0)
        symtab.declare("foo")
        keys = symtab.keys()
        self.failUnlessEqual(len(keys), 1)
        self.failUnlessEqual(keys[0], "foo")
