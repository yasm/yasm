#! /usr/bin/env python

from distutils.core import setup
from distutils.extension import Extension
from Pyrex.Distutils import build_ext

setup(
        name='yasm',
        version='0.0',
        description='Python bindings for Yasm',
        author='Michael Urman',
        url='http://www.tortall.net/projects/yasm',
        ext_modules=[
            Extension('yasm', ['yasm.pyx'],
                depends=['bytecode.pxi',
                         'coretype.pxi',
                         'expr.pxi',
                         'floatnum.pxi',
                         'intnum.pxi',
                         'symrec.pxi',
                         'value.pxi'],
                include_dirs=['../..'],
                library_dirs=['../..'],
                libraries=['yasm'],
            ),
        ],
        cmdclass = dict(build_ext=build_ext),
    )
        
