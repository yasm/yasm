#!/usr/bin/env python
# Generate expected binary hexdump output from specially formatted asm file.
#
#  Copyright (C) 2009  Peter Johnson
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND OTHER CONTRIBUTORS ``AS IS''
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR OTHER CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

def gen_expected(fout, fin):
    count = 0
    for line in fin:
        if not line:
            continue
        if line.startswith(';'):
            continue
        # extract portion after ; character
        (insn, sep, comment) = line.partition(';')
        if not sep:
            continue    # ; not found
        # comment must be formatted as 2-char hex and/or 3-char octal bytes
        # separated by spaces
        bytes = []
        for byte in comment.split():
            if len(byte) == 2:
                bytes.append(int(byte, 16))
            elif len(byte) == 3:
                bytes.append(int(byte, 8))
            else:
                break
        for byte in bytes:
            print >>fout, "%02x " % byte
        count += 1
    print "Processed %d instructions" % count


if __name__ == "__main__":
    import sys
    if len(sys.argv) != 3:
        print >>sys.stderr, "Usage: gencheck.py <in.asm> <out.hex>"
        sys.exit(2)
    gen_expected(open(sys.argv[2], "w"), open(sys.argv[1], "r"))
