# $Id$
from tests import TestCase, add
from yasm import Bytecode, ImmVal, Expression

class TImmVal(TestCase):
    def test_create(self):
        self.assertRaises(TypeError, ImmVal, "notimmval")

        imm = ImmVal(Expression('+', 2, 3))

add(TImmVal)
