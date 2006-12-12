#!/usr/bin/env python
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

# Some helper routines from the Python API
cdef extern from "Python.h":
  # For parsing tuples
  int PyArg_ParseTuple(object args, char *format, ...)

  # To access tuples
  object PyTuple_New(int)
  int PyTuple_SetItem(object, int, object)
  object PyTuple_GetItem(object, int)
  object PyTuple_GET_ITEM(object, int)
  int PyTuple_Size(object tuple)

  int Py_DECREF(object)
  int Py_INCREF(object)
  
  # To access integers
#  object PyInt_FromLong(long)

  # To access double
  object PyFloat_FromDouble(double)
  
  # To access strings
  object PyString_FromStringAndSize(char *s, int len)
  char *PyString_AsString(object string)
  int PyString_AsStringAndSize(object obj, char **buffer, int *length)
  object PyString_FromString(char *)

  object PyBuffer_FromMemory(void *ptr, int size)
  object PyBuffer_FromReadWriteMemory(void *ptr, int size)

#cdef enum:
  #MAXDIM = 40

# Structs and functions from numarray
cdef extern from "numarray/numarray.h":
  #ctypedef Complex64

  ctypedef enum NumRequirements:
    NUM_CONTIGUOUS
    NUM_NOTSWAPPED
    NUM_ALIGNED
    NUM_WRITABLE
    NUM_C_ARRAY
    NUM_UNCONVERTED

  ctypedef enum NumarrayByteOrder:
    NUM_LITTLE_ENDIAN
    NUM_BIG_ENDIAN

  cdef enum:
    UNCONVERTED
    C_ARRAY

  ctypedef enum NumarrayType:
    tAny
    tBool	
    tInt8
    tUInt8
    tInt16
    tUInt16
    tInt32
    tUInt32
    tInt64
    tUInt64
    tFloat32
    tFloat64
    tComplex32
    tComplex64
    tObject
    tDefault
    tLong
  
  # Declaration for the PyArrayObject
  struct PyArray_Descr:
    int type_num, elsize
    char type

  #ctypedef class numarray.numarraycore.NumArray [object PyArrayObject]:
  ctypedef class numarray._numarray._numarray [object PyArrayObject]:
    # Compatibility with Numeric
    cdef char *data
    cdef int nd
    #cdef int dimensions[MAXDIM], strides[MAXDIM]
    cdef int *dimensions, *strides
    cdef object base
    cdef PyArray_Descr *descr
    cdef int flags
    # New attributes for numarray objects
    cdef object _data         # object must meet buffer API
    cdef object _shadows      # ill-behaved original array.
    cdef int    nstrides      # elements in strides array
    cdef long   byteoffset    # offset into buffer where array data begins
    cdef long   bytestride    # basic seperation of elements in bytes
    cdef long   itemsize      # length of 1 element in bytes
    cdef char   byteorder     # NUM_BIG_ENDIAN, NUM_LITTLE_ENDIAN
    cdef char   _aligned      # test override flag
    cdef char   _contiguous   # test override flag

# Functions from numarray API
cdef extern from "numarray/numarray.h":
  ctypedef int maybelong
cdef extern from "numarray/libnumarray.h":
  void import_libnumarray()
  object NA_NewAll(
    int ndim, maybelong* shape, NumarrayType type, void* buffer,
    maybelong byteoffset, maybelong bytestride, int byteorder, int aligned, int writeable )
  object NA_NewAllStrides(
    int ndim, maybelong* shape, maybelong* strides, NumarrayType type, void* buffer,
    int byteorder, int byteoffset, int aligned, int writeable )
  object NA_New( void* buffer, NumarrayType type, int ndim,... )
  object NA_Empty( int ndim, maybelong* shape, NumarrayType type )
  object NA_NewArray( void* buffer, NumarrayType type, int ndim, ... )
  object NA_vNewArray( void* buffer, NumarrayType type, int ndim, maybelong *shape )

  object NA_InputArray (object, NumarrayType, int)
  object NA_OutputArray (object, NumarrayType, int)
  object NA_IoArray (object, NumarrayType, int)
#  object PyArray_FromDimsAndData(int nd, int *dims, int type, char *data)
#  object PyArray_FromDims(int nd, int *d, int type)
  object NA_NewAllFromBuffer(int ndim, maybelong *shape, NumarrayType type, object buffer, maybelong byteoffset, maybelong bytestride, int byteorder, int aligned, int writable)
  object NA_updateDataPtr(object)
  object NA_getPythonScalar(object, long)
  object NA_setFromPythonScalar(object, int, object)
  #object NA_getPythonScalar(PyArrayObject, long)
  #object NA_setFromPythonScalar(PyArrayObject, int, PyArrayObject)

  object PyArray_ContiguousFromObject(object op, int type, int min_dim, int max_dim)
  void*NA_OFFSETDATA(_numarray a)
  int  NA_nameToTypeNo(char*)
  char*NA_typeNoToName(int typeno)
  object NA_typeNoToTypeObject(int typeno)
  int NA_ByteOrder()

import numarray
import_libnumarray()




