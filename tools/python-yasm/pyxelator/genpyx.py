#!/usr/bin/env python
""" genpyx.py - parse c declarations

(c) 2002, 2003, 2004, 2005 Simon Burton <simon@arrowtheory.com>
Released under GNU LGPL license.

version 0.xx

This is a module of mixin classes for ir.py .

Towards the end of ir.py our global class definitions
are remapped to point to the class definitions in ir.py .
So, for example, when we refer to Node we get ir.Node .

"""

import sys
from datetime import datetime
from sets import Set

from dump import dump

# XX use this Context class instead of all those kw dicts !! XX
class Context(object):
  " just a record (struct) "
  def __init__( self, **kw ):
    for key, value in kw.items():
      setattr( self, key, value )
  def __getattr__( self, name ):
    return None # ?
  def __getitem__( self, name ):
    return getattr(self, name)

class OStream(object):
  def __init__( self, filename=None ):
    self.filename = filename
    self.tokens = []
    self._indent = 0
  def put( self, token="" ):
    assert type(token) is str
    self.tokens.append( token )
  def startln( self, token="" ):
    assert type(token) is str
    self.tokens.append( '  '*self._indent + token )
  def putln( self, ln="" ):
    assert type(ln) is str
    self.tokens.append( '  '*self._indent + ln + '\n')
  def endln( self, token="" ):
    assert type(token) is str
    self.tokens.append( token + '\n')
  def indent( self ):
    self._indent += 1
  def dedent( self ):
    self._indent -= 1
    assert self._indent >= 0, self._indent
  def join( self ):
    return ''.join( self.tokens )
  def close( self ):
    s = ''.join( self.tokens )
    f = open( self.filename, 'w' )
    f.write(s)

#
###############################################################################
#

class Node(object):
  """
    tree structure
  """
  _unique_id = 0
  def get_unique_id(cls):
    Node._unique_id += 1
    return Node._unique_id
  get_unique_id = classmethod(get_unique_id)

# XX toks: use a tree of tokens: a list that can be push'ed and pop'ed XX
  def pyxstr(self,toks=None,indent=0,**kw):
    """
      Build a list of tokens; return the joined tokens string
    """
#    dump( "self.__class__.__name__,toks" )
    if toks is None:
      toks = []
    for x in self:
      if isinstance(x,Node):
        x.pyxstr(toks, indent, **kw)
      else:
        toks.insert(0,str(x)+' ')
    s = ''.join(toks)
#    dump( "self.__class__.__name__,s" )
    return s

  def pyx_wrapper(self, tp_tank, ostream=None, **kw):
    if ostream is None:
      ostream = OStream()
    for node in self:
      if isinstance(node, Node):
        ostream = node.pyx_wrapper( tp_tank, ostream=ostream, **kw )
    return ostream

#
#################################################

class Named(object):
  "has a .name property"
  pass

class BasicType(object):
  "float double void char int"
  pass

class Qualifier(object):
  "register signed unsigned short long const volatile inline"
  def pyxstr(self,toks=None,indent=0,**kw):
    if toks is None:
      toks = []
    x = self[0]
    if x not in ( 'const','volatile','inline','register'): # ignore these
      toks.insert(0,str(x)+' ')
    s = ''.join(toks)
    return s

class StorageClass(object):
  "extern static auto"
  def pyxstr(self,toks=None,indent=0,**kw):
    return ""

class Ellipses(object):
  "..."
  pass

class GCCBuiltin(BasicType):
  "things with __builtin prefix"
  pass

class Identifier(object):
  """
  """
  def pyxstr(self,toks=None,indent=0,**kw):
    if toks is None:
      toks=[]
    if self.name:
      toks.append( self.name )
    return " ".join(toks)

class TypeAlias(object):
  """
   typedefed things, eg. size_t 
  """
  def pyxstr(self,toks=None,indent=0,cprefix="",**kw):
#    dump( "self.__class__.__name__,toks" )
    if toks is None:
      toks = []
    for x in self:
      if isinstance(x,Node):
        x.pyxstr(toks, indent, cprefix=cprefix, **kw)
      else:
        s = str(x)+' '
        if cprefix:
          s = cprefix+s
        toks.insert(0,s)
    s = ''.join(toks)
#    dump( "self.__class__.__name__,s" )
    return s

class Function(object):
  """
  """
  def pyxstr(self,toks,indent=0,**kw):
    #print '%s.pyxstr(%s)'%(self,toks)
    _toks=[]
    assert len(self)
    i=0
    while isinstance(self[i],Declarator):
      if not self[i].is_void():
        _toks.append( self[i].pyxstr(indent=indent, **kw) )
      i=i+1
    toks.append( '(%s)'% ', '.join(_toks) )
    while i<len(self):
      self[i].pyxstr(toks, indent=indent, **kw)
      i=i+1
    return " ".join(toks)

class Pointer(object):
  """
  """
  def pyxstr(self,toks,indent=0,**kw):
    assert len(self)
    node=self[0]
    toks.insert(0,'*')
    if isinstance(node,Function):
      toks.insert(0,'(')
      toks.append(')')
    elif isinstance(node,Array):
      toks.insert(0,'(')
      toks.append(')')
    return Node.pyxstr(self,toks,indent, **kw)

class Array(object):
  """
  """
  def pyxstr(self,toks,indent=0,**kw):
    if self.size is None:
      toks.append('[]')
    else:
      try:
        int(self.size)
        toks.append('[%s]'%self.size)
      except:
        toks.append('[]')
    return Node( *self[:-1] ).pyxstr( toks,indent, **kw )

class Tag(object):
  " the tag of a Struct, Union or Enum "
  pass

class Taged(object):
  "Struct, Union or Enum "
  pass

class Compound(Taged):
  "Struct or Union"
  def pyxstr(self,_toks=None,indent=0,cprefix="",shadow_name=True,**kw):
    if _toks is None:
      _toks=[]
    names = kw.get('names',{})
    kw['names'] = names
    tag_lookup = kw.get('tag_lookup')
    if self.tag:
      tag=self.tag.name
    else:
      tag = ''
    if isinstance(self,Struct):
      descr = 'struct'
    elif isinstance(self,Union):
      descr = 'union'
    _node = names.get(self.tag.name,None)
    if ( _node is not None and _node.has_members() ) or \
        ( _node is not None and not self.has_members() ):
      descr = '' # i am not defining myself here
