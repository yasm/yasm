# Copyright (c) 2005, National ICT Australia
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
# 
#   * Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.  
#   * Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the
#   distribution. 
#   * Neither the name of National ICT Australia nor the names of its
#   contributors may be used to endorse or promote products derived from
#   this software without specific prior written permission. 
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 

#cdef extern from *:
#  ctypedef unsigned int __c_size_t "size_t"

cdef extern from "string.h":
  void *__c_memcpy "memcpy" ( void*, void*, __c_size_t )
  int __c_memcmp "memcmp" ( void*, void*, __c_size_t )

# XX implement custom fast malloc (we do lots of little mallocs) XX
# ...profile...
cdef extern from "stdlib.h":
  void *malloc(__c_size_t)
  void *calloc(__c_size_t, __c_size_t)
  void free(void*)

cdef extern from "Python.h":
  ctypedef struct PyObject:
    int ob_refcnt
  void Py_INCREF(object o)
  void Py_DECREF(object o)
  object PyString_FromFormat(char *format, ...)
#  PyObject* PyObject_Call( PyObject *callable_object, PyObject *args, PyObject *kw)
  object PyObject_Call( object callable_object, object args, object kw)

cdef void *pyx_malloc( __c_size_t size ):
  return malloc(size)
cdef void *pyx_calloc( __c_size_t nmemb, __c_size_t size ):
  return calloc( nmemb, size )
cdef void pyx_free( void *p ):
  free(p)

cdef void *pyx_memcpy( void* tgt, void* src, __c_size_t n ):
  cdef void *res
#  print "pyx_memcpy", <long>tgt, <long>src, n
  res = __c_memcpy(tgt,src,n)
  return res

cdef int pyx_memcmp( void* tgt, void* src, __c_size_t n ):
  cdef int res
#  print "pyx_memcmp", <long>tgt, <long>src, n
  res = __c_memcmp(tgt,src,n)
  return res

#cdef __c_PetscErrorCode petsc_malloc(  __c_size_t a,int lineno,char *function,char *filename,char *dir,void**result):
#  result[0]=pyx_malloc( a )
#
#cdef __c_PetscErrorCode petsc_free( void *aa,int line,char *function,char *file,char *dir):
#  pyx_free(aa)
#
#__c_PetscSetMalloc( petsc_malloc, petsc_free )

  

###############################################################################
# XX we should mangle all these names a bit more ...
# XX or stick in a submodule ? 

#
# pyrex generates bad code for "unsigned long"
# which we would like to use as a pointer type
# so we just use "long".
#
# SystemError: ../Objects/longobject.c:240: bad argument to internal function:
# unsigned long PyLong_AsUnsignedLong(PyObject *vv)
#

cdef extern from "object.h":
  ctypedef class __builtin__.type [object PyHeapTypeObject]:
    pass

cdef class Meta(type)

# We want eg. CInt.pointer to give a class which is the type "pointer to CInt"
# so we inherit from above classes 
cdef class Meta(type):
  property array:
    def __get__( cls ):
      name = "CArray"+cls.__name__
      try:
        return globals()[name]
      except KeyError:
        tp = Meta( name, (CArray,), {} )
        tp.target = cls # class attribute
        tp.sizeof = None # array's don't have a size until they get a length (instantiated)
        globals()[name] = tp
        return tp
  property pointer:
    def __get__( cls ):
      name = "CPointer"+cls.__name__
      try:
        return globals()[name]
      except KeyError:
        tp = Meta( name, (CPointer,), {} )
        tp.target = cls # class attribute
        globals()[name] = tp
      return tp

def py2cobject( item ):
  if type(item) in (int,):
    return CInt(item) # really should be CLong 
  if type(item) == float:
    return CDouble(item)
  if type(item) == str:
    return CStr(item)

cdef class _CObject
#cdef public class _CObject [ object AdaptObj_CObject, type AdaptType_CObject ]

cdef class _CPointer(_CObject)
#cdef public class _CPointer(_CObject) [ object AdaptObj_CPointer, type AdaptType_CPointer ]

