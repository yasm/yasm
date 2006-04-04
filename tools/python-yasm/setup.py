#! /usr/bin/env python

from distutils.core import setup
from distutils.extension import Extension
from Pyrex.Distutils import build_ext
from os.path import basename, join, exists
import re

def ReadSetup(filename):
    """ReadSetup goes through filename and parses out the values stored
    in the file.  Values need to be stored in a
    \"key=value format\""""

    #return val
    opts = {}

    fh = open(filename, 'r')
    reobj = re.compile(r"([a-z]+)=(.+)")
    for line in fh.readlines():
        matchobj = reobj.search(line)

        if matchobj is None:
            raise "Error:  syntaxt error in setup.txt line: %s"%line

        lab, val = matchobj.groups()
        opts[lab] = val
    return opts

def ParseCPPFlags(str):
    """parse the CPPFlags macro"""
    incl_dir = []
    cppflags = []

    tokens = str.split()
    # go through the list of compile flags.  treat search path args
    # specially; otherwise just add to "extra_compile_args"
    for tok in tokens:
        if tok.startswith("-I"):
            incl_dir.append(tok[2:])
        else:
            cppflags.append(tok)

    return (incl_dir, cppflags)

def ParseSources(str, srcdir):
    """parse the Sources macro"""
    sources = []

    tokens = str.split()
    # go through the list of source filenames
    # do the dance of detecting if the source file is in the current
    # directory, and if it's not, prepend srcdir
    for tok in tokens:
        fn = None
        if tok.endswith(".c"):
            fn = tok
        elif tok.endswith(".y"):
            fn = basename(tok)[:-2] + ".c"
        else:
            continue
        if not exists(fn):
            fn = join(srcdir, fn)
        sources.append(fn)

    return sources

def RunSetup(incldir, cppflags, sources):
    setup(
          name='yasm',
          version='0.0',
          description='Python bindings for Yasm',
          author='Michael Urman',
          url='http://www.tortall.net/projects/yasm',
          ext_modules=[
                       Extension('yasm',
                                 sources=sources,
                                 extra_compile_args=cppflags,
                                 include_dirs=incldir,
                                 library_dirs=['.'],
                                 libraries=['yasm'],
                       ),
                      ],
          cmdclass = dict(build_ext=build_ext),
         )

if __name__ == "__main__":
    opts = ReadSetup("python-setup.txt")
    incldir, cppflags = ParseCPPFlags(opts["includes"])
    sources = ParseSources(opts["sources"], opts["srcdir"])
    sources.append('yasm_python.c')
    RunSetup(incldir, cppflags, sources)