#    print "Compound.pyxstr", tag
#    print self.deepstr()
    if descr:
      if cprefix and shadow_name:
        tag = '%s%s "%s"'%(cprefix,tag,tag)
      elif cprefix:
        tag = cprefix+tag
      toks = [ descr+' '+tag ] # struct foo
      if self.has_members():
        toks.append(':\n')
        for decl in self[1:]: # XX self.members
          toks.append( decl.pyxstr(indent=indent+1, cprefix=cprefix, shadow_name=shadow_name, **kw)+"\n" ) # shadow_name = False ?
      elif not tag_lookup.get( self.tag.name, self ).has_members():
        # define empty struct here, it's the best we're gonna get
        toks.append(':\n')
        toks.append( "  "*(indent+1) + "pass\n" )
    else:
      if cprefix: # and shadow_name:
        tag = cprefix+tag
      toks = [ ' '+tag+' ' ] # foo
    while toks:
      _toks.insert( 0, toks.pop() )
    return "".join( _toks )

class Struct(Compound):
  """
  """
  def pyx_wrapper(self, tp_tank, ostream=None, **kw):
    cobjects = kw['cobjects']
    ostream.putln( '# ' + self.cstr() )
    name = self.tag and self.tag.name
    if name and not name in cobjects:
      ostream.putln("%s = Meta( '%s', (CStruct,), {} )" % ( name, name ))
      cobjects[name] = self
#    ostream.putln( self.deepstr(comment=True) )
    return ostream

class Union(Compound):
  """
  """
  pass


class Enum(Taged):
  """
  """
  def pyxstr(self,_toks=None,indent=0,cprefix="",shadow_name=True,**kw):
    if _toks is None:
      _toks=[]
    names = kw.get('names',{})
    kw['names'] = names
    if self.tag:
      tag=self.tag.name
    else:
      tag = ''
    _node = names.get(self.tag.name,None)
    if ( _node is not None and _node.has_members() ) or \
        ( _node is not None and not self.has_members() ):
      descr = '' # i am not defining myself here
    else:
      descr = 'enum'
    if descr:
#    if not names.has_key(self.tag.name):
      toks = [ descr+' '+tag ] # enum foo
      toks.append(':\n')
      idents = [ ident for ident in self.members if ident.name not in names ]
      for ident in idents:
        if cprefix and shadow_name:
          ident = ident.clone()
          ident.name = '%s%s "%s"' % ( cprefix, ident.name, ident.name )
#        else: assert 0
        toks.append( '  '+'  '*indent + ident.pyxstr(**kw)+",\n" )
        names[ ident.name ] = ident
      if not idents:
        # empty enum def'n !
  #      assert 0 # should be handled by parents...
        toks.append( '  '+'  '*indent + "pass\n" )
    else:
      toks = [ ' '+tag+' ' ] # foo
    while toks:
      _toks.insert( 0, toks.pop() )
    return "".join( _toks )

#CChar char
#CSChar signed char
#CUChar unsigned char
#CShort short
#CUShort unsigned short
#CInt int
#CUInt unsigned int
#CLong long
#CULong unsigned long
#CLLong long long
#CULLong unsigned long long
#CFloat float
#CDouble double
#CLDouble long double 
class TPSet(object):
  char_tps = 'CChar CSChar CUChar'.split()
  short_tps = 'CShort CUShort'.split()
  int_tps = 'CInt CUInt CShort CUShort CLong CULong CLLong CULLong'.split()
  signed_tps = 'CSChar CShort CInt CLong CLLong'.split()
  unsigned_tps = 'CUInt CUChar CUShort CULong CULLong'.split()
  long_tps = 'CLong CULong CLLong CULLong CLDouble'.split()
  long_long_tps = 'CLLong CULLong'.split()
  float_tps = ['CFloat']
  double_tps = 'CDouble CLDouble'.split()
  void_tps = ['CVoid']
  all_tps = 'CVoid CChar CSChar CUChar CShort CUShort CInt CUInt CLong CULong CLLong CULLong CFloat CDouble CLDouble'.split()
  def __init__( self ):
    self.tps = self.all_tps
    self.toks = []
  def intersect( self, tps ):
#    print '   ', self.tps, tps
    self.tps = [ tp for tp in tps if tp in self.tps ]
#    print '   ', self.tps
  def parse( self, tok ):
    attr = '%s_tps' % tok
    if hasattr( self, attr ):
      self.intersect( getattr(self,attr) )
    if tok == 'long' and 'long' in self.toks:
      self.intersect( self.long_long_tps )
    self.toks.append(tok)
#    if not self.tps:
#      return 'CObject'
    return self.tps[0]

class VarAdaptor(object):
  """
    An adaptor generates code for converting between a python variable
  and a c variable.
  """
  def __init__( self, ostream, py_varname = None, c_varname = None, node = None, cobjects = None ):
    self.ostream = ostream
    self.py_varname = py_varname
    self.c_varname = c_varname
#    node = node.clone()
    self.onode = node.clone() # original node
    node = Declarator().init_from( node.clone() )
    if c_varname is not None:
      node.name = c_varname
    # turn array's into pointer's :
    for _node in node.nodes(): # bottom-up search
      for idx in range(len(_node)):
        if isinstance(_node[idx],Array):
          _node[idx] = _node[idx].to_pointer()
          node.invalidate() # so that we recompute cbasetype
    self.node = node
    self.bt = node.cbasetype() # WARNING: cbasetype may be cached
    self.cobjects = cobjects
    assert type(cobjects) is dict, repr(cobjects)
  def declare_py_arg( self ):
    if self.node.is_pyxnative():
      self.ostream.put( self.py_varname )
    elif self.node.is_pointer_to_char():
      self.ostream.put( self.py_varname )
    else:
      self.ostream.put( "%s %s" % (self.node.pyx_adaptor_decl(self.cobjects), self.py_varname) )
  def declare_cvar(self, names, tag_lookup, cprefix=""):
    if not self.node.is_void():
      self.ostream.putln( self.node.pyxstr( indent=0, names=names, tag_lookup=tag_lookup, cprefix=cprefix, shadow_name=False ) )
#    self.ostream.putln( "" )
  def declare_pyvar(self): #, names, tag_lookup, cprefix=""):
    if not self.node.is_void() and not self.node.is_pyxnative():
      s = 'cdef %s %s' % ( self.node.pyx_adaptor_decl(self.cobjects), self.py_varname )
      self.ostream.putln( s )