cdef class _CObject:
#cdef public class _CObject [ object AdaptObj_CObject, type AdaptType_CObject ]:
#  cdef void*p
#  cdef int malloced
#  cdef public object incref # incref what we are pointing to
  def __init__( self, value = None, addr = None ):
    """
      CObject( [value], [addr] ) 
        value : a compatible CObject or native python object
        addr : a memory location for a c-object
    """
    cdef long c_addr
    assert value is None
    if addr is not None:
      c_addr = addr
    else:
      c_addr = 0
    self.p = <void*>c_addr
    self.malloced = False
    self.incref = None
  def __int__( self ):
    # XX assumes we are pointing to a long
    cdef long *p
    assert self.p != NULL
    p = <long*>self.p
    return p[0]
  def get_value( self ): # == __int__
    # XX assumes we are pointing to a long
    cdef long *p
    assert self.p != NULL
    p = <long*>self.p
    return p[0]
  def __nonzero__( self ):
    return bool( self.get_value() )
  def set_value( self, value ):
    # XX assumes we are pointing to a long
    cdef long *p
    assert self.p != NULL
    p = <long*>self.p
    p[0] = value
  def clone( self ):
    return self.__class__( self )
  property addr:
    def __get__(self):
      cdef long c_addr
      cdef _CPointer p
      c_addr = <long>self.p
#      p = CPointer( value = c_addr )
      p = type(self).pointer( c_addr )
      p.incref = self # incref
      return p
  def cast_to( self, cls ):
    return cls( self )
  def get_basetype( self ):
    return _CObject
  def is_compatible( self, _CObject cobject ):
#    print "_CObject.is_compatible", type(cobject), self.basetype
    return isinstance( cobject, self.basetype )
  def __str__( self ):
    cdef long c_addr
    c_addr = <long>self.p
    val = ""
    if c_addr != 0:
      val = ", val = %s" % hex(self.get_value())
    return "%s( addr = %s%s )" % (self.__class__.__name__, hex(c_addr), val)
  def __repr__( self ):
    return str(self)
#  def __cmp__( _CObject self, _CObject other ): # XX it's not (self, other) XX
#    # XX assumes we are pointing to a long
#    cdef long *p1, *p2
#    assert self.p != NULL
#    p1 = <long*>self.p
#    assert other.p != NULL
#    p2 = <long*>other.p
#    print "__equ__", p1[0], p2[0]
#    return max(-1, min(1, p1[0] - p2[0]))
  def __cmp__( x, y ):
    print x.__class__, '__cmp__', y.__class__
    assert isinstance(x,_CObject) # x may not be self ... huh ?
    if not isinstance(y,_CObject):
      y = py2cobject(y)
    if x.sizeof == y.sizeof:
      res = x.addr.memcmp( y.addr, 1 )
    else:
      res = -1 # ?
  def __richcmp__( x, y, int op ):
#    print '__richcmp__', x.__class__, y.__class__, op
    cdef int cmp
    assert isinstance(x,_CObject) # x may not be self ... huh ?
    if not isinstance(y,_CObject):
      y = py2cobject(y)
      if y is None:
        # hmmm...
        if op in (0,3,4): return True
        else: return False
    if x.sizeof == y.sizeof:
      cmp = x.addr.memcmp( y.addr, 1 )
    else:
      cmp = -1 # ?
    if op == 0: # x<y
      return cmp<0
    elif op == 1: # x<=y
      return cmp<=0
    elif op == 2: # x==y
      return cmp==0
    elif op == 3: # x!=y
      return cmp!=0
    elif op == 4: # x>y
      return cmp>0
    elif op == 5: # x>=y
      return cmp>=0
    assert 0, op
  def __dealloc__( self ):
    if self.malloced:
#      print "__dealloc__", hex(<long>self.p)
      pyx_free( self.p )
#CObject = Meta( 'CObject', (_CObject,), {} )
CObject = _CObject
CVoid = Meta( 'CObject', (_CObject,), {} )
CVoid.basetype = CVoid

# more types:
include "_adapt.pxi"

cdef class _CPointer(_CObject):
#cdef public class _CPointer(_CObject) [ object AdaptObj_CPointer, type AdaptType_CPointer ]:
  def __init__( self, value = None, addr = None ):
    """
      CPointer( [value], [addr] )
        value : a CPointer
        addr : address of a c-pointer
    """
    cdef long c_value
    cdef void**p
    cdef long c_addr
    cdef _CObject cobject
    if addr is None:
      p = <void**>pyx_calloc(1,sizeof(void*))
      self.p = <void*>p
      if isinstance(value,_CObject):
        cobject = value
        if not self.is_compatible( value ):
          raise TypeError("%s cannot be set from %s" % (self.__class__, value))
        c_value = (<long*>cobject.p)[0]
      elif value is not None:
        c_value = value
      else:
        c_value = 0
