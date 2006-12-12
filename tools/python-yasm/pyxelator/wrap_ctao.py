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


tao_defs = """
typedef struct _p_TAO_SOLVER* TAO_SOLVER;
typedef struct _p_TAOAPPLICATION* TAO_APPLICATION;

typedef const char* TaoMethod;


extern int TAO_APP_COOKIE;


typedef void TaoApplication;
typedef void TaoIndexSet;
typedef void TaoIndexSetPetsc;
typedef void TaoLinearSolver;
typedef void TaoLinearSolverPetsc;
typedef void TaoMat;
typedef void TaoMatPetsc;
typedef void TaoVec;
typedef void TaoVecPetsc;


typedef enum {
              TAOMAT_SYMMETRIC_PSD=1,
              TAOMAT_SYMMETRIC=2,
              TAOMAT_UNSYMMETRIC=3 } TaoMatStructure;

typedef enum {
  TaoRedistributeSubset=0,
  TaoNoRedistributeSubset=2,
  TaoSingleProcessor=5,
  TaoMaskFullSpace=3} TaoPetscISType;


typedef enum { TAO_FALSE,TAO_TRUE } TaoTruth;

typedef double TaoScalar;

/*  Convergence flags.
    Be sure to check that these match the flags in 
    $TAO_DIR/include/finclude/tao_solver.h  
*/
typedef enum {/* converged */
              TAO_CONVERGED_ATOL          =  2, /* F < F_minabs */
              TAO_CONVERGED_RTOL          =  3, /* F < F_mintol*F_initial */
              TAO_CONVERGED_TRTOL         =  4, /* step size small */
              TAO_CONVERGED_MINF          =  5, /* grad F < grad F_min */
	      TAO_CONVERGED_USER          =  6, /* User defined */
              /* diverged */
              TAO_DIVERGED_MAXITS         = -2,  
              TAO_DIVERGED_NAN            = -4, 
              TAO_DIVERGED_MAXFCN         = -5,
              TAO_DIVERGED_LS_FAILURE     = -6,
              TAO_DIVERGED_TR_REDUCTION   = -7,
	      TAO_DIVERGED_USER           = -8, /* User defined */
              TAO_CONTINUE_ITERATING      =  0} TaoTerminateReason;
"""

def mk_ctao(cflags="",**options):
  PETSC_DIR = os.environ["PETSC_DIR"]
  PETSC_ARCH = os.environ["PETSC_ARCH"]
  TAO_DIR = os.environ["TAO_DIR"]
  files = [ 'pytao/include/tao.h' ]

  taolibdir = TAO_DIR+'/lib/'+PETSC_ARCH
  taolibs = [ taolibdir+'/'+name for name in os.listdir(taolibdir) if name.endswith('.so') ]
  print "taolibs", taolibs

  syms_dict = get_syms( *taolibs )
  syms = '\n'.join(syms_dict)
  open('syms.txt','w').write(syms)
  def cb(node):
    name, file = node.name, node.file
    return file and file.count('tao.h') and syms.count(name) and not name in syms_dict
  modname = '_tao'
  oname = 'pytao/%s.pyx'%modname # not used
  unit = WorkUnit(files,modname,oname,cflags,False,mark_cb=cb,**options)

  unit.parse( True )
  unit.transform(verbose=False, test_parse=False, test_types=False)
  node = unit.node
  ctoks = []
  htoks = []
  htoks.append( """
#ifdef __cplusplus
#define CTAO_EXTERN_C extern "C"
#else
#define CTAO_EXTERN_C extern
#endif
""")
  ctoks.append( '#include "tao.h"' )
  ctoks.append( '#include "petscksp.h"' )
  htoks.append( tao_defs )
  names = {}
  for node in unit.node:
    if node.function and node.name not in names:
      names[node.name] = None
      node.deeprm( ir.StorageClass('extern') )
      func = node.function
      for i, arg in enumerate(func.args):
        arg.name = "arg%s"%i
      cnode = node.clone()
      cnode.name = 'c' + node.name
      ctoks.append( 'extern "C" ' + cnode.cstr() )
      htoks.append( 'CTAO_EXTERN_C ' + cnode.cstr() + ';' )
      ctoks.append( '{return %s(%s);}' % ( node.name, ','.join([arg.name for arg in func.args]) ) )
#    else:
#      toks.append( node.cstr() + ';' )
  ofile = open('pytao/include/ctao.h','w')
  ofile.write( '\n'.join(htoks) + '\n' )
  ofile = open('pytao/include/ctao.c','w')
  ofile.write( '\n'.join(ctoks) + '\n' )

if __name__=="__main__":
  options = {}
  for i,arg in enumerate(sys.argv[1:]):
    if arg.count('='):
      key,val = arg.split('=')
      options[key]=val

  mk_ctao(**options)