#    self.ostream.putln( "" )
  def get_c_cast(self, names, tag_lookup, cprefix=""):
    # doh! node is already a pointer !! so use onode:
    node = self.onode.clone()
    node.name = ""
    cast = node.pyxstr(names=names,tag_lookup=tag_lookup, use_cdef = False, cprefix=cprefix, shadow_name=False)
    return "<%s>"%cast
  def get_c_type(self, names, tag_lookup, cprefix=""):
    # doh! node is already a pointer !! so use onode:
    node = self.onode.clone()
    node.name = ""
    tp = node.pyxstr(names=names,tag_lookup=tag_lookup, use_cdef = False, cprefix=cprefix, shadow_name=False)
    return tp
  def get_c_from_pyvar(self, names, tag_lookup, cprefix=""):
    assert names != None
    assert tag_lookup != None
    if self.node.is_pyxnative():
      s = self.py_varname
    elif self.node.is_pointer_to_char():
      s = "get_charp(%s)" % self.py_varname
    else:
      p_arg = self.node.pointer_to()
      cast = p_arg.pyxstr(names=names,tag_lookup=tag_lookup, use_cdef = False, cprefix=cprefix, shadow_name=False)
      s = "(<%s>(%s.p))[0]" % ( cast, self.py_varname )
      if s.count('cdef'):
        print p_arg.deepstr()
        assert 0
    return s
  def assign_to_compound_member(self, compound_adaptor, names, tag_lookup, cprefix=""):
    assert names != None
    assert tag_lookup != None
    if self.node.is_pyxnative() or self.node.is_pointer_to_char():
      if self.node.is_pyxnative():
        rhs = self.py_varname
      else:
        rhs = "get_charp(%s)" % self.py_varname
      self.ostream.putln( "(<%s%s*>(%s.p))[0].%s = %s" %\
        ( cprefix, compound_adaptor.node.name, compound_adaptor.py_varname,
          self.onode.name, rhs ) )
    else:
      p_arg = self.node.pointer_to()
      cast = p_arg.pyxstr(names=names,tag_lookup=tag_lookup, use_cdef = False, cprefix=cprefix, shadow_name=False)
      rhs = "(<%s>(%s.p))" % ( cast, self.py_varname )
      self.ostream.putln( "%smemcpy( &((<%s%s*>(%s.p))[0].%s), %s, sizeof(%s) )" %\
        ( cprefix, cprefix, compound_adaptor.node.name, compound_adaptor.py_varname, self.onode.name,
          rhs,
          self.get_c_type(names, tag_lookup, cprefix),
        ) )
  def assign_from_compound_member(self, compound_adaptor, names, tag_lookup, cprefix=""):
    assert names != None
    assert tag_lookup != None
    if self.node.is_pyxnative():
      self.ostream.putln( "%s = (<%s%s*>(%s.p))[0].%s" %\
        ( 
          self.py_varname, cprefix, compound_adaptor.node.name, compound_adaptor.py_varname,
          self.onode.name,
        ) )
    else:
      p_arg = self.node.pointer_to()
      cast = p_arg.pyxstr(names=names,tag_lookup=tag_lookup, use_cdef = False, cprefix=cprefix, shadow_name=False)
      s = "(<%s>(%s.p))[0]" % ( cast, self.py_varname )
      self.ostream.putln( "%s = (<%s%s*>(%s.p))[0].%s" %\
        ( s,
          cprefix, compound_adaptor.node.name, compound_adaptor.py_varname,
          self.onode.name ))
  def get_c_arg(self):
    return self.c_varname
  def assign_to_cvar(self, names, tag_lookup, cprefix=""):
    s = "%s = %s" % ( self.c_varname, self.get_c_from_pyvar( names, tag_lookup, cprefix ) )
    self.ostream.putln( s )
  def assign_cvar_from( self, s ):
    if not self.node.is_void():
      s = '%s = %s' % ( self.c_varname, s )
    self.ostream.putln( s )
#  def assign_pyvar_to( self, s ):
#    s = "%s = %s" % ( s, 
  def init_pyvar( self, cobjects ):
    self.ostream.putln('%s = %s()' % ( self.py_varname, self.node.pyx_adaptor_name(cobjects) ) )

  def assign_to_pyvar( self, cobjects, cprefix ):
    if self.bt.is_pyxnative(): # warning: char* assumed null terminated
      self.ostream.putln('%s = %s'%(self.py_varname, self.c_varname))
    elif not self.bt.is_void():
#      self.ostream.putln('# ' + ret.cstr() )
#      self.ostream.putln('%s = %s()' % ( self.py_varname, self.node.pyx_adaptor_name(cobjects) ) )
      self.init_pyvar( cobjects )
#      self.ostream.putln('%smemcpy( ret.p, &c_ret, ret.sizeof )'%cprefix)
      self.ostream.putln('%smemcpy( %s.p, &%s, %s.sizeof )'%(cprefix, self.py_varname, self.c_varname, self.py_varname ))
  def return_pyvar( self ):
    if not self.bt.is_void():
      self.ostream.putln('return %s' % self.py_varname)
  def return_cvar( self ):
    if not self.bt.is_void():
      self.ostream.putln('return %s' % self.c_varname)

class CallbackAdaptor(object):
  """ Emit code for generating c-function (cdef) callback.
  """
  def __init__(self, ostream, node, name):
    self.ostream = ostream
    self.node = node # the function declaration
    self.name = name # what we should call ourself
  def define_func(self, names, tag_lookup, cprefix="", cobjects=None, **kw):
    decl = self.node.deref() # clone
    function = decl.function

#    self.ostream.putln()
#    self.ostream.putln('#'+self.node.cstr())
#    self.ostream.putln('#'+decl.cstr())
    arg_adaptors = []
    decl.name = self.name
    if not function.deepfind(Ellipses):

      # build adaptors for function arguments
      for i, arg in enumerate(function.args):
        arg.name = "c_arg%s"%i
        py_varname = "arg%s"%i
        adaptor = VarAdaptor( self.ostream, py_varname=py_varname, c_varname=arg.name, node=arg, cobjects=cobjects )
        arg_adaptors.append( adaptor )
      ret_adaptor = VarAdaptor( self.ostream, py_varname='ret', c_varname='c_ret', node=function.ret, cobjects=cobjects )

      # declare function
      self.ostream.putln( decl.pyxstr(indent=0, cprefix=cprefix, shadow_name=False, **kw ) + ': # callback' )
      self.ostream.indent()
      self.ostream.putln( '"%s"'%decl.cstr() )
#      self.ostream.putln( "  ob = <object>c_arg%s" % i ) # the last arg is a PyObject*

      # declare local c variables
      for adaptor in arg_adaptors:
        adaptor.declare_pyvar()
      ret_adaptor.declare_cvar(names=names, tag_lookup=tag_lookup, cprefix=cprefix)

      # convert to python types
      for adaptor in arg_adaptors:
        adaptor.assign_to_pyvar( cobjects, cprefix )
      ret_adaptor.declare_pyvar()

      # call python function
      args = [adaptor.py_varname for adaptor in arg_adaptors]