#      print self.__class__.__name__, "__init__( value = ", hex(c_value), ")"
      p[0] = <void*>c_value
      self.malloced = True
    else:
      c_addr = addr
      self.p = <void*>c_addr
      self.malloced = False
  # XX make .target a property that sets el_size once and for all ? XX
#  def sizeof( self ):
#    return sizeof(void*)
  def memcpy( _CPointer self, _CPointer source, n_items ):
    " ob.memcpy( source, size ): copy <n_items> items of mem from <source> "
    assert self.get_value() != 0
    assert source.get_value() != 0
#    print "_CPointer.memcpy", self, source, n_items
    pyx_memcpy( (<void**>self.p)[0], (<void**>source.p)[0], n_items * self.target.sizeof )
  def memcmp( _CPointer self, _CPointer other, n_items ):
    " ob.memcmp( other, size ): compare <n_items> items of mem with <other> "
    assert self.get_value() != 0
    assert other.get_value() != 0
#    print "_CPointer.memcmp", self, other, n_items
    res = pyx_memcmp( (<void**>self.p)[0], (<void**>other.p)[0], n_items * self.target.sizeof )
    return res
  def __setitem__( self, idx, item ):
    item = self.target(item)
    p = self.target.pointer(self)
    p = p.__iadd__( idx )
    p.memcpy( item.addr, 1 )
  def __getitem__( self, idx ):
    item = self.target()
    p = self.target.pointer(self)
    p = p.__iadd__( idx )
    item.addr.memcpy( p, 1 )
    return item
  def is_compatible( self, _CObject cobject ):
#    if not type(tgt_class)==Meta:
#      tgt_class = type(tgt_class)
    src_class = self.__class__
    tgt_class = cobject.__class__
    assert src_class==_CObject or type(src_class)==Meta
    assert tgt_class==_CObject or type(tgt_class)==Meta
    while issubclass(src_class, _CPointer) and issubclass(tgt_class, _CPointer):
#      print "is_compatible", str(src_class), str(tgt_class)
      assert issubclass(src_class,_CPointer)
      assert issubclass(tgt_class,_CPointer)
      src_class = src_class.target
      tgt_class = tgt_class.target
#    print "is_compatible", str(src_class), str(tgt_class)
#    return _self.is_compatible(cobject) # isinstance(cobject, self.basetype)
    if tgt_class == CObject:
      res = True
    elif not issubclass(src_class, _CPointer):
      res = src_class().is_compatible( tgt_class() )
    else:
      res = tgt_class().is_compatible( src_class() )
#    print "is_compatible:", res
    return res
  def init_from( self, _CPointer cobject ):
    if not self.is_compatible(cobject):
      raise TypeError("%s cannot be set from %s" % (self.__class__, cobject))
    (<void**>self.p)[0] = (<void**>cobject.p)[0]
  def get_basetype( self ): # use an attribute ?
    return self.__class__
  def __int__( self ):
    cdef void**p
    cdef long c_addr
    c_addr = <long>( (<void**>self.p)[0] )
#    print "CPointer.__int__:", c_addr
    return c_addr
  property deref:
    def __get__( self ):
      cdef long**p
      p = <long**>self.p
      if hasattr(self,"target"):
        ob = self.target( addr = <long>p[0] ) # sets malloced=False
        ob.incref = self
      else:
        ob = CPointer( value = p[0][0] )
      return ob
  def __iadd__( self, other ):
    cdef long sz
    cdef _CPointer cobject
    cdef char *cp
    sz = self.target.sizeof * other
    cobject = self
    cp = (<char**>cobject.p)[0]
    cp = cp + sz
    (<char**>cobject.p)[0] = cp
    return self # hmmm, build a new one ?
  def __add__( _CPointer self, other ):
    cdef long c_addr
    assert int(other) == other, "can't add type %s to pointer (need integer type)"%type(other)
    c_addr = (<long*>self.p)[0]
    c_addr = c_addr + self.target.sizeof * other
    return self.__class__( value = c_addr )
  def __call__( self, *args ):
    ob = self.deref
    if isinstance(ob,CFunction):
      return ob(*args)
    raise TypeError( "%s not callable"%self )

