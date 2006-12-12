#!/usr/bin/env python

import adapt
from adapt import *

tps = [ 
  CChar,
  CSChar,
  CUChar,
  CShort,
  CUShort,
  CInt,
  CUInt,
  CLong,
  CULong,
  CLLong,
  CULLong,
  CFloat,
  CDouble,
  CLDouble,
]


a = CLLong(1234567890123456789)
print a
print repr(a)
print int(a)
print float(a)
print

a = CUInt(a)
print a
print repr(a)
print int(a)
print float(a)
print

a = CSChar(a)
print a
print repr(a)
print int(a)
print float(a)
print

a = CChar(ord('@'))
print a
print repr(a)
print int(a)
print float(a)
print

a = CFloat(123456)
print a
print repr(a)
print int(a)
print float(a)
print

print dir(adapt)
mem = adapt._CObject()
#print mem.malloced
mem.incref = None

mem = a.addr
print mem
print mem.incref
b = mem.deref
print b