#      for i, arg in enumerate(function.args[:-1]): # skip the last arg
#      for i, arg in enumerate(function.args):
#        if arg.is_pyxnative(): # use it directly
#          args.append('c_arg%s'%i)
#        else:
#          args.append('arg%s'%i)
#          self.ostream.putln( "arg%s = CPointer( value = <long>c_arg%s )" % (i, i) )
      self.ostream.putln( "# manually add code for finding ob here #")
      ret_adaptor.assign_cvar_from( "ob(%s)" % ','.join(args) )
      ret_adaptor.assign_to_pyvar( cobjects=cobjects, cprefix=cprefix )

      # return c variable
      ret_adaptor.return_cvar()
      self.ostream.dedent()
    return self.ostream

#class CallbackArgAdaptor(object):
#  """
#    Generate code for callback arguments.
#    These objects actually consists of two Adaptor's.
#  """
#  def __init__( self, ostream, func_adaptor, data_adaptor ):
#    self.ostream = ostream
#    self.func_adaptor = func_adaptor
#    self.data_adaptor = data_adaptor
#  def declare_py_arg( self ):
##    self.func_adaptor.declare_py_arg()
#    self.ostream.put( self.func_adaptor.py_varname )
#    self.ostream.put(', ')
##    self.data_adaptor.declare_py_arg()
#    self.ostream.put( self.data_adaptor.py_varname )
#  def declare_cvar(self, names, tag_lookup, cprefix=""):
#    self.func_adaptor.declare_cvar(names, tag_lookup, cprefix)
#    self.data_adaptor.declare_cvar(names, tag_lookup, cprefix)
#  def get_c_arg(self):
#    return "%s,%s"%(self.func_adaptor.get_c_arg(),self.data_adaptor.get_c_arg())
#  def assign_to_cvar(self, names, tag_lookup, cprefix=""):
#    # the c callback argument comes from a fixed c callback function
#    cbname = self.func_adaptor.onode.get_pyxtpname() # use the onode
#    self.ostream.putln( '%s = %s' % ( self.func_adaptor.c_varname, cbname ) )
#    # the void* data argument is the python callable
##    self.ostream.putln( '%s = <void*>%s' % ( self.data_adaptor.c_varname, self.data_adaptor.py_varname ) )
#    self.ostream.putln( '%s = <void*>%s' % ( self.data_adaptor.c_varname, self.func_adaptor.py_varname ) )

class CallbackArgAdaptor(VarAdaptor):
  def declare_cvar(self,*args,**kw):
    pass
  def assign_to_cvar(self,*args,**kw):
    pass

class FunctionAdaptor(object):
  def __init__(self, ostream, node, special_stream):
    self.ostream = ostream
    self.node = node # the function declaration
    self.special_stream = special_stream
  def define_func(self, names=None, tag_lookup=None, cprefix="", cobjects=None, **kw):
    # emit 'def' fn that calls this function
    # XX how to call varargs functions ? va_args functions ?

    # first we remove any void argument:
    function = self.node.function
    if len(function.args)==1 and function.args[0].is_void():
      function = function.clone()
      function.pop(0) # XX encapsulation

    ostream = self.ostream
    # check if there are function pointer args:
    for arg in function.args:
      if arg.is_pointer_to_fn():
        ostream = self.special_stream # these are special, to be hand edited

    arg_adaptors = []
    i = 0
    while i < len(function.args):
      arg = function.args[i]
#      # callback hackery:
#      if arg.is_callback() and (i+1<len(function.args)) \
#          and (function.args[i+1].pointer) \
#          and (function.args[i+1].deref().is_void()):
#        py_varname = 'arg%s'%i
#        func_adaptor = VarAdaptor(
#          ostream, py_varname=py_varname, c_varname='c_'+py_varname, node=arg, cobjects=cobjects )
#        i += 1
#        arg = function.args[i]
#        py_varname = 'arg%s'%i
#        data_adaptor = VarAdaptor(
#          ostream, py_varname=py_varname, c_varname='c_'+py_varname, node=arg, cobjects=cobjects )
#        i += 1
#        adaptor = CallbackArgAdaptor( ostream, func_adaptor, data_adaptor )
#        arg_adaptors.append( adaptor )
#        return ostream
      if arg.is_pointer_to_fn():
        cbname = self.node.name + "_arg%s_callback"%i
        adaptor = CallbackAdaptor( ostream, arg, cbname )
        ostream = adaptor.define_func( names, tag_lookup, cprefix=cprefix, cobjects=cobjects, **kw )
        py_varname = 'arg%s'%i
#        adaptor = VarAdaptor( ostream, py_varname=py_varname, c_varname='c_'+py_varname, node=arg, cobjects=cobjects )
#        adaptor = VarAdaptor( ostream, py_varname=None, c_varname=cbname, node=arg, cobjects=cobjects )
        adaptor = CallbackArgAdaptor( ostream, py_varname=py_varname, c_varname=cbname, node=arg, cobjects=cobjects )
        arg_adaptors.append( adaptor )
        i += 1   
      else:
        if arg.name and 0: # disabled; arg.name may be an illegal name
          py_varname = arg.name
        else:
          py_varname = 'arg%s'%i
        adaptor = VarAdaptor( ostream, py_varname=py_varname, c_varname='c_'+py_varname, node=arg, cobjects=cobjects )
        arg_adaptors.append( adaptor )
        i += 1
    ret_adaptor = VarAdaptor( ostream, py_varname='ret', c_varname='c_ret', node=function.ret, cobjects=cobjects )

    # declare the function:
    ostream.startln('def %s(' % self.node.name )
    for i, adaptor in enumerate(arg_adaptors):
      adaptor.declare_py_arg()
      if i+1 < len(arg_adaptors):
        ostream.put( ', ' )
    ostream.endln('):')
    ostream.indent()
    ostream.putln( '"%s"' % self.node.cstr() )

    ret = function.ret
    rtp = ret.cbasetype() # WARNING: cbasetype may be cached

    # declare the local c vars:
    for adaptor in arg_adaptors:
      adaptor.declare_cvar(names=names, tag_lookup=tag_lookup, cprefix=cprefix)
    ret_adaptor.declare_cvar(names=names, tag_lookup=tag_lookup, cprefix=cprefix)
    ret_adaptor.declare_pyvar() #names=names, tag_lookup=tag_lookup, cprefix=cprefix)

    # convert the python args:
    for adaptor in arg_adaptors:
      adaptor.assign_to_cvar(names=names, tag_lookup=tag_lookup, cprefix=cprefix)

    # now call the function:
    c_args = [adaptor.get_c_arg() for adaptor in arg_adaptors]
    ret_adaptor.assign_cvar_from( '%s%s(%s)' % ( cprefix, self.node.name, ','.join(c_args) ) )

    # now return result:
    ret_adaptor.assign_to_pyvar( cobjects=cobjects, cprefix=cprefix )
    ret_adaptor.return_pyvar()

    ostream.dedent()
    ostream.putln()
    return ostream