CPointer = Meta( 'CPointer', (_CPointer,), {} )
CPointer.target = CObject
CPointer.basetype = CPointer
CPointer.sizeof = sizeof(void*)
null = CPointer( 0 )

cdef class _CArray(_CPointer):
#  cdef public __c_size_t el_size # sizeof what we point to # XX class attr ??
#  cdef __c_size_t nmemb
#  cdef void *items # == self.p (XX duplicate XX)
  def __init__( self, arg=None ): # XX addr ?
    " arg: init list or size "
    cdef long c_addr
    if arg is not None:
      if type(arg) in (int,long):
        nmemb = arg
        value = None
      else:
        nmemb = len(arg)
        value = arg
      el_size = self.target.sizeof
      self.items = <void*>pyx_calloc(nmemb, el_size)
      c_addr = <long>self.items
      _CPointer.__init__( self, value = c_addr )
      self.el_size = el_size
      self.nmemb = nmemb
  #    print "%s.__init__: nmemb=%s, el_size=%s, target=%s, sizeof(target)=%s"\
  #      % ( self.__class__.__name__, nmemb, el_size, self.target, self.target.sizeof )
      self.sizeof = self.el_size * nmemb
      if value is not None:
        for idx, item in enumerate(value):
          self[idx] = item
    else:
      self.items = NULL
#    print self
#  def sizeof( self ):
#    return self.el_size * nmemb # is this right ??
  def __len__( self ):
    return self.nmemb
  def __dealloc__( self ):
    if self.items != NULL:
      pyx_free( self.items )
#    CPointer.__dealloc__(self) # ??
  def __str__( self ):
    return "%s(%s)" % ( self.__class__.__name__, list(self) )
#  def is_compatible( self, _CObject cobject ):
#    # XX check array sizes
#    # right now this is all handled from _CPointer XX
CArray = Meta( 'CArray', (_CArray,), {} )
# CArray.basetype ... not used

def get_slice_size( int start, int stop, int step ):
  if stop<=start: return 0
  if (stop-start)%step == 0:
    return (stop-start)/step
  return (stop-start)/step + 1

cdef class _CArrayCIntRange(_CArray): # derive from CArrayCInt
  def __init__( self, int start, int stop, int step ):
    cdef int count
    cdef int *p
    cdef int nmemb
    nmemb = get_slice_size(start,stop,step) 
    p = <int*>pyx_malloc( nmemb * sizeof(int) )
    self.nmemb = nmemb
    count = 0
    while nmemb:
      p[count] = count + start
      count = count + 1
      nmemb = nmemb - 1
    self.items = p
    addr = <long>self.items
    _CPointer.__init__( self, value = addr )
    self.el_size = sizeof(int)
CArrayCIntRange = Meta( 'CArrayCIntRange', (_CArrayCIntRange,), {} )
CArrayCIntRange.target = CInt

class CStr(CChar.array):
  def __init__( self, s ):
    CChar.array.__init__( self, len(s)+1 )
    idx = 0
    for c in s:
      self[idx] = ord(c)
      idx = idx + 1
    self[idx] = 0
  def str( _CObject self ):
    if self.p != NULL:
      p = (<char**>self.p)[0]
    else:
      p = "<NULL>"
    return p


cdef class _CStruct(_CObject):
  pass
CStruct = Meta( 'CStruct', (_CStruct,), {} )
CStruct.basetype = CStruct

cdef class _CUnion(_CObject):
  pass
CUnion = Meta( 'CUnion', (_CUnion,), {} )
CUnion.basetype = CUnion

#cdef class _CEnum(_CInt):
#  pass
#CEnum = Meta( 'CEnum', (_CEnum,), {} )
CEnum = Meta( 'CEnum', (CInt,), {} )
CEnum.basetype = CInt


cdef class _CFunction(_CObject):
  " a callable pointer "
#  cdef void (*p)()
  cdef object arg_tps
  def __init__(self, *arg_tps):
    _CObject.__init__(self)
    self.arg_tps = arg_tps
  def check_args(self, *args):
    if len(self.arg_tps)!=len(args):
      raise TypeError("xxx")
    for i, arg in enumerate(args):
      # check types here
      arg_tp = self.arg_tps[i]
      if arg_tp is not None:
        pass
