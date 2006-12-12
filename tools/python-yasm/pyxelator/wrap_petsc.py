#!/usr/bin/env python

""" 

(c) 2002, 2003, 2004, 2005 Simon Burton <simon@arrowtheory.com>
Released under GNU LGPL license.

version 0.xx

"""

import sys
import os

from work_unit import WorkUnit, get_syms

import genpyx

def mk_petsc(CPPFLAGS = "", CPP = None, modname = '_petsc', oname = None, **options): 
  if oname is None:
    oname = modname+'.pyx'
  PETSC_DIR = os.environ["PETSC_DIR"]
  PETSC_ARCH = os.environ["PETSC_ARCH"]
#    files = [ 'petsc.h' ]
  files = [ 'petscblaslapack.h', 'petscvec.h', 'petscmat.h', 'petscksp.h' ]
  files += [ 'src/mat/matimpl.h', 'src/mat/impls/aij/seq/aij.h' ]

  syms = get_syms( 'petsc petscvec petscmat petscksp'.split(), ['%s/lib/%s/'%(PETSC_DIR,PETSC_ARCH)] )

# this may be a macro:
# PetscErrorCode VecRestoreArray(Vec x,PetscScalar *a[]);

# This function is defined (with body) in the header (!) as static inline:
#PetscErrorCode VecSetValue(Vec v,int row,PetscScalar value, InsertMode mode);

  extradefs = ''
  if len(files)>1:
    extradefs = """
PetscErrorCode VecRestoreArray(Vec x,PetscScalar *a[]);
void SETERRQ(PetscErrorCode errorcode,char *message);
void CHKERRQ(PetscErrorCode errorcode);
PetscErrorCode VecGetArray(Vec x,PetscScalar *a[]);
PetscErrorCode MatSetValue(Mat m,PetscInt row,PetscInt col,PetscScalar value,InsertMode mode);
void BLASgemm_(const char*,const char*,PetscBLASInt*,PetscBLASInt*,PetscBLASInt*,PetscScalar*,PetscScalar*,PetscBLASInt*,PetscScalar*,PetscBLASInt*,PetscScalar*,PetscScalar*,PetscBLASInt*);
void LAPACKgesvd_(char *,char *,PetscBLASInt*,PetscBLASInt*,PetscScalar *,PetscBLASInt*,PetscReal*,PetscScalar*,PetscBLASInt*,PetscScalar*,PetscBLASInt*,PetscScalar*,PetscBLASInt*,PetscBLASInt*);
void LAPACKsygv_(PetscBLASInt*,const char*,const char*,PetscBLASInt*,PetscScalar*,PetscBLASInt*,PetscScalar*,PetscBLASInt*,PetscScalar*,PetscScalar*,PetscBLASInt*,PetscBLASInt*);
PetscErrorCode PetscMalloc(size_t m,void **result);
PetscErrorCode VecsDestroy(Vecs x);
PetscErrorCode VecsCreateSeqWithArray(MPI_Comm comm, PetscInt p, PetscInt m, PetscScalar a[], Vecs *x);
PetscErrorCode VecsCreateSeq(MPI_Comm comm, PetscInt p, PetscInt m, Vecs *x);
PetscErrorCode VecsDuplicate(Vecs x, Vecs *y);
"""

#    files = ['petsc.h']
#    extradefs = ""

  assert syms
  def mark_cb(trans_unit, node, *args):
    " mark the nodes we want to keep "
    name, file = node.name, node.file
    if node.compound:
      # for now, we rip out all struct members
      node.compound[1:] = [] # XX encapsulation 
      return True
    if file.count('mpi'): return False
    if name and name.count('Fortran'): return False
    if name and name.count('PLAPACK'): return False # PetscPLAPACKInitializePackage etc.
#      if node.tagged and node.tagged.tag.name and node.tagged.tag.name.startswith('__'): return False
    if node.tagged and 'petsc' in node.file:
      return True # struct, union or enum
    return name and ( name in syms or extradefs.count(name) )
#  def func_cb(ostream, node, *args):
#    " build adaptors for functions "
#    return genpyx.FunctionAdaptor(ostream,node)
  unit = WorkUnit(files,modname,oname,False,mark_cb=mark_cb,extradefs=extradefs,
    CPPFLAGS=CPPFLAGS, CPP=CPP, **options)

  unit.parse( True )
  unit.transform(verbose=False, test_parse=False, test_types=False)
#  unit.output(func_cb=func_cb)
  unit.output()

def main():
  options = {}
  for i,arg in enumerate(sys.argv[1:]):
    if arg.count('='):
      key,val = arg.split('=')
      options[key]=val
  mk_petsc(**options)

if __name__=="__main__":
  if 'prof' in sys.argv:
    from profile import run
    import pstats
    run( "main()", "prof.out" )   
    stats = pstats.Stats("prof.out")
    stats.sort_stats('cumulative')
    stats.print_stats(60)
    print '-'*80,'\n\n\n'
    stats.print_callers(60)
    print '-'*80,'\n\n\n'
    stats.print_callees(60)
    print '-'*80,'\n\n\n'
  else:
    main()






