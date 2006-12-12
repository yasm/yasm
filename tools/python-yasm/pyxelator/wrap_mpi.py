#!/usr/bin/env python

""" 

(c) 2002, 2003, 2004, 2005 Simon Burton <simon@arrowtheory.com>
Released under GNU LGPL license.

version 0.xx

"""


import sys
import os

from work_unit import WorkUnit, get_syms

def mk_mpi():
  PETSC_DIR = os.environ["PETSC_DIR"]
  PETSC_ARCH = os.environ["PETSC_ARCH"]
  cflags = ""
#    for dirname in 'include', 'bmake/'+PETSC_ARCH, 'externalpackages/mpich2-1.0.2/src/include':
#      cflags = cflags + '-I%s/%s '%(PETSC_DIR,dirname)
  cflags = " ".join( sys.argv[1:] )
  files = [ 'mpi.h' ]

  mpi = PETSC_DIR + "/externalpackages/mpich2-1.0.2p1/"
  libmpi = PETSC_DIR + "/externalpackages/mpich2-1.0.2p1/lib/libmpich.a"

  syms = get_syms( libmpi )

  extradefs = """
"""
  def cb(node):
    name, file = node.name, node.file
#    if file.count('mpi'): return False
    if name and name.count('Fortran'): return False
#    if node.tagged and 'petsc' in node.file:
#      return True
    return name and ( name in syms or extradefs.count(name) )
  modname = '_mpi'
  oname = '%s.pyx'%modname
  unit = WorkUnit(files,modname,oname,cflags,False,mark_cb=cb,extradefs=extradefs)

  unit.parse( True )
  del unit.node[100:]
  unit.transform(verbose=False, test_parse=False, test_types=False)
  unit.output()

def mk1( cstrs ):
  names = {}
  for cstr in cstrs:
    name = 'tmp/cmdline.h' 
    f = open(name, 'w')
    f.write( cstr + ';\n' )
    f.close()
    files = [ name ]
    cflags = "-I. -Itmp"
    unit = WorkUnit(files,cflags,False)
    unit.parse( True )
    unit.transform()
  #  unit.pprint()
  #  for node in unit.node:
  #    print
  #    print node.deepstr()
  #    print "# basetype:"
  #    print "# ctype:"
  #    print node.cbasetype().deepstr()
  #    print node.ctype().deepstr()
  #    print
  #    print node.cstr()
  #    print
  #    print node.pyxstr()
    print '# '+unit.node.deepstr().replace('\n','\n# ')
    print unit.node.pyxstr(names=names)

if __name__=="__main__":
  try:
    import psyco
    psyco.full()
  except: 
    pass

#  if sys.argv[1:]:
##    cstr = " ".join( sys.argv[1:] )
#    mk1( sys.argv[1:] )
#  else:
#  mk_all()
  mk_mpi()






