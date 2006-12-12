#!/usr/bin/env python

# 
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

# File    :  crest/utils/dump.py
#
# Author  :  Simon Burton (simon@arrowtheory.com)
#
# Created :  28 March 2005
#
# Updated :  
#

"""
Some heavy duty debug tools here...
"""

import sys
import logging
import gc

def dump( expr="", info="", indent="" ):
  """
    print calling function name,
    and python expression string <expr>, as evaluated in the calling function.
    Note: we split <expr> on commas.
  """
  try:
    assert 0
  except:
    tp, val, tb = sys.exc_info()
    frame = tb.tb_frame
    frame = frame.f_back # our caller
    name = frame.f_code.co_name
    if name=='?':
      name=''
    else:
      name=name+": "
    data = eval( expr, frame.f_globals, frame.f_locals )
    depth = 0 # of stack
    while frame.f_back:
      depth += 1
      frame = frame.f_back
    exprs = expr.split(',')
    if len(exprs)==1:
      data = (data,)
    data = tuple([ repr(x) for x in data ])
    print indent*depth + name + info + ( ",".join( ["%s=%%s"%x for x in exprs] ) % data )


def test_dump():
  # this is our function we are debugging
  def foo( a, b ):
    c = a + b
    dump( "a, b, a+b, c", "c should be a+b: " )
  # now we call it
  foo( 5, 77 )

class Tracer(object):
  singleton = None
  def __init__(self, callback, data, *filenames):
    """
      trace execution and call the callback on each line
    """
    if Tracer.singleton is not None:
      raise Exception, "Only one trace object supported at a time"
    Tracer.singleton = self
    self.filenames = filenames
    self.callback = callback
    self.data = data
    sys.settrace(self)
  def trace( self, frame, event, arg ):
    code = frame.f_code
#    print "trace:", event, arg, frame.f_lineno, frame.f_locals
    name, lineno, filename = code.co_name, frame.f_lineno, code.co_filename
    lcl, gbl = frame.f_locals, frame.f_globals
    depth = -1 # depth of stack
    while frame.f_back:
      depth += 1
      frame = frame.f_back
    msg = "  "*depth+"%s:%s in %s()"%(filename, lineno, name)
    self.callback( self.data, filename, lineno, name, lcl, gbl )
  def trace_local( self, frame, event, arg ):
    code = frame.f_code
#    print "trace_local:", event, arg, frame.f_lineno, frame.f_locals
    name, lineno, filename = code.co_name, frame.f_lineno, code.co_filename
    lcl, gbl = frame.f_locals, frame.f_globals
    self.callback( self.data, filename, lineno, name, lcl, gbl )
#    return None
    return self.trace_local
  def __call__( self, frame, event, arg ):
#    print "trace:", event, arg, frame.f_locals
    code = frame.f_code
    filename = code.co_filename
#    print filename
    if not self.filenames or filename.split('/')[-1] in self.filenames:
      self.trace(frame,event,arg)
      return self.trace_local
  def fini(self):
    sys.settrace(None)
    Tracer.singleton = None
    

class Coverage(Tracer):
  def __init__(self, filename):
    """
    """
    Tracer.__init__( self, self.callback, {}, filename )
    self.filename = filename
  def callback( self, data, filename, lno, name, *args ):
    data[lno] = name
  def save( self ):
#    print 'save:', self.data
    ifile = open( self.filename )
    filename = self.filename[:-3]+'.cov.py' # keep .py extension
    ofile = open( filename, 'w+' )
    lno = 1
    while 1:
      line = ifile.readline()
      if not line:
        break
      c = ' '
      if lno in self.data:
        c = '+'
      ofile.write( c + line )
      lno += 1

class Watcher(Tracer):
  def __init__(self, expr, everyline=False, gbls={}, lcls={}, *filenames):
    Tracer.__init__( self, self.callback, None, *filenames )
    self.expr = expr
    self.gbls = gbls
    self.lcls = lcls
    self.msg = ""
  def callback(self, data, filename, lno, name, *args):
    try:
      msg = str( eval(self.expr, self.gbls, self.lcls) )
    except Exception, e:
      msg = str(e)
    if msg != self.msg:
      print "function:", name, ":", lno-1
      print "%s: %s" % ( self.expr, msg )
      self.msg = msg
 

