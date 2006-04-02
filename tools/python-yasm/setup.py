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
                include_dirs=['../..'],
                extra_objects=['../../libyasm.a'],
            ),
        ],
        cmdclass = dict(build_ext=build_ext),
    )
        