class Declarator(object):
  def is_pyxnative( self ):
    # pyrex handles char* too
    # but i don't know if we should make this the default
    # sometimes we want to send a NULL, so ... XX
    self = self.cbasetype() # WARNING: cbasetype may be cached
    if self.is_void():
      return False
    if self.is_primative():
      return True
    if self.enum:
      return True
#    pointer = None
#    if self.pointer:
#      pointer = self.pointer
#    elif self.array:
#      pointer = self.array
#    if pointer and pointer.spec:
#      spec = pointer.spec
#      if BasicType("char") in spec and not Qualifier("unsigned") in spec:
#        # char*, const char*
##        print self.deepstr()
#        return True
    return False

  def _pyxstr( self, toks, indent, cprefix, use_cdef, shadow_name, **kw ):
    " this is the common part of pyxstr that gets called from both Declarator and Typedef "
    names = kw.get('names',{}) # what names have been defined ?
    kw['names']=names
    for node in self.nodes(): # depth-first
      if isinstance(node,Taged):
#        print "Declarator.pyxstr", node.cstr()
        if not node.tag.name:
          node.tag.name = "_anon_%s" % Node.get_unique_id()
        _node = names.get(node.tag.name,None)
        if _node is None or node.has_members():
          # either i am not defined at all, or this is my _real_ definition
          # emit def'n of this node
          toks.append( '  '*indent + 'cdef ' + node.pyxstr(indent=indent, cprefix=cprefix, shadow_name=shadow_name, **kw).strip() )
          names[ node.tag.name ] = node
      elif isinstance(node,GCCBuiltin) and node[0] not in names:
#        toks.append( '  '*indent + 'ctypedef long ' + node.pyxstr(indent=indent, **kw).strip() + ' # XX ??'  ) # XX ??
        toks.append( '  '*indent + 'struct __unknown_builtin ' )
        toks.append( '  '*indent + 'ctypedef __unknown_builtin ' + node.pyxstr(indent=indent, **kw).strip() )
        names[ node[0] ] = node
      for idx, child in enumerate(node):
        if type(child)==Array and not child.has_size():
          # mutate this mystery array into a pointer XX method: Array.to_pointer()
          node[idx] = Pointer()
          node[idx].init_from( child ) # warning: shallow init
          node[idx].pop() # pop the size element

  def pyxstr(self,toks=None,indent=0,cprefix="",use_cdef=True,shadow_name=True,**kw):
    " note: i do not check if my name is already in 'names' "
    self = self.clone() # <----- NOTE
    toks=[]
    names = kw.get('names',{}) # what names have been defined ?
    kw['names']=names

    self._pyxstr( toks, indent, cprefix, use_cdef, shadow_name, **kw )

    if self.name and not names.has_key( self.name ):
      names[ self.name ] = self
    if self.identifier is not None:
      comment = ""
      if self.name in python_kws:
        comment = "#"
      if cprefix and use_cdef and shadow_name:
        # When we are defining this guy, we refer to it using the pyrex shadow syntax.
        self.name = '%s%s "%s" ' % ( cprefix, self.name, self.name )
      cdef = 'cdef '
      if not use_cdef: cdef = '' # sometimes we don't want the cdef (eg. in a cast)
      # this may need shadow_name=False:
      toks.append( '  '*indent + comment + cdef + Node.pyxstr(self,indent=indent, cprefix=cprefix, **kw).strip() ) # + "(cprefix=%s)"%cprefix)
    #else: i am just a struct def (so i already did that) # huh ?? XX bad comment
#    dump("self,l")
#    print self.deepstr()
#    dump("self")
    return ' \n'.join(toks)

#  def get_pyxtpname( self ):
#    cstr = self.ctype().cstr()
#    name = ('__pyxtp_%s'%hash(cstr)).replace('-','m') # nasty...
#    return name

  def get_pyxtpname( self ):
    node = self.cbasetype() # WARNING: cbasetype may be cached
#    if node.pointer or node.array:
    # function pointers need unique names
    # because we make python callbacks with those names
    if (node.pointer or node.array) and not node.is_pointer_to_fn():
      name = "__pyxtp_pvoid"
#    elif isinstance(self,Typedef):
#      return self.name
    else:
      node.delete( StorageClass ) # hackery
      node.deeprm( Qualifier("const") ) # hackery
      cstr = node.cstr()
#      cstr = node.pyxstr( names = {}, tag_lookup = {}, use_cdef = False ) # XX redefines anon structs etc.
      name = ('__pyxtp_%s'%hash(cstr)).replace('-','m') # XX nasty...
      if cstr.replace(' ','').isalnum():
        name = '__pyxtp_'+'_'.join( cstr.split() )
      else:
        name = ('__pyxtp_%s'%hash(cstr)).replace('-','m') # XX nasty...
    return name

#  def handle_function_pointers(self, tp_tank, ostream, cprefix="", func_cb=None, root=None, **kw):
#    decl = self.deref()
#    fn = decl.function
#    if not fn.args:
#      return ostream # not a callback
#    arg = fn.args[-1]
#    if not arg.pointer or not arg.deref().is_void():
#      return ostream # not a callback
#
##    ostream.putln()
##    ostream.putln('#'+self.cstr())
##    ostream.putln('#'+decl.cstr())
#
#    name = self.get_pyxtpname()
#    tp_tank[name] = self
#    decl.name = name
#    if not fn.deepfind(Ellipses):
#      for i, arg in enumerate(fn.args):
#        arg.name = 'c_arg%s'%i
##      ostream.putln( decl.pyxstr(indent=0, cprefix="", **kw ) + ': # callback' )
#      ostream.putln( decl.pyxstr(indent=0, cprefix=cprefix, shadow_name=False, **kw ) + ': # callback' )
#      ostream.putln( '  "%s"'%decl.cstr() )
#      ostream.putln( "  ob = <object>c_arg%s" % i ) # the last arg is a PyObject*
#      args = []
#      for i, arg in enumerate(fn.args[:-1]): # skip the last arg
#        if arg.is_pyxnative(): # use it directly
#          args.append('c_arg%s'%i)
#        else:
#          args.append('arg%s'%i)
#          ostream.putln( "  arg%s = CPointer( value = <long>c_arg%s )" % (i, i) )
#      ostream.putln( "  res = ob(%s)" % ','.join(args) )
#      if not fn.ret.is_void():
#        if fn.ret.is_pyxnative():
#          ostream.putln( "  return res" )
#    return ostream

  def pyx_adaptor_decl( self, cobjects ):
    " return the name of the adaptor type for this node "
    assert type(cobjects)==dict, repr(cobjects)
    name = 'CObject'
    node = self.cbasetype()
    if node.struct:
      name = 'CStruct'
    elif node.pointer: # or node.array:
      name = 'CPointer'
    elif node.union:
      name = 'CUnion'
    elif node.enum:
#      name = 'CEnum'
      name = 'CInt'
    elif node.function: # what's this ?
      name = 'CObject'
    elif node.spec:
#      print self.spec
      tpset = TPSet()
      for _node in node.spec:
        if type(_node) in (BasicType,Qualifier):
          name = tpset.parse( _node.name )
#          print '  ', _node[0], name

    return '_'+name


  def pyx_adaptor_name( self, cobjects ):
    " return the name of the adaptor class for this node "
    assert type(cobjects)==dict
    name = 'CObject'
    if self.type_alias:
      alias = self.type_alias
      if alias.name in cobjects:
        name = alias.name
    elif self.struct:
      name = self.struct.tag and self.struct.tag.name
      if name is None or not name in cobjects:
        name = 'CStruct'
    elif self.pointer: # or self.array:
      node = self
      _name = ''
      while node.pointer:
        _name = _name + '.pointer'
        node = node.deref()
      name = node.pyx_adaptor_name( cobjects )
      if name == 'CObject': # type doesn't have the .pointer property
        name = 'CVoid' # class has the .pointer property
      name = name + _name
    elif self.union:
      name = 'CUnion'
    elif self.enum:
      name = 'CEnum'
    elif self.function:
#      name = 'CObject' # CFunction ?
      name = 'CFunction' # CFunction ?
    elif self.spec:
#      print self.spec
      tpset = TPSet()
      for node in self.spec:
        if type(node) in (BasicType,Qualifier):
          name = tpset.parse( node.name )
#          print '  ', node[0], name

    return name

  def pyx_wrapper(self, tp_tank, ostream=None, names=None, fnames=None, cprefix="", func_cb=None, special_stream=None, root=None, **kw):

# todo
# 2. all Typedefs emit Meta(pyx_adaptor_name(self)) statements
# 3. struct Declarators emit Meta(CStruct) statements
# 4. extern Declarators with pointer type have .target set correctly
# by calling the relevant .pointer Meta method

    if root is None:
      root = self
    # depth-first:
    ostream = Node.pyx_wrapper(self, tp_tank, ostream, names=names, fnames=fnames, cprefix=cprefix, func_cb=None, root=root, **kw) # do children
    modname = kw['modname']
    tag_lookup = kw['tag_lookup']
    macros = kw['macros']
    cobjects = kw['cobjects']

    nodebt = self.cbasetype() # WARNING: cbasetype may be cached
    cstr = nodebt.cstr()
    name = self.get_pyxtpname()
#    ostream.putln( self.deepstr(comment=True, nl="", indent="") )
#    if tp_tank.has_key(name):
##      assert tp_tank[name] == nodebt, (tp_tank[name], nodebt)
#      # move along.. nothing to see here..
#      pass
#    elif nodebt.function:
#      pass
#    elif nodebt.enum:
#      pass
#    elif nodebt.compound: # struct or union
#      pass # XX
#    elif nodebt.is_pointer_to_fn():
##      ostream = self.handle_function_pointers(tp_tank, ostream, cprefix=cprefix, func_cb=None, root=root, **kw)
#      pass
     
    if type(self)!=Declarator:
      pass
    elif self.name is None:
      pass
#    elif self.function and not self.name in fnames and not self.deepfind(Ellipses):
    elif self.function and not self.name in fnames and not self.function.is_varargs():
      fnames[self.name] = self
      if not func_cb:
        func_adaptor = FunctionAdaptor( ostream, self, special_stream )
      else:
        func_adaptor = func_cb( ostream, self )
      func_adaptor.define_func( names=names, tag_lookup=tag_lookup, cprefix=cprefix, cobjects=cobjects )
    elif nodebt.is_pyxnative():
      self.pyxsym(ostream=ostream, names=names, tag_lookup=tag_lookup, cprefix=cprefix, modname=modname, cobjects=cobjects)
    else:
      ostream.putln( '# ' + self.cstr() )
      # cannot take address of array:
      if not self.function and not self.array and self.name:
        # we can expose these as CObject's too
        self.pyxsym(ostream=ostream, names=names, tag_lookup=tag_lookup, cprefix=cprefix, modname=modname, cobjects=cobjects)

    return ostream

  def pyxsym(self, ostream, names=None, tag_lookup=None, cprefix="", modname=None, cobjects=None):
    assert self.name is not None, self.deepstr()
    ostream.putln( '# ' + self.cstr() )
# This cdef is no good: it does not expose a python object
# and we can't reliably set a global var
#    ostream.putln( 'cdef %s %s' % ( self.pyx_adaptor_decl(cobjects), self.name ) ) # _CObject
#    ostream.putln( '%s = %s()' % (self.name, self.pyx_adaptor_name(cobjects)) )
#    ostream.putln( '%s.p = <void*>&%s' % (self.name, cprefix+self.name) )
#    # expose a python object:
#    ostream.putln( '%s.%s = %s' % (modname,self.name, self.name) )
    ostream.putln( '%s = %s( addr = <long>&%s )' % (self.name, self.pyx_adaptor_name(cobjects), cprefix+self.name) )
    return ostream


class Typedef(Declarator):
  def pyxstr(self,toks=None,indent=0,cprefix="",use_cdef=True,shadow_name=True,**kw): # shadow_name=True
    " warning: i do not check if my name is already in 'names' "
    assert shadow_name == True
    self = self.clone() # <----- NOTE
    toks=[]
    names = kw.get('names',{}) # what names have been defined ?
    kw['names']=names