class TraceLogger(object): # inherit from Tracer ?
  singleton = None
  def __init__(self, *filenames):
    " log all function calls originating in a file from filenames "
    if TraceLogger.singleton is not None:
      raise Exception, "Only one trace object supported at a time"
    TraceLogger.singleton = self
    self.filenames = filenames
    self.loggers = {} # map names to loggers
    self.hd = logging.StreamHandler(sys.stdout)
#    fm = logging.Formatter('%(asctime)s %(levelname)s %(message)s')
    fm = logging.Formatter('%(message)s')
    self.hd.setFormatter(fm)
    sys.settrace(self)
  def get_logger(self, name):
    logger = self.loggers.get(name, None)
    if logger is None:
      logger = logging.getLogger(name)
      logger.addHandler( self.hd )
    logger.setLevel(1)
    return logger
  def trace( self, frame, event, arg ):
    code = frame.f_code
#    print "trace_local:", event, arg, frame.f_lineno, frame.f_locals
    name, lineno, filename = code.co_name, frame.f_lineno, code.co_filename
    lcl, gbl = frame.f_locals, frame.f_globals
    depth = -1 # depth of stack
    while frame.f_back:
      depth += 1
      frame = frame.f_back
    msg = "  "*depth+"%s:%s in %s()"%(filename, lineno, name)
    self.get_logger( gbl.get("__name__","?") ).log(1, msg)
  def trace_local( self, frame, event, arg ):
    return None
    return self.trace_local
  def __call__( self, frame, event, arg ):
#    print "trace:", event, arg, frame.f_locals
    code = frame.f_code
    filename = code.co_filename
    if not self.filenames or filename in self.filenames:
      self.trace(frame,event,arg)
#      return self.trace_local
  def fini(self):
    sys.settrace(None)
    TraceLogger.singleton = None
    

def test_trace():
  def fa(a,b):
    return a+b
  def fb(x,y):
    z = fa(x,y)
    return z
  trace = TraceLogger(__file__)
  sys.settrace(trace)
  fb(3,4)
  trace.fini()


class MemLogger(object):
  def __init__( self ):
    gc.set_debug(gc.DEBUG_LEAK)
    self.tank = {}
    for ob in gc.get_objects():
      self.tank[id(ob)]=None
    self.watchtank={}
  
  def watch(self, ob):
    self.watchtank[id(ob)] = None
  
  def dumpinfo(self, ob):
    print id(ob), type(ob),
    if hasattr(ob,'__name__'): print "\t", ob.__name__,
    if hasattr(ob,'shape'): print "\t", ob.shape,
    print
  
  def dumpfrom(self, ob, depth=1, indent=0):
    if depth==0: return
    fromobs = gc.get_referrers(ob)
    if fromobs:
      for _ob in fromobs:
        print '  '*indent+' ',; dumpinfo(_ob)
        dumpfrom(_ob,depth-1,indent+1)
  
  def find(self, ob, tp, tank=None):
    " traverse object space, from ob, looking for object of type tp "
    # XXX this needs to be breadth first XXX
    if tank is None:
      tank={id(ob):None}
  #  search = gc.get_referrers(ob)+gc.get_referents(ob)
    search = gc.get_referrers(ob)
    for _ob in search:
      if not id(_ob) in tank:
        tank[id(_ob)] = None # been here
        if type(_ob)==tp:
          return _ob # got it
        _ob = self.find(_ob,tp,tank)
        if _ob is not None:
          return _ob
  
  def check(self, msg=None):
    " look for newly created objects "
    foundone=False
    newtank={}
    for ob in gc.get_objects():
      newtank[id(ob)]=None
      if not self.tank.has_key(id(ob)): # and id(ob) in watchtank:
        print " NEW:", 
        dumpinfo(ob)
  #      dumpfrom(ob,1,2)
  #      _ob = self.find(ob,type(ob))
  #      if _ob:
  #        print " found:", dumpinfo(_ob)
  #      toobs = gc.get_referents(ob)
  #      if toobs:
  #        print "\trefers to:"
  #        for _ob in toobs:
  #          print "\t  ",; dumpinfo(_ob)
        self.tank[id(ob)] = None
        foundone = True
  #  for key in newtank:
  #    if not self.tank.has_key(key):
  #      print " DEL:", key
  ##            print "collect:", gc.collect() # == 0 (OK)
    if foundone:
      if msg: print "check:", msg
      print "objects:", len(gc.get_objects())
      print "--"*30
    sleep(0.04)
  
def test_mem():
  #XXX
  pass

if __name__ == "__main__":
  test_trace()


