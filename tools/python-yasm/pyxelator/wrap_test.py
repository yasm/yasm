#!/usr/bin/env python

""" 

(c) 2002, 2003, 2004, 2005 Simon Burton <simon@arrowtheory.com>
Released under GNU LGPL license.

version 0.xx

"""


import sys
import os

from work_unit import WorkUnit, get_syms

def mk_all():
    cflags = ""
    cflags = " ".join( sys.argv[1:] )
#    syms = get_syms( *libs )

#    files = [ "/usr/X11R6/include/X11/Xlib.h" ]
    files = [ "gdk/gdk.h" ]

    extradefs = """
"""
    def cb(node):
      name, file = node.name, node.file
      return True
      return name and ( name in syms or extradefs.count(name) )
    modname = '_gdk'
    oname = '%s.pyx'%modname
    unit = WorkUnit(files,modname,oname,cflags,True,mark_cb=cb,extradefs=extradefs) 

    unit.parse( True )
    unit.transform(verbose=False, test_parse=False, test_types=False)
    unit.output()

if __name__=="__main__":
  if 'trace' in sys.argv:
    sys.argv.remove('trace')
#    import dump
#    def cb( data, filename, lineno, name, lcl, gbl ):
#      print filename, name, lineno, lcl.keys()
    class Self: pass
    def trace( frame, event, arg ):
      code = frame.f_code
  #    print "trace:", event, arg, frame.f_lineno, frame.f_locals
      name, lineno, filename = code.co_name, frame.f_lineno, code.co_filename
      lcl, gbl = frame.f_locals, frame.f_globals
      if 'self' in lcl:
        self = Self()
        self.__dict__.update( lcl['self'].__dict__ )
      depth = -1
      while frame.f_back:
        depth += 1
        frame = frame.f_back
      indent = "  "*depth
      if "cparse" in filename:
        if 'self' in lcl:
          name = lcl['self'].__class__.__name__ + '.' + name
        msg = indent+"%s: %s"%(name, lineno)
        print msg
        return trace
      elif name == 'get_token':
        if event == 'return':
          print indent + str( ( self.lno, self.tok, self.kind ) )
        return trace
#    dump.Tracer( cb, None ) #, 'cparse.py' )
    sys.settrace(trace)
    mk_all()
  else:
    try:
      import psyco
      psyco.full()
    except: 
      pass


    mk_all()