#  this is an abstract base class, subclasses define __call__:
#  def __call__(self, *args):
#    cdef c_arg0
#    ret = (<int (*)(int)>self.p)( c_arg0 )
#    return ret
CFunction = Meta( 'CFunction', (_CFunction,), {} )
CFunction.basetype = CFunction

cdef class _CFunctionThief(_CFunction):
  " this function steals references to python objects "
  cdef object refs
  def __init__(self, *arg_tps):
    _CFunction.__init__(self,*arg_tps)
    self.refs = {} # map pyfunc to incref'd tuple
  def hold( self, pyfunc, data ):
    data = (pyfunc, data)
    self.refs[pyfunc] = data # incref
    return data
  def release( self, pyfunc ):
    del self.refs[pyfunc] # decref
CFunctionThief = Meta( 'CFunctionThief', (_CFunctionThief,), {} )
CFunctionThief.basetype = CFunctionThief

def _build_function_cls( *sig ):
  # use '0', '1' as begin/end markers
  # use '_' as separator
  toks = []
  for tp in sig:
    toks.append( tp.__class__.__name__ )
  name = 'CFunction0' + '_'.join( toks ) + '1'
  try:
    return globals()[name]
  except KeyError:
    tp = Meta( name, (CFunction,), {} ) # we use the ordinary Meta
    tp._sig = sig # class attribute
    globals()[name] = tp
  return tp

cdef class FunctionMeta(Meta):
  property sig:
    def __get__( cls ):
      return _build_function_cls
CFunction = FunctionMeta( 'CFunction', (_CFunction,), {} )
CFunction.basetype = CFunction


#cdef class _CPyObject(_CPointer):
#CPyObject = Meta( 'CPyObject', (_CPyObject), {} )

class CPyObject(CPointer):
  def __init__( self, value=None, addr=None ):
    cdef void* p
    if value is not None:
      if not isinstance(value,_CObject):
        p = <void*>value
        Py_INCREF(value)
        CPointer.__init__( self, <long>p )
    else:
      CPointer.__init__( self, addr=addr )
  def __call__( _CPointer self, *args, **kw ):
    cdef object o, result
    cdef void *p
    p = (<void**>self.p)[0]
    o = <object>p
    result = PyObject_Call( o, args, kw )
    return result
  def get_ob( _CPointer self ):
    # get the actual python object
    cdef void *p
    p = (<void**>self.p)[0]
    o = <object>p
    return o
  ob = property(get_ob)
  def get_refcnt( _CPointer self ):
    cdef PyObject*p
    p = (<PyObject**>self.p)[0]
    assert p != NULL
    return p[0].ob_refcnt
  refcnt = property(get_refcnt)
#  def incref( _CPointer self ):
#    cdef PyObject*p
#    p = (<PyObject**>self.p)[0]
#    assert p != NULL
#    Py_INCREF(p)
#  def decref( _CPointer self ):
#    cdef PyObject*p
#    p = (<PyObject**>self.p)[0]
#    assert p != NULL
#    Py_DECREF(p)
  def incref( _CPointer self ):
    cdef void *p
    p = (<void**>self.p)[0]
    o = <object>p
    Py_INCREF(o)
  def decref( _CPointer self ):
    cdef void *p
    p = (<void**>self.p)[0]
    o = <object>p
    Py_DECREF(o)
  def __del__( _CPointer self ):
    self.decref()

import sys
__module__ = sys.modules[__name__]

def __sizeof(cls):
  return cls.sizeof
__module__.sizeof=__sizeof

###############################################################################
# XX mangle these c names ?

cdef cobject_from_pointer( void *p ):
  cdef long addr
  addr = <long>p
  ob = CPointer( value = addr )
  return ob

cdef void *pointer_from_cobject( _CObject ob ):
  return ob.p

cdef char *get_charp( ob ):
  cdef _CObject _ob
  cdef char*p
  if type(ob)==str:
    p = ob
  elif type(ob) in (int,long):
    assert ob==0
    p = <char*>ob
  else:
    _ob = ob
    p = (<char**>_ob.p)[0]
#  print "get_charp", ob, hex(<long>p)
  return p

def str_from_charp( _CPointer ob ):
  cdef char*p
  p = (<char**>ob.p)[0]
  return p