#    if self.tagged and not self.tagged.tag.name:
#      # "typedef struct {...} foo;" => "typedef struct foo {...} foo;"
#      # (to be emitted in the node loop below, and suppressed in the final toks.append)
#      self.tagged.tag = Tag( self.name ) # this is how pyrex does it: tag.name == self.name
#   XX that doesn't work (the resulting c fails to compile) XX

    self._pyxstr( toks, indent, cprefix, use_cdef, shadow_name, **kw )

#    print self.deepstr()
#    dump("self")
    if self.name and not names.has_key( self.name ):
      names[ self.name ] = self
    if not (self.tagged and self.name == self.tagged.tag.name):
      comment = ""
      if self.name in python_kws:
        comment = "#"
        #if cprefix:
        #  self.name = '%s%s "%s" ' % ( cprefix, self.name, self.name ) # XX pyrex can't do this
      if cprefix: # shadow_name=True
        # My c-name gets this prefix. See also TypeAlias.pyxstr(): it also prepends the cprefix.
        self.name = '%s%s "%s" ' % ( cprefix, self.name, self.name )
      toks.append( '  '*indent + comment + 'ctypedef ' + Node.pyxstr(self,indent=indent, cprefix=cprefix, **kw).strip() )
#    dump("self,l")
    return ' \n'.join(toks)

  def pyx_wrapper(self, tp_tank, ostream=None, names=None, fnames=None, cprefix="", func_cb=None, root=None, **kw):
# todo
# 2. all Typedefs emit Meta(pyx_adaptor_name(self)) statements
# 3. struct Declarators emit Meta(CStruct) statements
# 4. extern Declarators with pointer type have .target set correctly
# by calling the relevant .pointer Meta method

    if root is None: # hmm, we aren't using this ATM.. (going to use it for doing callbacks i think)
      root = self
    # depth-first:
    ostream = Node.pyx_wrapper(self, tp_tank, ostream, names=names, fnames=fnames, cprefix=cprefix, func_cb=None, root=root, **kw) # do children
    modname = kw['modname']
    tag_lookup = kw['tag_lookup']
    macros = kw['macros']
    cobjects = kw['cobjects']

    nodebt = self.cbasetype() # WARNING: cbasetype may be cached
    name = self.get_pyxtpname()
#    ostream.putln( self.deepstr(comment=True, nl="", indent="") )
    if tp_tank.has_key(name):
#      assert tp_tank[name] == nodebt, (tp_tank[name], nodebt)
      # move along.. nothing to see here..
      pass
    elif nodebt.function:
      pass
    elif nodebt.enum:
      pass
#    elif nodebt.pointer or nodebt.array:
#      pass # we just malloc a void*
    elif nodebt.compound:
      pass
    elif nodebt.is_pointer_to_fn():
#      ostream = self.handle_function_pointers(tp_tank, ostream, cprefix=cprefix, func_cb=None, root=root, **kw)
      pass

    if self.name: # and not self.name[0]=='_': # we skip the underscored names
      ostream.putln( '' )
      ostream.putln( '# ' + self.cstr() )
      name = self.name
      adaptor_name = self.pyx_adaptor_name( cobjects ) 
      cobjects[name] = self # typedef struct foo foo; is ok, i think..
      if nodebt.compound:
        ostream.putln("class %s(%s):" %(name,adaptor_name))
        ostream.indent()
        ostream.putln("__metaclass__ = Meta")
#        assert self.compound.tag.name in tag_lookup, self.compound.tag.name
        compound = nodebt.compound
        if self.compound.tag.name:
          compound = tag_lookup[ self.compound.tag.name ]
        compound_adaptor = VarAdaptor( ostream, py_varname='self', c_varname=None, node=self, cobjects=cobjects )
#        print compound.cstr()
        for member in compound.members:
          ostream.putln("#"+member.cstr())
          arg_adaptor = VarAdaptor( ostream, py_varname='value', c_varname='c_value', node=member, cobjects=cobjects )

          # set method:
          ostream.startln("def set_%s( _CStruct self, " % member.name)
          arg_adaptor.declare_py_arg()
          ostream.endln(' ):')
          ostream.indent()
#          ostream.putln( "%s.%s = %s" %\
#            ( compound_adaptor.get_c_from_pyvar(names, tag_lookup, cprefix ),
#              member.name,
#              arg_adaptor.get_c_from_pyvar(names, tag_lookup, cprefix) ) )
#          # doesn't work for arrays:
#          ostream.putln( "(<%s%s*>(%s.p))[0].%s = %s%s" %\
#            ( cprefix, self.name, compound_adaptor.py_varname,
#              member.name,
#              arg_adaptor.get_c_cast(names, tag_lookup, cprefix),
#              arg_adaptor.get_c_from_pyvar(names, tag_lookup, cprefix) ) )
#          ostream.putln( "memcpy( &((<%s%s*>(%s.p))[0].%s), %s, sizeof(%s) )" %\
#            ( cprefix, self.name, compound_adaptor.py_varname, member.name,
#              arg_adaptor.get_caddr_from_pyvar(names, tag_lookup, cprefix),
#              arg_adaptor.get_c_type(names, tag_lookup, cprefix),
#            ) )
          arg_adaptor.assign_to_compound_member( compound_adaptor, names, tag_lookup, cprefix )
          ostream.dedent()

          # get method:
          ostream.putln("def get_%s( _CStruct self ):" % member.name)
          ostream.indent()
          arg_adaptor.declare_pyvar() #names=names, tag_lookup=tag_lookup, cprefix=cprefix)
          arg_adaptor.init_pyvar( cobjects )
          arg_adaptor.assign_from_compound_member( compound_adaptor, names, tag_lookup, cprefix )
          arg_adaptor.return_pyvar()
          ostream.dedent()

          ostream.putln("%s = property(get_%s,set_%s)" % ((member.name,)*3))
        ostream.dedent()
        ostream.putln("%s.sizeof = sizeof(%s%s)" % (name,cprefix,name))
      else:
        ostream.putln("%s = Meta( '%s', (%s,), {} )" % ( name, name, adaptor_name ))
      if self.enum:
        for member in self.enum.members:
          # for an Enum these are all Identifiers
          ostream.putln( '%s.%s = %s(%s)' % ( name, member.name, name, cprefix+member.name ) )
          ostream.putln( '%s = %s' % ( member.name, cprefix+member.name ) )
  #    ostream.putln( self.deepstr(comment=True) )

    return ostream


class AbstractDeclarator(Declarator):
  """ used in Function; may lack an identifier """
  def pyxstr(self,toks=None,indent=0,**kw):
    if self.name in python_kws:
      # Would be better to do this in __init__, but our subclass doesn't call our __init__.
      self.name = '_' + self.name
