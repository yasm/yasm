#!/usr/bin/env python

""" 

(c) 2002, 2003, 2004, 2005 Simon Burton <simon@arrowtheory.com>
Released under GNU LGPL license.

version 0.xx

"""


import sys
import os

from work_unit import WorkUnit, get_syms
import ir


def mk_tao(CPPFLAGS = "", CPP = None, modname = '_tao', oname = None, **options):
  if oname is None:
    oname = modname+'.pyx'
  PETSC_DIR = os.environ["PETSC_DIR"]
  PETSC_ARCH = os.environ["PETSC_ARCH"]
  TAO_DIR = os.environ["TAO_DIR"]
  files = [ 'ctao.h' ]

  taolibdir = TAO_DIR+'/lib/'+PETSC_ARCH
  syms = get_syms( ['ctao'], [taolibdir] )
  def cb(trans_unit, node, *args):
    name, file = node.name, node.file
    return name in syms
  extradefs = ""
  unit = WorkUnit(files,modname,oname,False,mark_cb=cb,extradefs=extradefs,
    CPPFLAGS=CPPFLAGS, CPP=CPP, **options)


  unit.parse( True )
  unit.transform(verbose=False, test_parse=False, test_types=False)
  unit.output()

def main():
  options = {}
  for i,arg in enumerate(sys.argv[1:]):
    if arg.count('='):
      key,val = arg.split('=')
      options[key]=val
  mk_tao(**options)

if __name__=="__main__":
  main()