#    return '  '*indent + Node.pyxstr(self,toks,indent, **kw).strip()
    return Node.pyxstr(self,toks,indent, **kw).strip()
  def pyx_wrapper(self, tp_tank, ostream=None, **kw):
    nodebt = self.cbasetype()
    name = self.get_pyxtpname()
#    ostream.putln( self.deepstr(comment=True, nl="", indent="") )
    if tp_tank.has_key(name):
      pass
#    elif nodebt.is_pointer_to_fn():
#      assert self.is_pointer_to_fn(), self.cstr()
#      ostream = self.handle_function_pointers(tp_tank, ostream, **kw)
#      # bad idea: if there is a struct/enum as an argument, it gets redefined as a "new" struct/enum
#      #ostream = nodebt.handle_function_pointers(tp_tank, ostream, **kw)

#    if self.find( Ellipses ) is None and not self.is_void(): 
#      ostream = Declarator.pyx_wrapper(self, tp_tank, ostream=ostream, **kw)
    return ostream


class FieldLength(object):
  """
  """
  def pyxstr(self,toks,indent,**kw):
    pass


class StructDeclarator(Declarator): # also used in Union
  """
  """
  def pyxstr(self,toks=None,indent=0,**kw):
    comment = ""
    if self.name in python_kws:
      comment = "#"
    return '  '*indent + comment + Node.pyxstr(self,toks,indent, **kw).strip()
  def pyx_wrapper(self, tp_tank, ostream=None, cprefix="", **kw):
    # disabled for now
    return ostream
    if self.field_length is None:
      ostream = Declarator.pyx_wrapper( self, tp_tank, ostream, cprefix=cprefix, **kw ) 
    return ostream

class DeclarationSpecifiers(object):
  """
  """
  pass

class TypeSpecifiers(DeclarationSpecifiers):
  """
  """
  pass

class Initializer(object):
  """
  """
  pass

class Declaration(object):
  """
  """
  pass

class ParameterDeclaration(Declaration):
  """
  """
  pass

class StructDeclaration(Declaration):
  """
  """
  pass

class TransUnit(object):
  """
    Top level node.
  """
  def pyxstr(self, filenames, modname, macros = {}, names = None, func_cb=None, **kw):
    cprefix = "__c_"
    # names: one global namespace:
    # all identifiers (not abstract identifiers), tags and enum elements
    # map names to Declarator, Enum element, ... what else?
    if names is None:
      names = {} # XX update from python_kws ??
    decls = self.pyx_decls(filenames,modname,macros,names,func_cb,cprefix)
    wrappers, special = self.pyx_wrappers(filenames,modname,macros,names,func_cb,cprefix)
    return decls, wrappers, special

  def pyx_decls(self, filenames, modname, macros = {}, names = None, func_cb=None, cprefix="", **kw):
    # PART 1: emit extern declarations
    ostream = OStream()
    now = datetime.today()
    ostream.putln( now.strftime('# Code generated by pyxelator on %x at %X') + '\n' )
    ostream.putln("# PART 1: extern declarations")
    for filename in filenames:
      ostream.putln( 'cdef extern from "%s":\n  pass\n' % filename )
    ostream.putln( 'cdef extern from *:' )
    file = None # current file
    ostream.putln("""  ctypedef long %ssize_t "size_t" # XX pyrex can't handle unsigned long """ % cprefix )
    ostream.putln("""  cdef void* %smemcpy "memcpy"(void*dest,void*src,__c_size_t n) """ % cprefix )
    names['size_t']=None
    names['memcpy']=None
    for node in self:
      ostream.putln('')
      ostream.putln('  # ' + node.cstr() )
      assert node.marked
      comment = False
      if node.name and node.name in names:
        comment = True # redeclaration
#      ostream.putln( node.deepstr( comment=True ) )
      s = node.pyxstr(indent=1, names=names, tag_lookup = self.tag_lookup, cprefix=cprefix, **kw)
      if s.split():
        if comment:
          s = "#"+s.replace( '\n', '\n#' ) + " # redeclaration "
        if node.file != file:
          file = node.file
#          ostream.putln( 'cdef extern from "%s":' % file )
          ostream.putln( '  # "%s"' % file )
        ostream.putln( s )
    ostream.putln('\n')
#    s = '\n'.join(toks)
    return ostream.join()

  def pyx_wrappers(self, filenames, modname, macros = {}, names = None, func_cb=None, cprefix="", **kw):
    ostream = OStream()
    special_stream = OStream() # this output is for template code that needs hand-tweaking
    now = datetime.today()
    ostream.putln( now.strftime('# Code generated by pyxelator on %x at %X') + '\n' )
#    ostream.putln('include "adapt.pxi"')
    ostream.putln('cimport adapt')
    adapt_cls = "CStruct CArray CChar CDouble CFloat CInt CLDouble CLLong CLong CObject CPointer CSChar CShort CUChar CUInt CULLong CULong CUShort ".split()
    ostream.putln('from adapt cimport '+','.join(['_'+a for a in adapt_cls]))
    adapt_cls = "CArray CArrayCIntRange CChar CDouble CEnum CFloat CFunction CInt CLDouble CLLong CLong CObject CPointer CSChar CShort CStr CStruct CUChar CUInt CULLong CULong CUShort CUnion CVoid CPyObject FunctionMeta Meta get_slice_size null py2cobject str_from_charp".split()
    ostream.putln('from adapt import '+','.join(adapt_cls))
    ostream.putln('include "%s_special.pxi"'%modname)
    ostream.put(\
"""

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

""")

#    ostream.putln("""
##import sys
##%s = sys.modules["%s"] # this doesn't always work ... ??
#
##import sys
##thismod = sys.modules[__name__]
##thismod.foo = 1234
#""" % (modname,modname) )

    ostream.putln("# wrappers")
    fnames = {}
    tp_tank = {}
    cobjects = {} # map each adaptor name (CObject subclass) to Node that created it
    for node in self:
      node.pyx_wrapper(tp_tank, ostream=ostream, names=names, fnames=fnames, tag_lookup=self.tag_lookup, cprefix=cprefix, cobjects=cobjects, modname=modname, macros=macros, func_cb=func_cb, special_stream = special_stream )

    ostream.putln('\n')
#    s = '\n'.join(toks)
#    sys.stderr.write( "# %s lines\n" % s.count('\n') )

    return ostream.join(), special_stream.join()

# XX warn when we find a python keyword XX
python_kws = """
break continue del except exec finally pass print raise
return try global assert lambda yield
for while if elif else and in is not or import from """.split()
python_kws = dict( zip( python_kws, (None,)*len(python_kws) ) )


