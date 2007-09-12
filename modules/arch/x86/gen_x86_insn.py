#! /usr/bin/env python
#
# x86 instructions and prefixes data and code generation
#
#  Copyright (C) 2002-2007  Peter Johnson
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
#
# NOTE: operands are arranged in NASM / Intel order (e.g. dest, src)

ordered_cpus = [
    "086", "186", "286", "386", "486", "586", "686", "K6", "Athlon", "P3",
    "P4", "IA64", "Hammer"]
ordered_cpu_features = [
    "FPU", "Cyrix", "AMD", "MMX", "3DNow", "SMM", "SSE", "SSE2",
    "SSE3", "SVM", "PadLock", "SSSE3", "SSE41", "SSE42", "SSE4a"]
unordered_cpu_features = ["Priv", "Prot", "Undoc", "Obs"]

def cpu_lcd(cpu1, cpu2):
    """Find the lowest common denominator of two CPU sets."""
    retval = set()

    # CPU
    cpu1cpus = set(ordered_cpus) & set(cpu1)
    if not cpu1cpus:
        cpu1cpus.add("086")
    cpu1mincpu = min(ordered_cpus.index(x) for x in cpu1cpus)
    cpu2cpus = set(ordered_cpus) & set(cpu2)
    if not cpu2cpus:
        cpu2cpus.add("086")
    cpu2mincpu = min(ordered_cpus.index(x) for x in cpu1cpus)
    cpumin = ordered_cpus[min(cpu1mincpu, cpu2mincpu)]
    if cpumin == "086":
        cpumin = "Any"

    if cpumin != "Any":
        retval.add(cpumin)

    # Feature
    cpu1features = set(ordered_cpu_features) & set(cpu1)
    if not cpu1features:
        cpu1minfeature = -1
    else:
        cpu1minfeature = min(ordered_cpu_features.index(x)
                             for x in cpu1features)

    cpu2features = set(ordered_cpu_features) & set(cpu2)
    if not cpu2features:
        cpu2minfeature = -1
    else:
        cpu2minfeature = min(ordered_cpu_features.index(x)
                             for x in cpu2features)

    if cpu1minfeature != -1 and cpu2minfeature != -1:
        featuremin = ordered_cpu_features[min(cpu1minfeature, cpu2minfeature)]
        retval.add(featuremin)

    # Unordered features
    for feature in set(unordered_cpu_features) & set(cpu1) & set(cpu2):
        retval.add(feature)

    # 64-bitness
    if "64" in cpu1 and "64" in cpu2:
        retval.add("64")
    if "Not64" in cpu1 and "Not64" in cpu2:
        retval.add("Not64")

    return retval

class Operand(object):
    def __init__(self, **kwargs):
        self.type = kwargs.pop("type")
        self.size = kwargs.pop("size", "Any")
        self.relaxed = kwargs.pop("relaxed", False)
        self.dest = kwargs.pop("dest", None)
        self.tmod = kwargs.pop("tmod", None)
        self.opt = kwargs.pop("opt", None)

        if kwargs:
            for arg in kwargs:
                print "Warning: unrecognized arg %s" % arg

    def __str__(self):
        op_str = []

        op_str.append("OPT_%s" % self.type)
        op_str.append("OPS_%s" % self.size)

        if self.relaxed:
            op_str.append("OPS_Relaxed")

        if self.tmod is not None:
            op_str.append("OPTM_%s" % self.tmod)

        if self.dest == "EA64":
            op_str.append("OPEAS_64")
            op_str.append("OPA_EA")
        else:
            op_str.append("OPA_%s" % self.dest)

        if self.opt is not None:
            op_str.append("OPAP_%s" % self.opt)

        return '|'.join(op_str)

    def __eq__(self, other):
        return (self.type == other.type and
                self.size == other.size and
                self.relaxed == other.relaxed and
                self.dest == other.dest and
                self.tmod == other.tmod and
                self.opt == other.opt)

    def __ne__(self, other):
        return (self.type != other.type or
                self.size != other.size or
                self.relaxed != other.relaxed or
                self.dest != other.dest or
                self.tmod != other.tmod or
                self.opt != other.opt)

class GroupForm(object):
    def __init__(self, **kwargs):
        # Parsers
        self.parsers = set(kwargs.pop("parsers", ["gas", "nasm"]))

        # CPU feature flags initialization
        self.cpu = set(kwargs.pop("cpu", []))
        if kwargs.pop("only64", False):
            self.cpu.add("64")
        if kwargs.pop("not64", False):
            self.cpu.add("Not64")

        # Operation size
        self.opersize = kwargs.pop("opersize", 0)
        if self.opersize == 8:
            self.opersize = 0

        if self.opersize == 64:
            self.cpu.add("64")
        elif self.opersize == 32 and "64" not in self.cpu:
            self.cpu.add("386")

        # Default operation size in 64-bit mode
        self.def_opersize_64 = kwargs.pop("def_opersize_64", 0)

        # GAS suffix
        self.gen_suffix = kwargs.pop("gen_suffix", True)
        self.suffixes = kwargs.pop("suffixes", None)
        suffix = kwargs.pop("suffix", None)
        if suffix is not None:
            self.suffixes = [suffix]
        if self.suffixes is not None:
            self.suffixes = set(x.upper() for x in self.suffixes)

        # Special instruction prefix
        self.special_prefix = "0"
        if "prefix" in kwargs:
            self.special_prefix = "0x%02X" % kwargs.pop("prefix")

        # Spare value
        self.spare = kwargs.pop("spare", 0)

        # Build opcodes string (C array initializer)
        if "opcode" in kwargs:
            # Usual case, just a single opcode
            self.opcode = kwargs.pop("opcode")
            self.opcode_len = len(self.opcode)
        elif "opcode1" in kwargs and "opcode2" in kwargs:
            # Two opcode case; the first opcode is the "optimized" opcode,
            # the second is the "relaxed" opcode.  For this to work, an
            # opt="foo" must be set for one of the operands.
            self.opcode1 = kwargs.pop("opcode1")
            self.opcode2 = kwargs.pop("opcode2")
            self.opcode_len = len(self.opcode1)
        else:
            raise KeyError("missing opcode")

        # Build operands string (C array initializer)
        self.operands = kwargs.pop("operands")
        for op in self.operands:
            if op.type in ["Reg", "RM", "Areg", "Creg", "Dreg"]:
                if op.size == 64:
                    self.cpu.add("64")
                elif op.size == 32 and "64" not in self.cpu:
                    self.cpu.add("386")
            if op.type in ["Imm", "ImmNotSegOff"]:
                if op.size == 64:
                    self.cpu.add("64")
                elif op.size == 32 and "64" not in self.cpu:
                    self.cpu.add("386")
            if op.type in ["FS", "GS"] and "64" not in self.cpu:
                self.cpu.add("386")
            if op.type in ["CR4"] and "64" not in self.cpu:
                self.cpu.add("586")
            if op.dest == "EA64":
                self.cpu.add("64")

        # Modifiers
        self.modifiers = kwargs.pop("modifiers", [])

        # GAS flags
        self.gas_only = ("nasm" not in self.parsers)
        self.gas_illegal = ("gas" not in self.parsers)
        self.gas_no_rev = (kwargs.pop("gas_no_reverse", False) or
                           kwargs.pop("gas_no_rev", False))

        # CPU feature flags finalization
        # Remove redundancies
        maxcpu = -1
        if "64" in self.cpu:
            pass #maxcpu = ordered_cpus.index("Hammer")
        else:
            maxcpu_set = self.cpu & set(ordered_cpus)
            if maxcpu_set:
                maxcpu = max(ordered_cpus.index(x) for x in maxcpu_set)
        if maxcpu != -1:
            for cpu in ordered_cpus[0:maxcpu]:
                self.cpu.discard(cpu)

        if kwargs:
            for arg in kwargs:
                print "Warning: unrecognized arg %s" % arg

    def __str__(self):
        if hasattr(self, "opcode"):
            opcodes_str = ["0x%02X" % x for x in self.opcode]
        elif hasattr(self, "opcode1") and hasattr(self, "opcode2"):
            opcodes_str = ["0x%02X" % x for x in self.opcode1]
            opcodes_str.extend("0x%02X" % x for x in self.opcode2)
        # Ensure opcodes initializer string is 3 long
        opcodes_str.extend(["0", "0", "0"])
        opcodes_str = "{" + ', '.join(opcodes_str[0:3]) + "}"

        cpus_str = "|".join("CPU_%s" % x for x in sorted(self.cpu))

        if len(self.modifiers) > 3:
            raise ValueError("too many modifiers: %s" % (self.modifiers,))

        cpus_str = []
        if self.cpu is not None:
            if len(self.cpu) > 3:
                raise ValueError("too many CPUs: %s" % (self.cpu,))

            # Ensure CPUs initializer string is at least 3 long
            cpus_str.extend("CPU_%s" % x for x in sorted(self.cpu))

        # Ensure cpus initializer string is 3 long; 0=CPU_Any
        cpus_str.extend(["0", "0", "0"])

        # Number gap elements as we copy
        modnames = []
        n = 0
        for mod in self.modifiers:
            if mod == "Gap":
                modnames.append("Gap%d" % n)
                n += 1
            else:
                modnames.append(mod)

        # Order by priority
        mods = [x for x in ["Gap0", "Op2Add", "Gap1", "Op1Add", "Gap2",
                            "Op0Add", "PreAdd", "SpAdd", "OpSizeR", "Imm8",
                            "AdSizeR", "DOpS64R", "Op1AddSp"]
                if x in modnames]

        if self.gas_only:
            mods.append("GasOnly")
        if self.gas_illegal:
            mods.append("GasIllegal")
        if self.gas_no_rev:
            mods.append("GasNoRev")
        if self.suffixes:
            mods.extend("GasSuf%s" % x for x in sorted(self.suffixes))
        mod_str = "|".join("MOD_%s" % x for x in mods)

        # Build instruction info structure initializer
        return "{ "+ ", ".join([cpus_str[0],
                                cpus_str[1],
                                cpus_str[2],
                                mod_str or "0",
                                "%d" % (self.opersize or 0),
                                "%d" % (self.def_opersize_64 or 0),
                                self.special_prefix or "0",
                                "%d" % self.opcode_len,
                                opcodes_str,
                                "%d" % (self.spare or 0),
                                "%d" % len(self.operands),
                                "%d" % self.all_operands_index]) + " }"

groups = {}
groupnames_ordered = []
def add_group(name, **kwargs):
    forms = groups.setdefault(name, [])
    forms.append(GroupForm(**kwargs))
    groupnames_ordered.append(name)

class Insn(object):
    def __init__(self, groupname, suffix=None, parser=None, modifiers=None,
                 cpu=None, only64=False, not64=False):
        self.groupname = groupname
        if suffix is None:
            self.suffix = None
        else:
            self.suffix = suffix.upper()

        self.parsers = None
        if suffix is not None:
            self.parsers = set(["gas"])
        if parser is not None:
            self.parsers = set([parser])

        if modifiers is None:
            self.modifiers = []
        else:
            self.modifiers = modifiers
        if cpu is None:
            self.cpu = None
        else:
            self.cpu = set(cpu)

        if only64:
            if self.cpu is None:
                self.cpu = set()
            self.cpu.add("64")
        if not64:
            if self.cpu is None:
                self.cpu = set()
            self.cpu.add("Not64")

    def auto_cpu(self, parser):
        if self.cpu is not None:
            return
        """Determine lowest common denominator CPU from group and suffix.
        Does nothing if CPU is already set."""
        # Scan through group, matching parser and suffix
        for form in groups[self.groupname]:
            if parser not in form.parsers:
                continue
            if (self.suffix is not None and len(self.suffix) == 1 and
                (form.suffixes is None or self.suffix not in form.suffixes)):
                continue
            if self.cpu is None:
                self.cpu = form.cpu
            else:
                self.cpu = cpu_lcd(self.cpu, form.cpu)

    def copy(self):
        """Return a shallow copy."""
        return Insn(self.groupname,
                    suffix=self.suffix,
                    modifiers=self.modifiers,
                    cpu=self.cpu)

    def __str__(self):
        if self.suffix is None:
            suffix_str = "NONE"
        elif len(self.suffix) == 1:
            suffix_str = "SUF_" + self.suffix
        else:
            suffix_str = self.suffix

        cpus_str = []
        if self.cpu is not None:
            if len(self.cpu) > 3:
                raise ValueError("too many CPUs: %s" % (self.cpu,))

            # Ensure CPUs initializer string is at least 3 long
            cpus_str.extend("CPU_%s" % x for x in sorted(self.cpu))

        # Ensure cpus initializer string is 3 long
        cpus_str.extend(["0", "0", "0"])

        if len(self.modifiers) > 3:
            raise ValueError("too many modifiers")

        # Ensure modifiers is at least 3 long
        modifiers = [x for x in self.modifiers]
        modifiers.extend([0, 0, 0])

        # Find longest list of modifiers in groups
        modnames = []
        for group in groups[self.groupname]:
            if group.modifiers is not None and len(group.modifiers) > len(modnames):
                # Number gap elements as we copy
                modnames = []
                n = 0
                for mod in group.modifiers:
                    if mod == "Gap":
                        modnames.append("Gap%d" % n)
                        n += 1
                    else:
                        modnames.append(mod)

        # Match up and order by priority
        mods = dict(zip(modnames, modifiers))

        modifier_str = "".join(reversed(["%02X" % mods[x] for x in
                                         ["Gap0", "Op2Add", "Gap1", "Op1Add",
                                          "Gap2", "Op0Add", "PreAdd", "SpAdd",
                                          "OpSizeR", "Imm8", "AdSizeR", "DOpS64R",
                                          "Op1AddSp"] if x in mods]))
        if modifier_str:
            modifier_str = "0x" + modifier_str
        else:
            modifier_str = "0"

        return ",\t".join(["%s_insn" % self.groupname,
                           "(%sUL<<8)|%d" % \
                                (modifier_str, len(groups[self.groupname])),
                           suffix_str,
                           cpus_str[0],
                           cpus_str[1],
                           cpus_str[2]])

insns = {}
def add_insn(name, groupname, **kwargs):
    opts = insns.setdefault(name, [])
    opts.append(Insn(groupname, **kwargs))

class Prefix(object):
    def __init__(self, groupname, value, only64=False):
        self.groupname = groupname
        self.value = value
        self.only64 = only64

    def __str__(self):
        return ",\t".join(["NULL",
                           "X86_" + self.groupname,
                           "0x%02X" % self.value,
                           self.only64 and "CPU_64" or "0",
                           "0",
                           "0"])

gas_insns = {}
nasm_insns = {}

def add_prefix(name, groupname, value, parser=None, **kwargs):
    prefix = Prefix(groupname, value, **kwargs)
    if parser is None or parser == "gas":
        gas_insns[name] = prefix
    if parser is None or parser == "nasm":
        nasm_insns[name] = prefix

def finalize_insns():
    for name, opts in insns.iteritems():
        for insn in opts:
            group = groups[insn.groupname]

            parsers = set()
            for form in group:
                parsers |= form.parsers
            if insn.parsers is not None:
                parsers &= insn.parsers

            if "gas" in parsers:
                keyword = name
                if keyword in gas_insns:
                    raise ValueError("duplicate gas instruction %s" % keyword)
                newinsn = insn.copy()
                newinsn.auto_cpu("gas")
                gas_insns[keyword] = newinsn

                if insn.suffix is None:
                    suffixes = set()
                    for form in group:
                        if form.gen_suffix and form.suffixes is not None:
                            suffixes |= form.suffixes

                    for suffix in suffixes:
                        keyword = name+suffix
                        if keyword in gas_insns:
                            raise ValueError("duplicate gas instruction %s" %
                                             keyword)
                        newinsn = insn.copy()
                        newinsn.suffix = suffix
                        newinsn.auto_cpu("gas")
                        gas_insns[keyword] = newinsn

            if "nasm" in parsers:
                keyword = name
                if keyword in nasm_insns:
                    raise ValueError("duplicate nasm instruction %s" % keyword)
                newinsn = insn.copy()
                newinsn.auto_cpu("nasm")
                nasm_insns[keyword] = newinsn

def output_insns(f, parser, insns):
    print >>f, """%%ignore-case
%%language=ANSI-C
%%compare-strncmp
%%readonly-tables
%%enum
%%struct-type
%%define hash-function-name insnprefix_%s_hash
%%define lookup-function-name insnprefix_%s_find
struct insnprefix_parse_data;
%%%%""" % (parser, parser)
    for keyword in sorted(insns):
        print >>f, "%s,\t%s" % (keyword.lower(), insns[keyword])

def output_gas_insns(f):
    output_insns(f, "gas", gas_insns)

def output_nasm_insns(f):
    output_insns(f, "nasm", nasm_insns)

def output_groups(f):
    # Merge all operand lists into single list
    # Sort by number of operands to shorten output
    all_operands = []
    for form in sorted((form for g in groups.itervalues() for form in g),
                       key=lambda x:len(x.operands), reverse=True):
        num_operands = len(form.operands)
        for i in xrange(len(all_operands)):
            if all_operands[i:i+num_operands] == form.operands:
                form.all_operands_index = i
                break
        else:
            form.all_operands_index = len(all_operands)
            all_operands.extend(form.operands)

    # Output operands list
    print >>f, "static const unsigned long insn_operands[] = {"
    print >>f, "   ",
    print >>f, ",\n    ".join(str(x) for x in all_operands)
    print >>f, "};\n"

    # Output groups
    seen = set()
    for name in groupnames_ordered:
        if name in seen:
            continue
        seen.add(name)
        print >>f, "static const x86_insn_info %s_insn[] = {" % name
        print >>f, "   ",
        print >>f, ",\n    ".join(str(x) for x in groups[name])
        print >>f, "};\n"

#####################################################################
# General instruction groupings
#####################################################################

#
# Empty instruction
#
add_group("empty", opcode=[], operands=[])

#
# Placeholder for instructions invalid in 64-bit mode
#
add_group("not64", opcode=[], operands=[], not64=True)

#
# One byte opcode instructions with no operands
#
add_group("onebyte",
    modifiers=["Op0Add", "OpSizeR", "DOpS64R"],
    opcode=[0x00],
    operands=[])

#
# One byte opcode instructions with "special" prefix with no operands
#
add_group("onebyte_prefix",
    modifiers=["PreAdd", "Op0Add"],
    prefix=0x00,
    opcode=[0x00],
    operands=[])

#
# Two byte opcode instructions with no operands
#
add_group("twobyte",
    gen_suffix=False,
    suffixes=["l", "q"],
    modifiers=["Op0Add", "Op1Add"],
    opcode=[0x00, 0x00],
    operands=[])

#
# Three byte opcode instructions with no operands
#
add_group("threebyte",
    modifiers=["Op0Add", "Op1Add", "Op2Add"],
    opcode=[0x00, 0x00, 0x00],
    operands=[])

#
# One byte opcode instructions with general memory operand
#
add_group("onebytemem",
    gen_suffix=False,
    suffixes=["l", "q", "s"],
    modifiers=["SpAdd", "Op0Add"],
    opcode=[0x00],
    spare=0,
    operands=[Operand(type="Mem", dest="EA")])

#
# Two byte opcode instructions with general memory operand
#
add_group("twobytemem",
    gen_suffix=False,
    suffixes=["w", "l", "q", "s"],
    modifiers=["SpAdd", "Op0Add", "Op1Add"],
    opcode=[0x00, 0x00],
    spare=0,
    operands=[Operand(type="Mem", dest="EA")])

#
# mov
#

# Absolute forms for non-64-bit mode
for sfx, sz in zip("bwl", [8, 16, 32]):
    add_group("mov",
        suffix=sfx,
        not64=True,
        opersize=sz,
        opcode=[0xA0+(sz!=8)],
        operands=[Operand(type="Areg", size=sz, dest=None),
                  Operand(type="MemOffs", size=sz, relaxed=True, dest="EA")])

for sfx, sz in zip("bwl", [8, 16, 32]):
    add_group("mov",
        suffix=sfx,
        not64=True,
        opersize=sz,
        opcode=[0xA2+(sz!=8)],
        operands=[Operand(type="MemOffs", size=sz, relaxed=True, dest="EA"),
                  Operand(type="Areg", size=sz, dest=None)])

# 64-bit absolute forms for 64-bit mode.  Disabled for GAS, see movabs
for sz in (8, 16, 32, 64):
    add_group("mov",
        opersize=sz,
        opcode=[0xA0+(sz!=8)],
        only64=True,
        operands=[Operand(type="Areg", size=sz, dest=None),
                  Operand(type="MemOffs", size=sz, relaxed=True, dest="EA64")])

for sz in (8, 16, 32, 64):
    add_group("mov",
        only64=True,
        opersize=sz,
        opcode=[0xA2+(sz!=8)],
        operands=[Operand(type="MemOffs", size=sz, relaxed=True, dest="EA64"),
                  Operand(type="Areg", size=sz, dest=None)])

# General 32-bit forms using Areg / short absolute option
for sfx, sz in zip("bwlq", [8, 16, 32, 64]):
    add_group("mov",
        suffix=sfx,
        opersize=sz,
        opcode1=[0x88+(sz!=8)],
        opcode2=[0xA2+(sz!=8)],
        operands=[
            Operand(type="RM", size=sz, relaxed=True, dest="EA", opt="ShortMov"),
            Operand(type="Areg", size=sz, dest="Spare")])

# General 32-bit forms
for sfx, sz in zip("bwlq", [8, 16, 32, 64]):
    add_group("mov",
        suffix=sfx,
        opersize=sz,
        opcode=[0x88+(sz!=8)],
        operands=[Operand(type="RM", size=sz, relaxed=True, dest="EA"),
                  Operand(type="Reg", size=sz, dest="Spare")])

# General 32-bit forms using Areg / short absolute option
for sfx, sz in zip("bwlq", [8, 16, 32, 64]):
    add_group("mov",
        suffix=sfx,
        opersize=sz,
        opcode1=[0x8A+(sz!=8)],
        opcode2=[0xA0+(sz!=8)],
        operands=[Operand(type="Areg", size=sz, dest="Spare"),
                  Operand(type="RM", size=sz, relaxed=True, dest="EA",
                          opt="ShortMov")])

# General 32-bit forms
for sfx, sz in zip("bwlq", [8, 16, 32, 64]):
    add_group("mov",
        suffix=sfx,
        opersize=sz,
        opcode=[0x8A+(sz!=8)],
        operands=[Operand(type="Reg", size=sz, dest="Spare"),
                  Operand(type="RM", size=sz, relaxed=True, dest="EA")])

# Segment register forms
add_group("mov",
    suffix="w",
    opcode=[0x8C],
    operands=[Operand(type="Mem", size=16, relaxed=True, dest="EA"),
              Operand(type="SegReg", size=16, relaxed=True, dest="Spare")])
for sfx, sz in zip("wlq", [16, 32, 64]):
    add_group("mov",
        suffix=sfx,
        opersize=sz,
        opcode=[0x8C],
        operands=[
            Operand(type="Reg", size=sz, dest="EA"),
            Operand(type="SegReg", size=16, relaxed=True, dest="Spare")])
add_group("mov",
    suffix="w",
    opcode=[0x8E],
    operands=[Operand(type="SegReg", size=16, relaxed=True, dest="Spare"),
              Operand(type="RM", size=16, relaxed=True, dest="EA")])
for sfx, sz in zip("lq", [32, 64]):
    add_group("mov",
        suffix=sfx,
        opcode=[0x8E],
        operands=[
            Operand(type="SegReg", size=16, relaxed=True, dest="Spare"),
            Operand(type="Reg", size=sz, dest="EA")])

# Immediate forms
add_group("mov",
    suffix="b",
    opcode=[0xB0],
    operands=[Operand(type="Reg", size=8, dest="Op0Add"),
              Operand(type="Imm", size=8, relaxed=True, dest="Imm")])
for sfx, sz in zip("wl", [16, 32]):
    add_group("mov",
        suffix=sfx,
        opersize=sz,
        opcode=[0xB8],
        operands=[Operand(type="Reg", size=sz, dest="Op0Add"),
                  Operand(type="Imm", size=sz, relaxed=True, dest="Imm")])
# 64-bit forced size form
add_group("mov",
    parsers=["nasm"],
    opersize=64,
    opcode=[0xB8],
    operands=[Operand(type="Reg", size=64, dest="Op0Add"),
              Operand(type="Imm", size=64, dest="Imm")])
add_group("mov",
    suffix="q",
    opersize=64,
    opcode1=[0xB8],
    opcode2=[0xC7],
    operands=[Operand(type="Reg", size=64, dest="Op0Add"),
              Operand(type="Imm", size=64, relaxed=True, dest="Imm",
                      opt="SImm32Avail")])
# Need two sets here, one for strictness on left side, one for right.
for sfx, sz, immsz in zip("bwlq", [8, 16, 32, 64], [8, 16, 32, 32]):
    add_group("mov",
        suffix=sfx,
        opersize=sz,
        opcode=[0xC6+(sz!=8)],
        operands=[Operand(type="RM", size=sz, relaxed=True, dest="EA"),
                  Operand(type="Imm", size=immsz, dest="Imm")])
for sfx, sz, immsz in zip("bwlq", [8, 16, 32, 64], [8, 16, 32, 32]):
    add_group("mov",
            suffix=sfx,
            opersize=sz,
            opcode=[0xC6+(sz!=8)],
            operands=[Operand(type="RM", size=sz, dest="EA"),
                Operand(type="Imm", size=immsz, relaxed=True, dest="Imm")])

# CR forms
add_group("mov",
    suffix="l",
    not64=True,
    cpu=["Priv"],
    opcode=[0x0F, 0x22],
    operands=[Operand(type="CR4", size=32, dest="Spare"),
              Operand(type="Reg", size=32, dest="EA")])
add_group("mov",
    suffix="l",
    not64=True,
    cpu=["Priv"],
    opcode=[0x0F, 0x22],
    operands=[Operand(type="CRReg", size=32, dest="Spare"),
              Operand(type="Reg", size=32, dest="EA")])
add_group("mov",
    suffix="q",
    cpu=["Priv"],
    opcode=[0x0F, 0x22],
    operands=[Operand(type="CRReg", size=32, dest="Spare"),
              Operand(type="Reg", size=64, dest="EA")])
add_group("mov",
    suffix="l",
    not64=True,
    cpu=["Priv"],
    opcode=[0x0F, 0x20],
    operands=[Operand(type="Reg", size=32, dest="EA"),
              Operand(type="CR4", size=32, dest="Spare")])
add_group("mov",
    suffix="l",
    cpu=["Priv"],
    not64=True,
    opcode=[0x0F, 0x20],
    operands=[Operand(type="Reg", size=32, dest="EA"),
              Operand(type="CRReg", size=32, dest="Spare")])
add_group("mov",
    suffix="q",
    cpu=["Priv"],
    opcode=[0x0F, 0x20],
    operands=[Operand(type="Reg", size=64, dest="EA"),
              Operand(type="CRReg", size=32, dest="Spare")])

# DR forms
add_group("mov",
    suffix="l",
    not64=True,
    cpu=["Priv"],
    opcode=[0x0F, 0x23],
    operands=[Operand(type="DRReg", size=32, dest="Spare"),
              Operand(type="Reg", size=32, dest="EA")])
add_group("mov",
    suffix="q",
    cpu=["Priv"],
    opcode=[0x0F, 0x23],
    operands=[Operand(type="DRReg", size=32, dest="Spare"),
              Operand(type="Reg", size=64, dest="EA")])
add_group("mov",
    suffix="l",
    not64=True,
    cpu=["Priv"],
    opcode=[0x0F, 0x21],
    operands=[Operand(type="Reg", size=32, dest="EA"),
              Operand(type="DRReg", size=32, dest="Spare")])
add_group("mov",
    suffix="q",
    cpu=["Priv"],
    opcode=[0x0F, 0x21],
    operands=[Operand(type="Reg", size=64, dest="EA"),
              Operand(type="DRReg", size=32, dest="Spare")])

# MMX forms for GAS parser (copied from movq)
add_group("mov",
    suffix="q",
    cpu=["MMX"],
    parsers=["gas"],
    opcode=[0x0F, 0x6F],
    operands=[Operand(type="SIMDReg", size=64, dest="Spare"),
              Operand(type="SIMDRM", size=64, relaxed=True, dest="EA")])
add_group("mov",
    suffix="q",
    cpu=["MMX"],
    parsers=["gas"],
    opersize=64,
    opcode=[0x0F, 0x6E],
    operands=[Operand(type="SIMDReg", size=64, dest="Spare"),
              Operand(type="RM", size=64, relaxed=True, dest="EA")])
add_group("mov",
    suffix="q",
    cpu=["MMX"],
    parsers=["gas"],
    opcode=[0x0F, 0x7F],
    operands=[Operand(type="SIMDRM", size=64, relaxed=True, dest="EA"),
              Operand(type="SIMDReg", size=64, dest="Spare")])
add_group("mov",
    suffix="q",
    cpu=["MMX"],
    parsers=["gas"],
    opersize=64,
    opcode=[0x0F, 0x7E],
    operands=[Operand(type="RM", size=64, relaxed=True, dest="EA"),
              Operand(type="SIMDReg", size=64, dest="Spare")])

# SSE2 forms for GAS parser (copied from movq)
add_group("mov",
    suffix="q",
    cpu=["SSE2"],
    parsers=["gas"],
    prefix=0xF3,
    opcode=[0x0F, 0x7E],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="SIMDReg", size=128, dest="EA")])
add_group("mov",
    suffix="q",
    cpu=["SSE2"],
    parsers=["gas"],
    prefix=0xF3,
    opcode=[0x0F, 0x7E],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="SIMDRM", size=64, relaxed=True, dest="EA")])
add_group("mov",
    suffix="q",
    cpu=["SSE2"],
    parsers=["gas"],
    opersize=64,
    prefix=0x66,
    opcode=[0x0F, 0x6E],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="RM", size=64, relaxed=True, dest="EA")])
add_group("mov",
    suffix="q",
    cpu=["SSE2"],
    parsers=["gas"],
    prefix=0x66,
    opcode=[0x0F, 0xD6],
    operands=[Operand(type="SIMDRM", size=64, relaxed=True, dest="EA"),
              Operand(type="SIMDReg", size=128, dest="Spare")])
add_group("mov",
    suffix="q",
    cpu=["SSE2"],
    parsers=["gas"],
    opersize=64,
    prefix=0x66,
    opcode=[0x0F, 0x7E],
    operands=[Operand(type="RM", size=64, relaxed=True, dest="EA"),
              Operand(type="SIMDReg", size=128, dest="Spare")])

add_insn("mov", "mov")

#
# 64-bit absolute move (for GAS).
# These are disabled for GAS for normal mov above.
#
add_group("movabs",
    suffix="b",
    only64=True,
    opcode=[0xA0],
    operands=[Operand(type="Areg", size=8, dest=None),
              Operand(type="MemOffs", size=8, relaxed=True, dest="EA64")])
for sfx, sz in zip("wlq", [16, 32, 64]):
    add_group("movabs",
        only64=True,
        suffix=sfx,
        opersize=sz,
        opcode=[0xA1],
        operands=[Operand(type="Areg", size=sz, dest=None),
                  Operand(type="MemOffs", size=sz, relaxed=True,
                          dest="EA64")])

add_group("movabs",
    suffix="b",
    only64=True,
    opcode=[0xA2],
    operands=[Operand(type="MemOffs", size=8, relaxed=True, dest="EA64"),
              Operand(type="Areg", size=8, dest=None)])
for sfx, sz in zip("wlq", [16, 32, 64]):
    add_group("movabs",
        suffix=sfx,
        only64=True,
        opersize=sz,
        opcode=[0xA3],
        operands=[Operand(type="MemOffs", size=sz, relaxed=True,
                          dest="EA64"),
                  Operand(type="Areg", size=sz, dest=None)])

# 64-bit immediate form
add_group("movabs",
    suffix="q",
    opersize=64,
    opcode=[0xB8],
    operands=[Operand(type="Reg", size=64, dest="Op0Add"),
              Operand(type="Imm", size=64, relaxed=True, dest="Imm")])

add_insn("movabs", "movabs", parser="gas")

#
# Move with sign/zero extend
#
add_group("movszx",
    suffix="b",
    cpu=["386"],
    modifiers=["Op1Add"],
    opersize=16,
    opcode=[0x0F, 0x00],
    operands=[Operand(type="Reg", size=16, dest="Spare"),
              Operand(type="RM", size=8, relaxed=True, dest="EA")])
add_group("movszx",
    suffix="b",
    cpu=["386"],
    modifiers=["Op1Add"],
    opersize=32,
    opcode=[0x0F, 0x00],
    operands=[Operand(type="Reg", size=32, dest="Spare"),
              Operand(type="RM", size=8, dest="EA")])
add_group("movszx",
    suffix="b",
    modifiers=["Op1Add"],
    opersize=64,
    opcode=[0x0F, 0x00],
    operands=[Operand(type="Reg", size=64, dest="Spare"),
              Operand(type="RM", size=8, dest="EA")])
add_group("movszx",
    suffix="w",
    cpu=["386"],
    modifiers=["Op1Add"],
    opersize=32,
    opcode=[0x0F, 0x01],
    operands=[Operand(type="Reg", size=32, dest="Spare"),
              Operand(type="RM", size=16, dest="EA")])
add_group("movszx",
    suffix="w",
    modifiers=["Op1Add"],
    opersize=64,
    opcode=[0x0F, 0x01],
    operands=[Operand(type="Reg", size=64, dest="Spare"),
              Operand(type="RM", size=16, dest="EA")])

add_insn("movsbw", "movszx", suffix="b", modifiers=[0xBE])
add_insn("movsbl", "movszx", suffix="b", modifiers=[0xBE])
add_insn("movswl", "movszx", suffix="w", modifiers=[0xBE])
add_insn("movsbq", "movszx", suffix="b", modifiers=[0xBE], only64=True)
add_insn("movswq", "movszx", suffix="w", modifiers=[0xBE], only64=True)
add_insn("movsx", "movszx", modifiers=[0xBE])
add_insn("movzbw", "movszx", suffix="b", modifiers=[0xB6])
add_insn("movzbl", "movszx", suffix="b", modifiers=[0xB6])
add_insn("movzwl", "movszx", suffix="w", modifiers=[0xB6])
add_insn("movzbq", "movszx", suffix="b", modifiers=[0xB6], only64=True)
add_insn("movzwq", "movszx", suffix="w", modifiers=[0xB6], only64=True)
add_insn("movzx", "movszx", modifiers=[0xB6])

#
# Move with sign-extend doubleword (64-bit mode only)
#
add_group("movsxd",
    suffix="l",
    opersize=64,
    opcode=[0x63],
    operands=[Operand(type="Reg", size=64, dest="Spare"),
              Operand(type="RM", size=32, dest="EA")])

add_insn("movslq", "movsxd", suffix="l")
add_insn("movsxd", "movsxd", parser="nasm")

#
# Push instructions
#
add_group("push",
    suffix="w",
    opersize=16,
    def_opersize_64=64,
    opcode=[0x50],
    operands=[Operand(type="Reg", size=16, dest="Op0Add")])
add_group("push",
    suffix="l",
    not64=True,
    opersize=32,
    opcode=[0x50],
    operands=[Operand(type="Reg", size=32, dest="Op0Add")])
add_group("push",
    suffix="q",
    def_opersize_64=64,
    opcode=[0x50],
    operands=[Operand(type="Reg", size=64, dest="Op0Add")])
add_group("push",
    suffix="w",
    opersize=16,
    def_opersize_64=64,
    opcode=[0xFF],
    spare=6,
    operands=[Operand(type="RM", size=16, dest="EA")])
add_group("push",
    suffix="l",
    not64=True,
    opersize=32,
    opcode=[0xFF],
    spare=6,
    operands=[Operand(type="RM", size=32, dest="EA")])
add_group("push",
    suffix="q",
    def_opersize_64=64,
    opcode=[0xFF],
    spare=6,
    operands=[Operand(type="RM", size=64, dest="EA")])
add_group("push",
    cpu=["186"],
    parsers=["nasm"],
    def_opersize_64=64,
    opcode=[0x6A],
    operands=[Operand(type="Imm", size=8, dest="SImm")])
add_group("push",
    cpu=["186"],
    parsers=["gas"],
    def_opersize_64=64,
    opcode=[0x6A],
    operands=[Operand(type="Imm", size=8, relaxed=True, dest="SImm")])
add_group("push",
    suffix="w",
    cpu=["186"],
    parsers=["gas"],
    opersize=16,
    def_opersize_64=64,
    opcode1=[0x6A],
    opcode2=[0x68],
    operands=[Operand(type="Imm", size=16, relaxed=True, dest="Imm",
                      opt="SImm8")])
add_group("push",
    suffix="l",
    not64=True,
    parsers=["gas"],
    opersize=32,
    opcode1=[0x6A],
    opcode2=[0x68],
    operands=[Operand(type="Imm", size=32, relaxed=True, dest="Imm",
                      opt="SImm8")])
add_group("push",
    suffix="q",
    only64=True,
    opersize=64,
    def_opersize_64=64,
    opcode1=[0x6A],
    opcode2=[0x68],
    operands=[Operand(type="Imm", size=32, relaxed=True, dest="SImm",
                      opt="SImm8")])
add_group("push",
    not64=True,
    cpu=["186"],
    parsers=["nasm"],
    opcode1=[0x6A],
    opcode2=[0x68],
    operands=[Operand(type="Imm", size="BITS", relaxed=True, dest="Imm",
                      opt="SImm8")])
# Need these when we don't match the BITS size, but they need to be
# below the above line so the optimizer can kick in by default.
add_group("push",
    cpu=["186"],
    parsers=["nasm"],
    opersize=16,
    def_opersize_64=64,
    opcode=[0x68],
    operands=[Operand(type="Imm", size=16, dest="Imm")])
add_group("push",
    not64=True,
    parsers=["nasm"],
    opersize=32,
    opcode=[0x68],
    operands=[Operand(type="Imm", size=32, dest="Imm")])
add_group("push",
    only64=True,
    parsers=["nasm"],
    opersize=64,
    def_opersize_64=64,
    opcode=[0x68],
    operands=[Operand(type="Imm", size=32, dest="SImm")])
add_group("push",
    not64=True,
    opcode=[0x0E],
    operands=[Operand(type="CS", dest=None)])
add_group("push",
    suffix="w",
    not64=True,
    opersize=16,
    opcode=[0x0E],
    operands=[Operand(type="CS", size=16, dest=None)])
add_group("push",
    suffix="l",
    not64=True,
    opersize=32,
    opcode=[0x0E],
    operands=[Operand(type="CS", size=32, dest=None)])
add_group("push",
    not64=True,
    opcode=[0x16],
    operands=[Operand(type="SS", dest=None)])
add_group("push",
    suffix="w",
    not64=True,
    opersize=16,
    opcode=[0x16],
    operands=[Operand(type="SS", size=16, dest=None)])
add_group("push",
    suffix="l",
    not64=True,
    opersize=32,
    opcode=[0x16],
    operands=[Operand(type="SS", size=32, dest=None)])
add_group("push",
    not64=True,
    opcode=[0x1E],
    operands=[Operand(type="DS", dest=None)])
add_group("push",
    suffix="w",
    not64=True,
    opersize=16,
    opcode=[0x1E],
    operands=[Operand(type="DS", size=16, dest=None)])
add_group("push",
    suffix="l",
    not64=True,
    opersize=32,
    opcode=[0x1E],
    operands=[Operand(type="DS", size=32, dest=None)])
add_group("push",
    not64=True,
    opcode=[0x06],
    operands=[Operand(type="ES", dest=None)])
add_group("push",
    suffix="w",
    not64=True,
    opersize=16,
    opcode=[0x06],
    operands=[Operand(type="ES", size=16, dest=None)])
add_group("push",
    suffix="l",
    not64=True,
    opersize=32,
    opcode=[0x06],
    operands=[Operand(type="ES", size=32, dest=None)])
add_group("push",
    opcode=[0x0F, 0xA0],
    operands=[Operand(type="FS", dest=None)])
add_group("push",
    suffix="w",
    opersize=16,
    opcode=[0x0F, 0xA0],
    operands=[Operand(type="FS", size=16, dest=None)])
add_group("push",
    suffix="l",
    opersize=32,
    opcode=[0x0F, 0xA0],
    operands=[Operand(type="FS", size=32, dest=None)])
add_group("push",
    opcode=[0x0F, 0xA8],
    operands=[Operand(type="GS", dest=None)])
add_group("push",
    suffix="w",
    opersize=16,
    opcode=[0x0F, 0xA8],
    operands=[Operand(type="GS", size=16, dest=None)])
add_group("push",
    suffix="l",
    opersize=32,
    opcode=[0x0F, 0xA8],
    operands=[Operand(type="GS", size=32, dest=None)])

add_insn("push", "push")
add_insn("pusha", "onebyte", modifiers=[0x60, 0], cpu=["186"], not64=True)
add_insn("pushad", "onebyte", parser="nasm", modifiers=[0x60, 32],
         cpu=["386"], not64=True)
add_insn("pushal", "onebyte", parser="gas", modifiers=[0x60, 32],
         cpu=["386"], not64=True)
add_insn("pushaw", "onebyte", modifiers=[0x60, 16], cpu=["186"], not64=True)

#
# Pop instructions
#
add_group("pop",
    suffix="w",
    opersize=16,
    def_opersize_64=64,
    opcode=[0x58],
    operands=[Operand(type="Reg", size=16, dest="Op0Add")])
add_group("pop",
    suffix="l",
    not64=True,
    opersize=32,
    opcode=[0x58],
    operands=[Operand(type="Reg", size=32, dest="Op0Add")])
add_group("pop",
    suffix="q",
    def_opersize_64=64,
    opcode=[0x58],
    operands=[Operand(type="Reg", size=64, dest="Op0Add")])
add_group("pop",
    suffix="w",
    opersize=16,
    def_opersize_64=64,
    opcode=[0x8F],
    operands=[Operand(type="RM", size=16, dest="EA")])
add_group("pop",
    suffix="l",
    not64=True,
    opersize=32,
    opcode=[0x8F],
    operands=[Operand(type="RM", size=32, dest="EA")])
add_group("pop",
    suffix="q",
    def_opersize_64=64,
    opcode=[0x8F],
    operands=[Operand(type="RM", size=64, dest="EA")])
# POP CS is debateably valid on the 8086, if obsolete and undocumented.
# We don't include it because it's VERY unlikely it will ever be used
# anywhere.  If someone really wants it they can db 0x0F it.
#add_group("pop",
#    cpu=["Undoc", "Obs"],
#    opcode=[0x0F],
#    operands=[Operand(type="CS", dest=None)])
add_group("pop",
    not64=True,
    opcode=[0x17],
    operands=[Operand(type="SS", dest=None)])
add_group("pop",
    not64=True,
    opersize=16,
    opcode=[0x17],
    operands=[Operand(type="SS", size=16, dest=None)])
add_group("pop",
    not64=True,
    opersize=32,
    opcode=[0x17],
    operands=[Operand(type="SS", size=32, dest=None)])
add_group("pop",
    not64=True,
    opcode=[0x1F],
    operands=[Operand(type="DS", dest=None)])
add_group("pop",
    not64=True,
    opersize=16,
    opcode=[0x1F],
    operands=[Operand(type="DS", size=16, dest=None)])
add_group("pop",
    not64=True,
    opersize=32,
    opcode=[0x1F],
    operands=[Operand(type="DS", size=32, dest=None)])
add_group("pop",
    not64=True,
    opcode=[0x07],
    operands=[Operand(type="ES", dest=None)])
add_group("pop",
    not64=True,
    opersize=16,
    opcode=[0x07],
    operands=[Operand(type="ES", size=16, dest=None)])
add_group("pop",
    not64=True,
    opersize=32,
    opcode=[0x07],
    operands=[Operand(type="ES", size=32, dest=None)])
add_group("pop",
    opcode=[0x0F, 0xA1],
    operands=[Operand(type="FS", dest=None)])
add_group("pop",
    opersize=16,
    opcode=[0x0F, 0xA1],
    operands=[Operand(type="FS", size=16, dest=None)])
add_group("pop",
    opersize=32,
    opcode=[0x0F, 0xA1],
    operands=[Operand(type="FS", size=32, dest=None)])
add_group("pop",
    opcode=[0x0F, 0xA9],
    operands=[Operand(type="GS", dest=None)])
add_group("pop",
    opersize=16,
    opcode=[0x0F, 0xA9],
    operands=[Operand(type="GS", size=16, dest=None)])
add_group("pop",
    opersize=32,
    opcode=[0x0F, 0xA9],
    operands=[Operand(type="GS", size=32, dest=None)])

add_insn("pop", "pop")
add_insn("popa", "onebyte", modifiers=[0x61, 0], cpu=["186"], not64=True)
add_insn("popad", "onebyte", parser="nasm", modifiers=[0x61, 32],
         cpu=["386"], not64=True)
add_insn("popal", "onebyte", parser="gas", modifiers=[0x61, 32],
         cpu=["386"], not64=True)
add_insn("popaw", "onebyte", modifiers=[0x61, 16], cpu=["186"], not64=True)

#
# Exchange instructions
#
add_group("xchg",
    suffix="b",
    opcode=[0x86],
    operands=[Operand(type="RM", size=8, relaxed=True, dest="EA"),
              Operand(type="Reg", size=8, dest="Spare")])
add_group("xchg",
    suffix="b",
    opcode=[0x86],
    operands=[Operand(type="Reg", size=8, dest="Spare"),
              Operand(type="RM", size=8, relaxed=True, dest="EA")])
# We could be extra-efficient in the 64-bit mode case here.
# XCHG AX, AX in 64-bit mode is a NOP, as it doesn't clear the
# high 48 bits of RAX. Thus we don't need the operand-size prefix.
# But this feels too clever, and probably not what the user really
# expects in the generated code, so we don't do it.
#add_group("xchg",
#    suffix="w",
#    only64=True,
#    opcode=[0x90],
#    operands=[Operand(type="Areg", size=16, dest=None),
#              Operand(type="AReg", size=16, dest="Op0Add")])
add_group("xchg",
    suffix="w",
    opersize=16,
    opcode=[0x90],
    operands=[Operand(type="Areg", size=16, dest=None),
              Operand(type="Reg", size=16, dest="Op0Add")])
add_group("xchg",
    suffix="w",
    opersize=16,
    opcode=[0x90],
    operands=[Operand(type="Reg", size=16, dest="Op0Add"),
              Operand(type="Areg", size=16, dest=None)])
add_group("xchg",
    suffix="w",
    opersize=16,
    opcode=[0x87],
    operands=[Operand(type="RM", size=16, relaxed=True, dest="EA"),
              Operand(type="Reg", size=16, dest="Spare")])
add_group("xchg",
    suffix="w",
    opersize=16,
    opcode=[0x87],
    operands=[Operand(type="Reg", size=16, dest="Spare"),
              Operand(type="RM", size=16, relaxed=True, dest="EA")])
# Be careful with XCHG EAX, EAX in 64-bit mode.  This needs to use
# the long form rather than the NOP form, as the long form clears
# the high 32 bits of RAX.  This makes all 32-bit forms in 64-bit
# mode have consistent operation.
#
# FIXME: due to a hard-to-fix bug in how we handle generating gas suffix CPU
# rules, this causes xchgl to be CPU_Any instead of CPU_386.  A hacky patch
# could fix it, but it's doubtful anyone will ever notice, so leave it.
add_group("xchg",
    suffix="l",
    only64=True,
    opersize=32,
    opcode=[0x87],
    operands=[Operand(type="Areg", size=32, dest="EA"),
              Operand(type="Areg", size=32, dest="Spare")])
add_group("xchg",
    suffix="l",
    opersize=32,
    opcode=[0x90],
    operands=[Operand(type="Areg", size=32, dest=None),
              Operand(type="Reg", size=32, dest="Op0Add")])
add_group("xchg",
    suffix="l",
    opersize=32,
    opcode=[0x90],
    operands=[Operand(type="Reg", size=32, dest="Op0Add"),
              Operand(type="Areg", size=32, dest=None)])
add_group("xchg",
    suffix="l",
    opersize=32,
    opcode=[0x87],
    operands=[Operand(type="RM", size=32, relaxed=True, dest="EA"),
              Operand(type="Reg", size=32, dest="Spare")])
add_group("xchg",
    suffix="l",
    opersize=32,
    opcode=[0x87],
    operands=[Operand(type="Reg", size=32, dest="Spare"),
              Operand(type="RM", size=32, relaxed=True, dest="EA")])
# Be efficient with XCHG RAX, RAX.
# This is a NOP and thus doesn't need the REX prefix.
add_group("xchg",
    suffix="q",
    only64=True,
    opcode=[0x90],
    operands=[Operand(type="Areg", size=64, dest=None),
              Operand(type="Areg", size=64, dest="Op0Add")])
add_group("xchg",
    suffix="q",
    opersize=64,
    opcode=[0x90],
    operands=[Operand(type="Areg", size=64, dest=None),
              Operand(type="Reg", size=64, dest="Op0Add")])
add_group("xchg",
    suffix="q",
    opersize=64,
    opcode=[0x90],
    operands=[Operand(type="Reg", size=64, dest="Op0Add"),
              Operand(type="Areg", size=64, dest=None)])
add_group("xchg",
    suffix="q",
    opersize=64,
    opcode=[0x87],
    operands=[Operand(type="RM", size=64, relaxed=True, dest="EA"),
              Operand(type="Reg", size=64, dest="Spare")])
add_group("xchg",
    suffix="q",
    opersize=64,
    opcode=[0x87],
    operands=[Operand(type="Reg", size=64, dest="Spare"),
              Operand(type="RM", size=64, relaxed=True, dest="EA")])

add_insn("xchg", "xchg")

#####################################################################
# In/out from ports
#####################################################################
add_group("in",
    suffix="b",
    opcode=[0xE4],
    operands=[Operand(type="Areg", size=8, dest=None),
              Operand(type="Imm", size=8, relaxed=True, dest="Imm")])
for sfx, sz in zip("wl", [16, 32]):
    add_group("in",
        suffix=sfx,
        opersize=sz,
        opcode=[0xE5],
        operands=[Operand(type="Areg", size=sz, dest=None),
                  Operand(type="Imm", size=8, relaxed=True, dest="Imm")])
add_group("in",
    suffix="b",
    opcode=[0xEC],
    operands=[Operand(type="Areg", size=8, dest=None),
              Operand(type="Dreg", size=16, dest=None)])
for sfx, sz in zip("wl", [16, 32]):
    add_group("in",
        suffix=sfx,
        opersize=sz,
        opcode=[0xED],
        operands=[Operand(type="Areg", size=sz, dest=None),
                  Operand(type="Dreg", size=16, dest=None)])
# GAS-only variants (implicit accumulator register)
add_group("in",
    suffix="b",
    parsers=["gas"],
    opcode=[0xE4],
    operands=[Operand(type="Imm", size=8, relaxed=True, dest="Imm")])
for sfx, sz in zip("wl", [16, 32]):
    add_group("in",
        suffix=sfx,
        parsers=["gas"],
        opersize=sz,
        opcode=[0xE5],
        operands=[Operand(type="Imm", size=8, relaxed=True, dest="Imm")])
add_group("in",
    suffix="b",
    parsers=["gas"],
    opcode=[0xEC],
    operands=[Operand(type="Dreg", size=16, dest=None)])
add_group("in",
    suffix="w",
    parsers=["gas"],
    opersize=16,
    opcode=[0xED],
    operands=[Operand(type="Dreg", size=16, dest=None)])
add_group("in",
    suffix="l",
    cpu=["386"],
    parsers=["gas"],
    opersize=32,
    opcode=[0xED],
    operands=[Operand(type="Dreg", size=16, dest=None)])

add_insn("in", "in")

add_group("out",
    suffix="b",
    opcode=[0xE6],
    operands=[Operand(type="Imm", size=8, relaxed=True, dest="Imm"),
              Operand(type="Areg", size=8, dest=None)])
for sfx, sz in zip("wl", [16, 32]):
    add_group("out",
        suffix=sfx,
        opersize=sz,
        opcode=[0xE7],
        operands=[Operand(type="Imm", size=8, relaxed=True, dest="Imm"),
                  Operand(type="Areg", size=sz, dest=None)])
add_group("out",
    suffix="b",
    opcode=[0xEE],
    operands=[Operand(type="Dreg", size=16, dest=None),
              Operand(type="Areg", size=8, dest=None)])
for sfx, sz in zip("wl", [16, 32]):
    add_group("out",
        suffix=sfx,
        opersize=sz,
        opcode=[0xEF],
        operands=[Operand(type="Dreg", size=16, dest=None),
                  Operand(type="Areg", size=sz, dest=None)])
# GAS-only variants (implicit accumulator register)
add_group("out",
    suffix="b",
    parsers=["gas"],
    opcode=[0xE6],
    operands=[Operand(type="Imm", size=8, relaxed=True, dest="Imm")])
add_group("out",
    suffix="w",
    parsers=["gas"],
    opersize=16,
    opcode=[0xE7],
    operands=[Operand(type="Imm", size=8, relaxed=True, dest="Imm")])
add_group("out",
    suffix="l",
    cpu=["386"],
    parsers=["gas"],
    opersize=32,
    opcode=[0xE7],
    operands=[Operand(type="Imm", size=8, relaxed=True, dest="Imm")])
add_group("out",
    suffix="b",
    parsers=["gas"],
    opcode=[0xEE],
    operands=[Operand(type="Dreg", size=16, dest=None)])
add_group("out",
    suffix="w",
    parsers=["gas"],
    opersize=16,
    opcode=[0xEF],
    operands=[Operand(type="Dreg", size=16, dest=None)])
add_group("out",
    suffix="l",
    cpu=["386"],
    parsers=["gas"],
    opersize=32,
    opcode=[0xEF],
    operands=[Operand(type="Dreg", size=16, dest=None)])

add_insn("out", "out")

#
# Load effective address
#
for sfx, sz in zip("wlq", [16, 32, 64]):
    add_group("lea",
        suffix=sfx,
        opersize=sz,
        opcode=[0x8D],
        operands=[Operand(type="Reg", size=sz, dest="Spare"),
                  Operand(type="Mem", size=sz, relaxed=True, dest="EA")])

add_insn("lea", "lea")

#
# Load segment registers from memory
#
for sfx, sz in zip("wl", [16, 32]):
    add_group("ldes",
        suffix=sfx,
        not64=True,
        modifiers=["Op0Add"],
        opersize=sz,
        opcode=[0x00],
        operands=[Operand(type="Reg", size=sz, dest="Spare"),
                  Operand(type="Mem", dest="EA")])

add_insn("lds", "ldes", modifiers=[0xC5])
add_insn("les", "ldes", modifiers=[0xC4])

for sfx, sz in zip("wl", [16, 32]):
    add_group("lfgss",
        suffix=sfx,
        cpu=["386"],
        modifiers=["Op1Add"],
        opersize=sz,
        opcode=[0x0F, 0x00],
        operands=[Operand(type="Reg", size=sz, dest="Spare"),
                  Operand(type="Mem", dest="EA")])

add_insn("lfs", "lfgss", modifiers=[0xB4])
add_insn("lgs", "lfgss", modifiers=[0xB5])
add_insn("lss", "lfgss", modifiers=[0xB2])

#
# Flags registers instructions
#
add_insn("clc", "onebyte", modifiers=[0xF8])
add_insn("cld", "onebyte", modifiers=[0xFC])
add_insn("cli", "onebyte", modifiers=[0xFA])
add_insn("clts", "twobyte", modifiers=[0x0F, 0x06], cpu=["286", "Priv"])
add_insn("cmc", "onebyte", modifiers=[0xF5])
add_insn("lahf", "onebyte", modifiers=[0x9F])
add_insn("sahf", "onebyte", modifiers=[0x9E])
add_insn("pushf", "onebyte", modifiers=[0x9C, 0, 64])
add_insn("pushfd", "onebyte", parser="nasm", modifiers=[0x9C, 32],
         cpu=["386"], not64=True)
add_insn("pushfl", "onebyte", parser="gas", modifiers=[0x9C, 32],
         cpu=["386"], not64=True)
add_insn("pushfw", "onebyte", modifiers=[0x9C, 16, 64])
add_insn("pushfq", "onebyte", modifiers=[0x9C, 64, 64], only64=True)
add_insn("popf", "onebyte", modifiers=[0x9D, 0, 64])
add_insn("popfd", "onebyte", parser="nasm", modifiers=[0x9D, 32],
         cpu=["386"], not64=True)
add_insn("popfl", "onebyte", parser="gas", modifiers=[0x9D, 32],
         cpu=["386"], not64=True)
add_insn("popfw", "onebyte", modifiers=[0x9D, 16, 64])
add_insn("popfq", "onebyte", modifiers=[0x9D, 64, 64], only64=True)
add_insn("stc", "onebyte", modifiers=[0xF9])
add_insn("std", "onebyte", modifiers=[0xFD])
add_insn("sti", "onebyte", modifiers=[0xFB])

#
# Arithmetic - general
#
add_group("arith",
    suffix="b",
    modifiers=["Op0Add"],
    opcode=[0x04],
    operands=[Operand(type="Areg", size=8, dest=None),
              Operand(type="Imm", size=8, relaxed=True, dest="Imm")])
for sfx, sz, immsz in zip("wlq", [16, 32, 64], [16, 32, 32]):
    add_group("arith",
        suffix=sfx,
        modifiers=["Op2Add", "Op1AddSp"],
        opersize=sz,
        opcode1=[0x83, 0xC0],
        opcode2=[0x05],
        operands=[Operand(type="Areg", size=sz, dest=None),
                  Operand(type="Imm", size=immsz, relaxed=True, dest="Imm",
                          opt="SImm8")])

add_group("arith",
    suffix="b",
    modifiers=["Gap", "SpAdd"],
    opcode=[0x80],
    spare=0,
    operands=[Operand(type="RM", size=8, dest="EA"),
              Operand(type="Imm", size=8, relaxed=True, dest="Imm")])
add_group("arith",
    suffix="b",
    modifiers=["Gap", "SpAdd"],
    opcode=[0x80],
    spare=0,
    operands=[Operand(type="RM", size=8, relaxed=True, dest="EA"),
              Operand(type="Imm", size=8, dest="Imm")])

add_group("arith",
    suffix="w",
    modifiers=["Gap", "SpAdd"],
    opersize=16,
    opcode=[0x83],
    spare=0,
    operands=[Operand(type="RM", size=16, dest="EA"),
              Operand(type="Imm", size=8, dest="SImm")])
add_group("arith",
    parsers=["nasm"],
    modifiers=["Gap", "SpAdd"],
    opersize=16,
    opcode1=[0x83],
    opcode2=[0x81],
    spare=0,
    operands=[Operand(type="RM", size=16, relaxed=True, dest="EA"),
              Operand(type="Imm", size=16, dest="Imm", opt="SImm8")])
add_group("arith",
    suffix="w",
    modifiers=["Gap", "SpAdd"],
    opersize=16,
    opcode1=[0x83],
    opcode2=[0x81],
    spare=0,
    operands=[
        Operand(type="RM", size=16, dest="EA"),
        Operand(type="Imm", size=16, relaxed=True, dest="Imm", opt="SImm8")])

add_group("arith",
    suffix="l",
    modifiers=["Gap", "SpAdd"],
    opersize=32,
    opcode=[0x83],
    spare=0,
    operands=[Operand(type="RM", size=32, dest="EA"),
              Operand(type="Imm", size=8, dest="SImm")])
# Not64 because we can't tell if add [], dword in 64-bit mode is supposed
# to be a qword destination or a dword destination.
add_group("arith",
    not64=True,
    parsers=["nasm"],
    modifiers=["Gap", "SpAdd"],
    opersize=32,
    opcode1=[0x83],
    opcode2=[0x81],
    spare=0,
    operands=[Operand(type="RM", size=32, relaxed=True, dest="EA"),
              Operand(type="Imm", size=32, dest="Imm", opt="SImm8")])
add_group("arith",
    suffix="l",
    modifiers=["Gap", "SpAdd"],
    opersize=32,
    opcode1=[0x83],
    opcode2=[0x81],
    spare=0,
    operands=[
        Operand(type="RM", size=32, dest="EA"),
        Operand(type="Imm", size=32, relaxed=True, dest="Imm", opt="SImm8")])

# No relaxed-RM mode for 64-bit destinations; see above Not64 comment.
add_group("arith",
    suffix="q",
    modifiers=["Gap", "SpAdd"],
    opersize=64,
    opcode=[0x83],
    spare=0,
    operands=[Operand(type="RM", size=64, dest="EA"),
              Operand(type="Imm", size=8, dest="SImm")])
add_group("arith",
    suffix="q",
    modifiers=["Gap", "SpAdd"],
    opersize=64,
    opcode1=[0x83],
    opcode2=[0x81],
    spare=0,
    operands=[
        Operand(type="RM", size=64, dest="EA"),
        Operand(type="Imm", size=32, relaxed=True, dest="Imm", opt="SImm8")])

for sfx, sz in zip("bwlq", [8, 16, 32, 64]):
    add_group("arith",
        suffix=sfx,
        modifiers=["Op0Add"],
        opersize=sz,
        opcode=[0x00+(sz!=8)],
        operands=[Operand(type="RM", size=sz, relaxed=True, dest="EA"),
                  Operand(type="Reg", size=sz, dest="Spare")])
for sfx, sz in zip("bwlq", [8, 16, 32, 64]):
    add_group("arith",
        suffix=sfx,
        modifiers=["Op0Add"],
        opersize=sz,
        opcode=[0x02+(sz!=8)],
        operands=[Operand(type="Reg", size=sz, dest="Spare"),
                  Operand(type="RM", size=sz, relaxed=True, dest="EA")])

add_insn("add", "arith", modifiers=[0x00, 0])
add_insn("or",  "arith", modifiers=[0x08, 1])
add_insn("adc", "arith", modifiers=[0x10, 2])
add_insn("sbb", "arith", modifiers=[0x18, 3])
add_insn("and", "arith", modifiers=[0x20, 4])
add_insn("sub", "arith", modifiers=[0x28, 5])
add_insn("xor", "arith", modifiers=[0x30, 6])
add_insn("cmp", "arith", modifiers=[0x38, 7])

#
# Arithmetic - inc/dec
#
add_group("incdec",
    suffix="b",
    modifiers=["Gap", "SpAdd"],
    opcode=[0xFE],
    spare=0,
    operands=[Operand(type="RM", size=8, dest="EA")])
for sfx, sz in zip("wl", [16, 32]):
    add_group("incdec",
        suffix=sfx,
        not64=True,
        modifiers=["Op0Add"],
        opersize=sz,
        opcode=[0x00],
        operands=[Operand(type="Reg", size=sz, dest="Op0Add")])
    add_group("incdec",
        suffix=sfx,
        modifiers=["Gap", "SpAdd"],
        opersize=sz,
        opcode=[0xFF],
        spare=0,
        operands=[Operand(type="RM", size=sz, dest="EA")])
add_group("incdec",
    suffix="q",
    modifiers=["Gap", "SpAdd"],
    opersize=64,
    opcode=[0xFF],
    spare=0,
    operands=[Operand(type="RM", size=64, dest="EA")])

add_insn("inc", "incdec", modifiers=[0x40, 0])
add_insn("dec", "incdec", modifiers=[0x48, 1])

#
# Arithmetic - mul/neg/not F6 opcodes
#
for sfx, sz in zip("bwlq", [8, 16, 32, 64]):
    add_group("f6",
        suffix=sfx,
        modifiers=["SpAdd"],
        opersize=sz,
        opcode=[0xF6+(sz!=8)],
        spare=0,
        operands=[Operand(type="RM", size=sz, dest="EA")])

add_insn("not", "f6", modifiers=[2])
add_insn("neg", "f6", modifiers=[3])
add_insn("mul", "f6", modifiers=[4])

#
# Arithmetic - div/idiv F6 opcodes
# These allow explicit accumulator in GAS mode.
#
for sfx, sz in zip("bwlq", [8, 16, 32, 64]):
    add_group("div",
        suffix=sfx,
        modifiers=["SpAdd"],
        opersize=sz,
        opcode=[0xF6+(sz!=8)],
        spare=0,
        operands=[Operand(type="RM", size=sz, dest="EA")])
# Versions with explicit accumulator
for sfx, sz in zip("bwlq", [8, 16, 32, 64]):
    add_group("div",
        suffix=sfx,
        modifiers=["SpAdd"],
        opersize=sz,
        opcode=[0xF6+(sz!=8)],
        spare=0,
        operands=[Operand(type="Areg", size=sz, dest=None),
                  Operand(type="RM", size=sz, dest="EA")])

add_insn("div", "div", modifiers=[6])
add_insn("idiv", "div", modifiers=[7])

#
# Arithmetic - test instruction
#
for sfx, sz, immsz in zip("bwlq", [8, 16, 32, 64], [8, 16, 32, 32]):
    add_group("test",
        suffix=sfx,
        opersize=sz,
        opcode=[0xA8+(sz!=8)],
        operands=[Operand(type="Areg", size=sz, dest=None),
                  Operand(type="Imm", size=immsz, relaxed=True, dest="Imm")])

for sfx, sz, immsz in zip("bwlq", [8, 16, 32, 64], [8, 16, 32, 32]):
    add_group("test",
        suffix=sfx,
        opersize=sz,
        opcode=[0xF6+(sz!=8)],
        operands=[Operand(type="RM", size=sz, dest="EA"),
                  Operand(type="Imm", size=immsz, relaxed=True, dest="Imm")])
    add_group("test",
        suffix=sfx,
        opersize=sz,
        opcode=[0xF6+(sz!=8)],
        operands=[Operand(type="RM", size=sz, relaxed=True, dest="EA"),
                  Operand(type="Imm", size=immsz, dest="Imm")])

for sfx, sz in zip("bwlq", [8, 16, 32, 64]):
    add_group("test",
        suffix=sfx,
        opersize=sz,
        opcode=[0x84+(sz!=8)],
        operands=[Operand(type="RM", size=sz, relaxed=True, dest="EA"),
                  Operand(type="Reg", size=sz, dest="Spare")])
for sfx, sz in zip("bwlq", [8, 16, 32, 64]):
    add_group("test",
        suffix=sfx,
        opersize=sz,
        opcode=[0x84+(sz!=8)],
        operands=[Operand(type="Reg", size=sz, dest="Spare"),
                  Operand(type="RM", size=sz, relaxed=True, dest="EA")])

add_insn("test", "test")

#
# Arithmetic - aad/aam
#
add_group("aadm",
    modifiers=["Op0Add"],
    opcode=[0xD4, 0x0A],
    operands=[])
add_group("aadm",
    modifiers=["Op0Add"],
    opcode=[0xD4],
    operands=[Operand(type="Imm", size=8, relaxed=True, dest="Imm")])

add_insn("aaa", "onebyte", modifiers=[0x37], not64=True)
add_insn("aas", "onebyte", modifiers=[0x3F], not64=True)
add_insn("daa", "onebyte", modifiers=[0x27], not64=True)
add_insn("das", "onebyte", modifiers=[0x2F], not64=True)
add_insn("aad", "aadm", modifiers=[0x01], not64=True)
add_insn("aam", "aadm", modifiers=[0x00], not64=True)

#
# Conversion instructions
#
add_insn("cbw", "onebyte", modifiers=[0x98, 16])
add_insn("cwde", "onebyte", modifiers=[0x98, 32], cpu=["386"])
add_insn("cdqe", "onebyte", modifiers=[0x98, 64], only64=True)
add_insn("cwd", "onebyte", modifiers=[0x99, 16])
add_insn("cdq", "onebyte", modifiers=[0x99, 32], cpu=["386"])
add_insn("cqo", "onebyte", modifiers=[0x99, 64], only64=True)

#
# Conversion instructions - GAS / AT&T naming
#
add_insn("cbtw", "onebyte", parser="gas", modifiers=[0x98, 16])
add_insn("cwtl", "onebyte", parser="gas", modifiers=[0x98, 32], cpu=["386"])
add_insn("cltq", "onebyte", parser="gas", modifiers=[0x98, 64], only64=True)
add_insn("cwtd", "onebyte", parser="gas", modifiers=[0x99, 16])
add_insn("cltd", "onebyte", parser="gas", modifiers=[0x99, 32], cpu=["386"])
add_insn("cqto", "onebyte", parser="gas", modifiers=[0x99, 64], only64=True)

#
# Arithmetic - imul
#
for sfx, sz in zip("bwlq", [8, 16, 32, 64]):
    add_group("imul",
        suffix=sfx,
        opersize=sz,
        opcode=[0xF6+(sz!=8)],
        spare=5,
        operands=[Operand(type="RM", size=sz, dest="EA")])
for sfx, sz in zip("wlq", [16, 32, 64]):
    add_group("imul",
        suffix=sfx,
        cpu=["386"],
        opersize=sz,
        opcode=[0x0F, 0xAF],
        operands=[Operand(type="Reg", size=sz, dest="Spare"),
                  Operand(type="RM", size=sz, relaxed=True, dest="EA")])
for sfx, sz in zip("wlq", [16, 32, 64]):
    add_group("imul",
        suffix=sfx,
        cpu=["186"],
        opersize=sz,
        opcode=[0x6B],
        operands=[Operand(type="Reg", size=sz, dest="Spare"),
                  Operand(type="RM", size=sz, relaxed=True, dest="EA"),
                  Operand(type="Imm", size=8, dest="SImm")])
for sfx, sz in zip("wlq", [16, 32, 64]):
    add_group("imul",
        suffix=sfx,
        cpu=["186"],
        opersize=sz,
        opcode=[0x6B],
        operands=[Operand(type="Reg", size=sz, dest="SpareEA"),
                  Operand(type="Imm", size=8, dest="SImm")])
for sfx, sz, immsz in zip("wlq", [16, 32, 64], [16, 32, 32]):
    add_group("imul",
        suffix=sfx,
        cpu=["186"],
        opersize=sz,
        opcode1=[0x6B],
        opcode2=[0x69],
        operands=[Operand(type="Reg", size=sz, dest="Spare"),
                  Operand(type="RM", size=sz, relaxed=True, dest="EA"),
                  Operand(type="Imm", size=immsz, relaxed=True, dest="SImm",
                          opt="SImm8")])
for sfx, sz, immsz in zip("wlq", [16, 32, 64], [16, 32, 32]):
    add_group("imul",
        suffix=sfx,
        cpu=["186"],
        opersize=sz,
        opcode1=[0x6B],
        opcode2=[0x69],
        operands=[Operand(type="Reg", size=sz, dest="SpareEA"),
                  Operand(type="Imm", size=immsz, relaxed=True, dest="SImm",
                          opt="SImm8")])

add_insn("imul", "imul")

#
# Shifts - standard
#
for sfx, sz in zip("bwlq", [8, 16, 32, 64]):
    add_group("shift",
        suffix=sfx,
        modifiers=["SpAdd"],
        opersize=sz,
        opcode=[0xD2+(sz!=8)],
        spare=0,
        operands=[Operand(type="RM", size=sz, dest="EA"),
                  Operand(type="Creg", size=8, dest=None)])
    add_group("shift",
        suffix=sfx,
        modifiers=["SpAdd"],
        opersize=sz,
        opcode=[0xD0+(sz!=8)],
        spare=0,
        operands=[Operand(type="RM", size=sz, dest="EA"),
                  Operand(type="Imm1", size=8, relaxed=True, dest=None)])
    add_group("shift",
        suffix=sfx,
        cpu=["186"],
        modifiers=["SpAdd"],
        opersize=sz,
        opcode=[0xC0+(sz!=8)],
        spare=0,
        operands=[Operand(type="RM", size=sz, dest="EA"),
                  Operand(type="Imm", size=8, relaxed=True, dest="Imm")])
# In GAS mode, single operands are equivalent to shifting by 1 forms
for sfx, sz in zip("bwlq", [8, 16, 32, 64]):
    add_group("shift",
        suffix=sfx,
        parsers=["gas"],
        modifiers=["SpAdd"],
        opersize=sz,
        opcode=[0xD0+(sz!=8)],
        spare=0,
        operands=[Operand(type="RM", size=sz, dest="EA")])

add_insn("rol", "shift", modifiers=[0])
add_insn("ror", "shift", modifiers=[1])
add_insn("rcl", "shift", modifiers=[2])
add_insn("rcr", "shift", modifiers=[3])
add_insn("sal", "shift", modifiers=[4])
add_insn("shl", "shift", modifiers=[4])
add_insn("shr", "shift", modifiers=[5])
add_insn("sar", "shift", modifiers=[7])

#
# Shifts - doubleword
#
for sfx, sz in zip("wlq", [16, 32, 64]):
    add_group("shlrd",
        suffix=sfx,
        cpu=["386"],
        modifiers=["Op1Add"],
        opersize=sz,
        opcode=[0x0F, 0x00],
        operands=[Operand(type="RM", size=sz, relaxed=True, dest="EA"),
                  Operand(type="Reg", size=sz, dest="Spare"),
                  Operand(type="Imm", size=8, relaxed=True, dest="Imm")])
    add_group("shlrd",
        suffix=sfx,
        cpu=["386"],
        modifiers=["Op1Add"],
        opersize=sz,
        opcode=[0x0F, 0x01],
        operands=[Operand(type="RM", size=sz, relaxed=True, dest="EA"),
                  Operand(type="Reg", size=sz, dest="Spare"),
                  Operand(type="Creg", size=8, dest=None)])
# GAS parser supports two-operand form for shift with CL count
for sfx, sz in zip("wlq", [16, 32, 64]):
    add_group("shlrd",
        suffix=sfx,
        cpu=["386"],
        parsers=["gas"],
        modifiers=["Op1Add"],
        opersize=sz,
        opcode=[0x0F, 0x01],
        operands=[Operand(type="RM", size=sz, relaxed=True, dest="EA"),
                  Operand(type="Reg", size=sz, dest="Spare")])

add_insn("shld", "shlrd", modifiers=[0xA4])
add_insn("shrd", "shlrd", modifiers=[0xAC])

#####################################################################
# Control transfer instructions (unconditional)
#####################################################################
#
# call
#
add_group("call",
    opcode=[],
    operands=[Operand(type="ImmNotSegOff", dest="JmpRel")])
add_group("call",
    opersize=16,
    opcode=[],
    operands=[Operand(type="ImmNotSegOff", size=16, dest="JmpRel")])
add_group("call",
    not64=True,
    opersize=32,
    opcode=[],
    operands=[Operand(type="ImmNotSegOff", size=32, dest="JmpRel")])
add_group("call",
    only64=True,
    opersize=64,
    opcode=[],
    operands=[Operand(type="ImmNotSegOff", size=32, dest="JmpRel")])

add_group("call",
    opersize=16,
    def_opersize_64=64,
    opcode=[0xE8],
    operands=[Operand(type="Imm", size=16, tmod="Near", dest="JmpRel")])
add_group("call",
    not64=True,
    opersize=32,
    opcode=[0xE8],
    operands=[Operand(type="Imm", size=32, tmod="Near", dest="JmpRel")])
add_group("call",
    only64=True,
    opersize=64,
    def_opersize_64=64,
    opcode=[0xE8],
    operands=[Operand(type="Imm", size=32, tmod="Near", dest="JmpRel")])
add_group("call",
    def_opersize_64=64,
    opcode=[0xE8],
    operands=[Operand(type="Imm", tmod="Near", dest="JmpRel")])

add_group("call",
    opersize=16,
    opcode=[0xFF],
    spare=2,
    operands=[Operand(type="RM", size=16, dest="EA")])
add_group("call",
    not64=True,
    opersize=32,
    opcode=[0xFF],
    spare=2,
    operands=[Operand(type="RM", size=32, dest="EA")])
add_group("call",
    opersize=64,
    def_opersize_64=64,
    opcode=[0xFF],
    spare=2,
    operands=[Operand(type="RM", size=64, dest="EA")])
add_group("call",
    def_opersize_64=64,
    opcode=[0xFF],
    spare=2,
    operands=[Operand(type="Mem", dest="EA")])
add_group("call",
    opersize=16,
    def_opersize_64=64,
    opcode=[0xFF],
    spare=2,
    operands=[Operand(type="RM", size=16, tmod="Near", dest="EA")])
add_group("call",
    not64=True,
    opersize=32,
    opcode=[0xFF],
    spare=2,
    operands=[Operand(type="RM", size=32, tmod="Near", dest="EA")])
add_group("call",
    opersize=64,
    def_opersize_64=64,
    opcode=[0xFF],
    spare=2,
    operands=[Operand(type="RM", size=64, tmod="Near", dest="EA")])
add_group("call",
    def_opersize_64=64,
    opcode=[0xFF],
    spare=2,
    operands=[Operand(type="Mem", tmod="Near", dest="EA")])

# Far indirect (through memory).  Needs explicit FAR override.
for sz in [16, 32, 64]:
    add_group("call",
        opersize=sz,
        opcode=[0xFF],
        spare=3,
        operands=[Operand(type="Mem", size=sz, tmod="Far", dest="EA")])
add_group("call",
    opcode=[0xFF],
    spare=3,
    operands=[Operand(type="Mem", tmod="Far", dest="EA")])

# With explicit FAR override
for sz in [16, 32]:
    add_group("call",
        not64=True,
        opersize=sz,
        opcode=[0x9A],
        spare=3,
        operands=[Operand(type="Imm", size=sz, tmod="Far", dest="JmpFar")])
add_group("call",
    not64=True,
    opcode=[0x9A],
    spare=3,
    operands=[Operand(type="Imm", tmod="Far", dest="JmpFar")])

# Since not caught by first ImmNotSegOff group, implicitly FAR.
for sz in [16, 32]:
    add_group("call",
        not64=True,
        opersize=sz,
        opcode=[0x9A],
        spare=3,
        operands=[Operand(type="Imm", size=sz, dest="JmpFar")])
add_group("call",
    not64=True,
    opcode=[0x9A],
    spare=3,
    operands=[Operand(type="Imm", dest="JmpFar")])

add_insn("call", "call")
add_insn("calll", "call", parser="gas", not64=True)
add_insn("callq", "call", parser="gas", only64=True)

#
# jmp
#
add_group("jmp",
    opcode=[],
    operands=[Operand(type="ImmNotSegOff", dest="JmpRel")])
add_group("jmp",
    opersize=16,
    opcode=[],
    operands=[Operand(type="ImmNotSegOff", size=16, dest="JmpRel")])
add_group("jmp",
    not64=True,
    opersize=32,
    opcode=[0x00],
    operands=[Operand(type="ImmNotSegOff", size=32, dest="JmpRel")])
add_group("jmp",
    only64=True,
    opersize=64,
    opcode=[0x00],
    operands=[Operand(type="ImmNotSegOff", size=32, dest="JmpRel")])

add_group("jmp",
    def_opersize_64=64,
    opcode=[0xEB],
    operands=[Operand(type="Imm", tmod="Short", dest="JmpRel")])
add_group("jmp",
    opersize=16,
    def_opersize_64=64,
    opcode=[0xE9],
    operands=[Operand(type="Imm", size=16, tmod="Near", dest="JmpRel")])
add_group("jmp",
    not64=True,
    cpu=["386"],
    opersize=32,
    opcode=[0xE9],
    operands=[Operand(type="Imm", size=32, tmod="Near", dest="JmpRel")])
add_group("jmp",
    only64=True,
    opersize=64,
    def_opersize_64=64,
    opcode=[0xE9],
    operands=[Operand(type="Imm", size=32, tmod="Near", dest="JmpRel")])
add_group("jmp",
    def_opersize_64=64,
    opcode=[0xE9],
    operands=[Operand(type="Imm", tmod="Near", dest="JmpRel")])

add_group("jmp",
    opersize=16,
    def_opersize_64=64,
    opcode=[0xFF],
    spare=4,
    operands=[Operand(type="RM", size=16, dest="EA")])
add_group("jmp",
    not64=True,
    opersize=32,
    opcode=[0xFF],
    spare=4,
    operands=[Operand(type="RM", size=32, dest="EA")])
add_group("jmp",
    opersize=64,
    def_opersize_64=64,
    opcode=[0xFF],
    spare=4,
    operands=[Operand(type="RM", size=64, dest="EA")])
add_group("jmp",
    def_opersize_64=64,
    opcode=[0xFF],
    spare=4,
    operands=[Operand(type="Mem", dest="EA")])
add_group("jmp",
    opersize=16,
    def_opersize_64=64,
    opcode=[0xFF],
    spare=4,
    operands=[Operand(type="RM", size=16, tmod="Near", dest="EA")])
add_group("jmp",
    not64=True,
    cpu=["386"],
    opersize=32,
    opcode=[0xFF],
    spare=4,
    operands=[Operand(type="RM", size=32, tmod="Near", dest="EA")])
add_group("jmp",
    opersize=64,
    def_opersize_64=64,
    opcode=[0xFF],
    spare=4,
    operands=[Operand(type="RM", size=64, tmod="Near", dest="EA")])
add_group("jmp",
    def_opersize_64=64,
    opcode=[0xFF],
    spare=4,
    operands=[Operand(type="Mem", tmod="Near", dest="EA")])

# Far indirect (through memory).  Needs explicit FAR override.
for sz in [16, 32, 64]:
    add_group("jmp",
        opersize=sz,
        opcode=[0xFF],
        spare=5,
        operands=[Operand(type="Mem", size=sz, tmod="Far", dest="EA")])
add_group("jmp",
    opcode=[0xFF],
    spare=5,
    operands=[Operand(type="Mem", tmod="Far", dest="EA")])

# With explicit FAR override
for sz in [16, 32]:
    add_group("jmp",
        not64=True,
        opersize=sz,
        opcode=[0xEA],
        spare=3,
        operands=[Operand(type="Imm", size=sz, tmod="Far", dest="JmpFar")])
add_group("jmp",
    not64=True,
    opcode=[0xEA],
    spare=3,
    operands=[Operand(type="Imm", tmod="Far", dest="JmpFar")])

# Since not caught by first ImmNotSegOff group, implicitly FAR.
for sz in [16, 32]:
    add_group("jmp",
        not64=True,
        opersize=sz,
        opcode=[0xEA],
        spare=3,
        operands=[Operand(type="Imm", size=sz, dest="JmpFar")])
add_group("jmp",
    not64=True,
    opcode=[0xEA],
    spare=3,
    operands=[Operand(type="Imm", dest="JmpFar")])

add_insn("jmp", "jmp")

#
# ret
#
add_group("retnf",
    not64=True,
    modifiers=["Op0Add"],
    opcode=[0x01],
    operands=[])
add_group("retnf",
    not64=True,
    modifiers=["Op0Add"],
    opcode=[0x00],
    operands=[Operand(type="Imm", size=16, relaxed=True, dest="Imm")])
add_group("retnf",
    only64=True,
    modifiers=["Op0Add", "OpSizeR"],
    opcode=[0x01],
    operands=[])
add_group("retnf",
    only64=True,
    modifiers=["Op0Add", "OpSizeR"],
    opcode=[0x00],
    operands=[Operand(type="Imm", size=16, relaxed=True, dest="Imm")])
add_group("retnf",
    gen_suffix=False,
    suffixes=["w", "l", "q"],
    modifiers=["Op0Add", "OpSizeR"],
    opcode=[0x01],
    operands=[])
# GAS suffix versions
add_group("retnf",
    gen_suffix=False,
    suffixes=["w", "l", "q"],
    modifiers=["Op0Add", "OpSizeR"],
    opcode=[0x00],
    operands=[Operand(type="Imm", size=16, relaxed=True, dest="Imm")])

add_insn("ret", "retnf", modifiers=[0xC2])
add_insn("retw", "retnf", parser="gas", modifiers=[0xC2, 16])
add_insn("retl", "retnf", parser="gas", modifiers=[0xC2], not64=True)
add_insn("retq", "retnf", parser="gas", modifiers=[0xC2], only64=True)
add_insn("retn", "retnf", parser="nasm", modifiers=[0xC2])
add_insn("retf", "retnf", parser="nasm", modifiers=[0xCA, 64])
add_insn("lretw", "retnf", parser="gas", modifiers=[0xCA, 16], suffix="NONE")
add_insn("lretl", "retnf", parser="gas", modifiers=[0xCA], suffix="NONE")
add_insn("lretq", "retnf", parser="gas", modifiers=[0xCA, 64], only64=True,
         suffix="NONE")

#
# enter
#
add_group("enter",
    suffix="l",
    not64=True,
    cpu=["186"],
    gas_no_reverse=True,
    opcode=[0xC8],
    operands=[
        Operand(type="Imm", size=16, relaxed=True, dest="EA", opt="A16"),
        Operand(type="Imm", size=8, relaxed=True, dest="Imm")])
add_group("enter",
    suffix="q",
    only64=True,
    cpu=["186"],
    gas_no_reverse=True,
    opersize=64,
    def_opersize_64=64,
    opcode=[0xC8],
    operands=[
        Operand(type="Imm", size=16, relaxed=True, dest="EA", opt="A16"),
        Operand(type="Imm", size=8, relaxed=True, dest="Imm")])
# GAS suffix version
add_group("enter",
    suffix="w",
    cpu=["186"],
    parsers=["gas"],
    gas_no_reverse=True,
    opersize=16,
    opcode=[0xC8],
    operands=[
        Operand(type="Imm", size=16, relaxed=True, dest="EA", opt="A16"),
        Operand(type="Imm", size=8, relaxed=True, dest="Imm")])

add_insn("enter", "enter")

#
# leave
#
add_insn("leave", "onebyte", modifiers=[0xC9, 0, 64], cpu=["186"])
add_insn("leavew", "onebyte", parser="gas", modifiers=[0xC9, 16, 0],
         cpu=["186"])
add_insn("leavel", "onebyte", parser="gas", modifiers=[0xC9, 0, 64],
         cpu=["186"])
add_insn("leaveq", "onebyte", parser="gas", modifiers=[0xC9, 0, 64],
         only64=True)

#####################################################################
# Conditional jumps
#####################################################################
add_group("jcc",
    opcode=[],
    operands=[Operand(type="Imm", dest="JmpRel")])
add_group("jcc",
    opersize=16,
    opcode=[],
    operands=[Operand(type="Imm", size=16, dest="JmpRel")])
add_group("jcc",
    not64=True,
    opersize=32,
    opcode=[],
    operands=[Operand(type="Imm", size=32, dest="JmpRel")])
add_group("jcc",
    only64=True,
    opersize=64,
    opcode=[],
    operands=[Operand(type="Imm", size=32, dest="JmpRel")])

add_group("jcc",
    modifiers=["Op0Add"],
    def_opersize_64=64,
    opcode=[0x70],
    operands=[Operand(type="Imm", tmod="Short", dest="JmpRel")])
add_group("jcc",
    cpu=["186"],
    modifiers=["Op1Add"],
    opersize=16,
    def_opersize_64=64,
    opcode=[0x0F, 0x80],
    operands=[Operand(type="Imm", size=16, tmod="Near", dest="JmpRel")])
add_group("jcc",
    not64=True,
    cpu=["386"],
    modifiers=["Op1Add"],
    opersize=32,
    opcode=[0x0F, 0x80],
    operands=[Operand(type="Imm", size=32, tmod="Near", dest="JmpRel")])
add_group("jcc",
    only64=True,
    modifiers=["Op1Add"],
    opersize=64,
    def_opersize_64=64,
    opcode=[0x0F, 0x80],
    operands=[Operand(type="Imm", size=32, tmod="Near", dest="JmpRel")])
add_group("jcc",
    cpu=["186"],
    modifiers=["Op1Add"],
    def_opersize_64=64,
    opcode=[0x0F, 0x80],
    operands=[Operand(type="Imm", tmod="Near", dest="JmpRel")])

add_insn("jo", "jcc", modifiers=[0x00])
add_insn("jno", "jcc", modifiers=[0x01])
add_insn("jb", "jcc", modifiers=[0x02])
add_insn("jc", "jcc", modifiers=[0x02])
add_insn("jnae", "jcc", modifiers=[0x02])
add_insn("jnb", "jcc", modifiers=[0x03])
add_insn("jnc", "jcc", modifiers=[0x03])
add_insn("jae", "jcc", modifiers=[0x03])
add_insn("je", "jcc", modifiers=[0x04])
add_insn("jz", "jcc", modifiers=[0x04])
add_insn("jne", "jcc", modifiers=[0x05])
add_insn("jnz", "jcc", modifiers=[0x05])
add_insn("jbe", "jcc", modifiers=[0x06])
add_insn("jna", "jcc", modifiers=[0x06])
add_insn("jnbe", "jcc", modifiers=[0x07])
add_insn("ja", "jcc", modifiers=[0x07])
add_insn("js", "jcc", modifiers=[0x08])
add_insn("jns", "jcc", modifiers=[0x09])
add_insn("jp", "jcc", modifiers=[0x0A])
add_insn("jpe", "jcc", modifiers=[0x0A])
add_insn("jnp", "jcc", modifiers=[0x0B])
add_insn("jpo", "jcc", modifiers=[0x0B])
add_insn("jl", "jcc", modifiers=[0x0C])
add_insn("jnge", "jcc", modifiers=[0x0C])
add_insn("jnl", "jcc", modifiers=[0x0D])
add_insn("jge", "jcc", modifiers=[0x0D])
add_insn("jle", "jcc", modifiers=[0x0E])
add_insn("jng", "jcc", modifiers=[0x0E])
add_insn("jnle", "jcc", modifiers=[0x0F])
add_insn("jg", "jcc", modifiers=[0x0F])

#
# jcxz
#
add_group("jcxz",
    modifiers=["AdSizeR"],
    opcode=[],
    operands=[Operand(type="Imm", dest="JmpRel")])
add_group("jcxz",
    modifiers=["AdSizeR"],
    def_opersize_64=64,
    opcode=[0xE3],
    operands=[Operand(type="Imm", tmod="Short", dest="JmpRel")])

add_insn("jcxz", "jcxz", modifiers=[16])
add_insn("jecxz", "jcxz", modifiers=[32], cpu=["386"])
add_insn("jrcxz", "jcxz", modifiers=[64], only64=True)

#####################################################################
# Loop instructions
#####################################################################
add_group("loop",
    opcode=[],
    operands=[Operand(type="Imm", dest="JmpRel")])
add_group("loop",
    not64=True,
    opcode=[],
    operands=[Operand(type="Imm", dest="JmpRel"),
              Operand(type="Creg", size=16, dest="AdSizeR")])
add_group("loop",
    def_opersize_64=64,
    opcode=[],
    operands=[Operand(type="Imm", dest="JmpRel"),
              Operand(type="Creg", size=32, dest="AdSizeR")])
add_group("loop",
    def_opersize_64=64,
    opcode=[],
    operands=[Operand(type="Imm", dest="JmpRel"),
              Operand(type="Creg", size=64, dest="AdSizeR")])

add_group("loop",
    not64=True,
    modifiers=["Op0Add"],
    opcode=[0xE0],
    operands=[Operand(type="Imm", tmod="Short", dest="JmpRel")])
for sz in [16, 32, 64]:
    add_group("loop",
        modifiers=["Op0Add"],
        def_opersize_64=64,
        opcode=[0xE0],
        operands=[Operand(type="Imm", tmod="Short", dest="JmpRel"),
                  Operand(type="Creg", size=sz, dest="AdSizeR")])

add_insn("loop", "loop", modifiers=[2])
add_insn("loopz", "loop", modifiers=[1])
add_insn("loope", "loop", modifiers=[1])
add_insn("loopnz", "loop", modifiers=[0])
add_insn("loopne", "loop", modifiers=[0])

#####################################################################
# Set byte on flag instructions
#####################################################################
add_group("setcc",
    suffix="b",
    cpu=["386"],
    modifiers=["Op1Add"],
    opcode=[0x0F, 0x90],
    spare=2,
    operands=[Operand(type="RM", size=8, relaxed=True, dest="EA")])

add_insn("seto", "setcc", modifiers=[0x00])
add_insn("setno", "setcc", modifiers=[0x01])
add_insn("setb", "setcc", modifiers=[0x02])
add_insn("setc", "setcc", modifiers=[0x02])
add_insn("setnae", "setcc", modifiers=[0x02])
add_insn("setnb", "setcc", modifiers=[0x03])
add_insn("setnc", "setcc", modifiers=[0x03])
add_insn("setae", "setcc", modifiers=[0x03])
add_insn("sete", "setcc", modifiers=[0x04])
add_insn("setz", "setcc", modifiers=[0x04])
add_insn("setne", "setcc", modifiers=[0x05])
add_insn("setnz", "setcc", modifiers=[0x05])
add_insn("setbe", "setcc", modifiers=[0x06])
add_insn("setna", "setcc", modifiers=[0x06])
add_insn("setnbe", "setcc", modifiers=[0x07])
add_insn("seta", "setcc", modifiers=[0x07])
add_insn("sets", "setcc", modifiers=[0x08])
add_insn("setns", "setcc", modifiers=[0x09])
add_insn("setp", "setcc", modifiers=[0x0A])
add_insn("setpe", "setcc", modifiers=[0x0A])
add_insn("setnp", "setcc", modifiers=[0x0B])
add_insn("setpo", "setcc", modifiers=[0x0B])
add_insn("setl", "setcc", modifiers=[0x0C])
add_insn("setnge", "setcc", modifiers=[0x0C])
add_insn("setnl", "setcc", modifiers=[0x0D])
add_insn("setge", "setcc", modifiers=[0x0D])
add_insn("setle", "setcc", modifiers=[0x0E])
add_insn("setng", "setcc", modifiers=[0x0E])
add_insn("setnle", "setcc", modifiers=[0x0F])
add_insn("setg", "setcc", modifiers=[0x0F])

#####################################################################
# String instructions
#####################################################################
add_insn("cmpsb", "onebyte", modifiers=[0xA6, 0])
add_insn("cmpsw", "onebyte", modifiers=[0xA7, 16])

# cmpsd has to be non-onebyte for SSE2 forms below
add_group("cmpsd",
    parsers=["nasm"],
    opersize=32,
    opcode=[0xA7],
    operands=[])

add_insn("cmpsd", "cmpsd", cpu=[])

add_insn("cmpsl", "onebyte", parser="gas", modifiers=[0xA7, 32], cpu=["386"])
add_insn("cmpsq", "onebyte", modifiers=[0xA7, 64], only64=True)
add_insn("insb", "onebyte", modifiers=[0x6C, 0])
add_insn("insw", "onebyte", modifiers=[0x6D, 16])
add_insn("insd", "onebyte", parser="nasm", modifiers=[0x6D, 32], cpu=["386"])
add_insn("insl", "onebyte", parser="gas", modifiers=[0x6D, 32], cpu=["386"])
add_insn("outsb", "onebyte", modifiers=[0x6E, 0])
add_insn("outsw", "onebyte", modifiers=[0x6F, 16])
add_insn("outsd", "onebyte", parser="nasm", modifiers=[0x6F, 32],
         cpu=["386"])
add_insn("outsl", "onebyte", parser="gas", modifiers=[0x6F, 32], cpu=["386"])
add_insn("lodsb", "onebyte", modifiers=[0xAC, 0])
add_insn("lodsw", "onebyte", modifiers=[0xAD, 16])
add_insn("lodsd", "onebyte", parser="nasm", modifiers=[0xAD, 32],
         cpu=["386"])
add_insn("lodsl", "onebyte", parser="gas", modifiers=[0xAD, 32], cpu=["386"])
add_insn("lodsq", "onebyte", modifiers=[0xAD, 64], only64=True)
add_insn("movsb", "onebyte", modifiers=[0xA4, 0])
add_insn("movsw", "onebyte", modifiers=[0xA5, 16])

# movsd has to be non-onebyte for SSE2 forms below
add_group("movsd",
    parsers=["nasm"],
    opersize=32,
    opcode=[0xA5],
    operands=[])

add_insn("movsd", "movsd", cpu=["386"])

add_insn("movsl", "onebyte", parser="gas", modifiers=[0xA5, 32], cpu=["386"])
add_insn("movsq", "onebyte", modifiers=[0xA5, 64], only64=True)
# smov alias for movs in GAS mode
add_insn("smovb", "onebyte", parser="gas", modifiers=[0xA4, 0])
add_insn("smovw", "onebyte", parser="gas", modifiers=[0xA5, 16])
add_insn("smovl", "onebyte", parser="gas", modifiers=[0xA5, 32], cpu=["386"])
add_insn("smovq", "onebyte", parser="gas", modifiers=[0xA5, 64], only64=True)
add_insn("scasb", "onebyte", modifiers=[0xAE, 0])
add_insn("scasw", "onebyte", modifiers=[0xAF, 16])
add_insn("scasd", "onebyte", parser="nasm", modifiers=[0xAF, 32],
         cpu=["386"])
add_insn("scasl", "onebyte", parser="gas", modifiers=[0xAF, 32], cpu=["386"])
add_insn("scasq", "onebyte", modifiers=[0xAF, 64], only64=True)
# ssca alias for scas in GAS mode
add_insn("sscab", "onebyte", parser="gas", modifiers=[0xAE, 0])
add_insn("sscaw", "onebyte", parser="gas", modifiers=[0xAF, 16])
add_insn("sscal", "onebyte", parser="gas", modifiers=[0xAF, 32], cpu=["386"])
add_insn("sscaq", "onebyte", parser="gas", modifiers=[0xAF, 64], only64=True)
add_insn("stosb", "onebyte", modifiers=[0xAA, 0])
add_insn("stosw", "onebyte", modifiers=[0xAB, 16])
add_insn("stosd", "onebyte", parser="nasm", modifiers=[0xAB, 32],
         cpu=["386"])
add_insn("stosl", "onebyte", parser="gas", modifiers=[0xAB, 32], cpu=["386"])
add_insn("stosq", "onebyte", modifiers=[0xAB, 64], only64=True)
add_insn("xlatb", "onebyte", modifiers=[0xD7, 0])

#####################################################################
# Bit manipulation
#####################################################################

#
# bit tests
#
for sfx, sz in zip("wlq", [16, 32, 64]):
    add_group("bittest",
        suffix=sfx,
        cpu=["386"],
        modifiers=["Op1Add"],
        opersize=sz,
        opcode=[0x0F, 0x00],
        operands=[Operand(type="RM", size=sz, relaxed=True, dest="EA"),
                  Operand(type="Reg", size=sz, dest="Spare")])
for sfx, sz in zip("wlq", [16, 32, 64]):
    add_group("bittest",
        suffix=sfx,
        cpu=["386"],
        modifiers=["Gap", "SpAdd"],
        opersize=sz,
        opcode=[0x0F, 0xBA],
        spare=0,
        operands=[Operand(type="RM", size=sz, dest="EA"),
                  Operand(type="Imm", size=8, relaxed=True, dest="Imm")])

add_insn("bt",  "bittest", modifiers=[0xA3, 4])
add_insn("bts", "bittest", modifiers=[0xAB, 5])
add_insn("btr", "bittest", modifiers=[0xB3, 6])
add_insn("btc", "bittest", modifiers=[0xBB, 7])

#
# bit scans - also used for lar/lsl
#
for sfx, sz in zip("wlq", [16, 32, 64]):
    add_group("bsfr",
        suffix=sfx,
        cpu=["386"],
        modifiers=["Op1Add"],
        opersize=sz,
        opcode=[0x0F, 0x00],
        operands=[Operand(type="Reg", size=sz, dest="Spare"),
                  Operand(type="RM", size=sz, relaxed=True, dest="EA")])

add_insn("bsf", "bsfr", modifiers=[0xBC])
add_insn("bsr", "bsfr", modifiers=[0xBD])

#####################################################################
# Interrupts and operating system instructions
#####################################################################
add_group("int",
    opcode=[0xCD],
    operands=[Operand(type="Imm", size=8, relaxed=True, dest="Imm")])

add_insn("int", "int")
add_insn("int3", "onebyte", modifiers=[0xCC])
add_insn("int03", "onebyte", parser="nasm", modifiers=[0xCC])
add_insn("into", "onebyte", modifiers=[0xCE], not64=True)
add_insn("iret", "onebyte", modifiers=[0xCF])
add_insn("iretw", "onebyte", modifiers=[0xCF, 16])
add_insn("iretd", "onebyte", parser="nasm", modifiers=[0xCF, 32],
         cpu=["386"])
add_insn("iretl", "onebyte", parser="gas", modifiers=[0xCF, 32], cpu=["386"])
add_insn("iretq", "onebyte", modifiers=[0xCF, 64], only64=True)
add_insn("rsm", "twobyte", modifiers=[0x0F, 0xAA], cpu=["586", "SMM"])

for sfx, sz in zip("wl", [16, 32]):
    add_group("bound",
        suffix=sfx,
        cpu=["186"],
        not64=True,
        opersize=sz,
        opcode=[0x62],
        operands=[Operand(type="Reg", size=sz, dest="Spare"),
                  Operand(type="Mem", size=sz, relaxed=True, dest="EA")])

add_insn("bound", "bound")
add_insn("hlt", "onebyte", modifiers=[0xF4], cpu=["Priv"])
add_insn("nop", "onebyte", modifiers=[0x90])

#
# Protection control
#
add_insn("lar", "bsfr", modifiers=[0x02], cpu=["286", "Prot"])
add_insn("lsl", "bsfr", modifiers=[0x03], cpu=["286", "Prot"])

add_group("arpl",
    suffix="w",
    cpu=["Prot", "286"],
    not64=True,
    opcode=[0x63],
    operands=[Operand(type="RM", size=16, relaxed=True, dest="EA"),
              Operand(type="Reg", size=16, dest="Spare")])

add_insn("arpl", "arpl")

for sfx in [None, "w", "l", "q"]:
    add_insn("lgdt"+(sfx or ""), "twobytemem", suffix=sfx,
             modifiers=[2, 0x0F, 0x01], cpu=["286", "Priv"])
    add_insn("lidt"+(sfx or ""), "twobytemem", suffix=sfx,
             modifiers=[3, 0x0F, 0x01], cpu=["286", "Priv"])
    add_insn("sgdt"+(sfx or ""), "twobytemem", suffix=sfx,
             modifiers=[0, 0x0F, 0x01], cpu=["286", "Priv"])
    add_insn("sidt"+(sfx or ""), "twobytemem", suffix=sfx,
             modifiers=[1, 0x0F, 0x01], cpu=["286", "Priv"])

for sfx, sz in zip("wlq", [16, 32, 64]):
    add_group("str",
        suffix=sfx,
        cpu=["Prot", "286"],
        opersize=sz,
        opcode=[0x0F, 0x00],
        spare=1,
        operands=[Operand(type="Reg", size=sz, dest="EA")])
add_group("str",
    suffixes=["w", "l"],
    cpu=["Prot", "286"],
    opcode=[0x0F, 0x00],
    spare=1,
    operands=[Operand(type="RM", size=16, relaxed=True, dest="EA")])

add_insn("str", "str")

add_group("prot286",
    suffix="w",
    cpu=["286"],
    modifiers=["SpAdd", "Op1Add"],
    opcode=[0x0F, 0x00],
    spare=0,
    operands=[Operand(type="RM", size=16, relaxed=True, dest="EA")])

add_insn("lldt", "prot286", modifiers=[2, 0], cpu=["286", "Prot", "Priv"])
add_insn("ltr", "prot286", modifiers=[3, 0], cpu=["286", "Prot", "Priv"])
add_insn("verr", "prot286", modifiers=[4, 0], cpu=["286", "Prot"])
add_insn("verw", "prot286", modifiers=[5, 0], cpu=["286", "Prot"])
add_insn("lmsw", "prot286", modifiers=[6, 1], cpu=["286", "Priv"])

for sfx, sz in zip("wlq", [16, 32, 64]):
    add_group("sldtmsw",
        suffix=sfx,
        only64=(sz==64),
        cpu=[(sz==32) and "386" or "286"],
        modifiers=["SpAdd", "Op1Add"],
        opcode=[0x0F, 0x00],
        spare=0,
        operands=[Operand(type="Mem", size=sz, relaxed=True, dest="EA")])
for sfx, sz in zip("wlq", [16, 32, 64]):
    add_group("sldtmsw",
        suffix=sfx,
        cpu=["286"],
        modifiers=["SpAdd", "Op1Add"],
        opersize=sz,
        opcode=[0x0F, 0x00],
        spare=0,
        operands=[Operand(type="Reg", size=sz, dest="EA")])

add_insn("sldt", "sldtmsw", modifiers=[0, 0])
add_insn("smsw", "sldtmsw", modifiers=[4, 1])

#####################################################################
# Floating point instructions
#####################################################################
add_insn("fcompp",  "twobyte", modifiers=[0xDE, 0xD9], cpu=["FPU"])
add_insn("fucompp", "twobyte", modifiers=[0xDA, 0xE9], cpu=["286", "FPU"])
add_insn("ftst",    "twobyte", modifiers=[0xD9, 0xE4], cpu=["FPU"])
add_insn("fxam",    "twobyte", modifiers=[0xD9, 0xE5], cpu=["FPU"])
add_insn("fld1",    "twobyte", modifiers=[0xD9, 0xE8], cpu=["FPU"])
add_insn("fldl2t",  "twobyte", modifiers=[0xD9, 0xE9], cpu=["FPU"])
add_insn("fldl2e",  "twobyte", modifiers=[0xD9, 0xEA], cpu=["FPU"])
add_insn("fldpi",   "twobyte", modifiers=[0xD9, 0xEB], cpu=["FPU"])
add_insn("fldlg2",  "twobyte", modifiers=[0xD9, 0xEC], cpu=["FPU"])
add_insn("fldln2",  "twobyte", modifiers=[0xD9, 0xED], cpu=["FPU"])
add_insn("fldz",    "twobyte", modifiers=[0xD9, 0xEE], cpu=["FPU"])
add_insn("f2xm1",   "twobyte", modifiers=[0xD9, 0xF0], cpu=["FPU"])
add_insn("fyl2x",   "twobyte", modifiers=[0xD9, 0xF1], cpu=["FPU"])
add_insn("fptan",   "twobyte", modifiers=[0xD9, 0xF2], cpu=["FPU"])
add_insn("fpatan",  "twobyte", modifiers=[0xD9, 0xF3], cpu=["FPU"])
add_insn("fxtract", "twobyte", modifiers=[0xD9, 0xF4], cpu=["FPU"])
add_insn("fprem1",  "twobyte", modifiers=[0xD9, 0xF5], cpu=["286", "FPU"])
add_insn("fdecstp", "twobyte", modifiers=[0xD9, 0xF6], cpu=["FPU"])
add_insn("fincstp", "twobyte", modifiers=[0xD9, 0xF7], cpu=["FPU"])
add_insn("fprem",   "twobyte", modifiers=[0xD9, 0xF8], cpu=["FPU"])
add_insn("fyl2xp1", "twobyte", modifiers=[0xD9, 0xF9], cpu=["FPU"])
add_insn("fsqrt",   "twobyte", modifiers=[0xD9, 0xFA], cpu=["FPU"])
add_insn("fsincos", "twobyte", modifiers=[0xD9, 0xFB], cpu=["286", "FPU"])
add_insn("frndint", "twobyte", modifiers=[0xD9, 0xFC], cpu=["FPU"])
add_insn("fscale",  "twobyte", modifiers=[0xD9, 0xFD], cpu=["FPU"])
add_insn("fsin",    "twobyte", modifiers=[0xD9, 0xFE], cpu=["286", "FPU"])
add_insn("fcos",    "twobyte", modifiers=[0xD9, 0xFF], cpu=["286", "FPU"])
add_insn("fchs",    "twobyte", modifiers=[0xD9, 0xE0], cpu=["FPU"])
add_insn("fabs",    "twobyte", modifiers=[0xD9, 0xE1], cpu=["FPU"])
add_insn("fninit",  "twobyte", modifiers=[0xDB, 0xE3], cpu=["FPU"])
add_insn("finit", "threebyte", modifiers=[0x9B, 0xDB, 0xE3], cpu=["FPU"])
add_insn("fnclex",  "twobyte", modifiers=[0xDB, 0xE2], cpu=["FPU"])
add_insn("fclex", "threebyte", modifiers=[0x9B, 0xDB, 0xE2], cpu=["FPU"])
for sfx in [None, "l", "s"]:
    add_insn("fnstenv"+(sfx or ""), "onebytemem", suffix=sfx,
             modifiers=[6, 0xD9], cpu=["FPU"])
    add_insn("fstenv"+(sfx or ""),  "twobytemem", suffix=sfx,
             modifiers=[6, 0x9B, 0xD9], cpu=["FPU"])
    add_insn("fldenv"+(sfx or ""),  "onebytemem", suffix=sfx,
             modifiers=[4, 0xD9], cpu=["FPU"])
    add_insn("fnsave"+(sfx or ""),  "onebytemem", suffix=sfx,
             modifiers=[6, 0xDD], cpu=["FPU"])
    add_insn("fsave"+(sfx or ""),   "twobytemem", suffix=sfx,
             modifiers=[6, 0x9B, 0xDD], cpu=["FPU"])
    add_insn("frstor"+(sfx or ""),  "onebytemem", suffix=sfx,
             modifiers=[4, 0xDD], cpu=["FPU"])
add_insn("fnop",    "twobyte", modifiers=[0xD9, 0xD0], cpu=["FPU"])
add_insn("fwait",   "onebyte", modifiers=[0x9B], cpu=["FPU"])
# Prefixes; should the others be here too? should wait be a prefix?
add_insn("wait",    "onebyte", modifiers=[0x9B])

#
# load/store with pop (integer and normal)
#
add_group("fld",
    suffix="s",
    cpu=["FPU"],
    opcode=[0xD9],
    operands=[Operand(type="Mem", size=32, dest="EA")])
add_group("fld",
    suffix="l",
    cpu=["FPU"],
    opcode=[0xDD],
    operands=[Operand(type="Mem", size=64, dest="EA")])
add_group("fld",
    cpu=["FPU"],
    opcode=[0xDB],
    spare=5,
    operands=[Operand(type="Mem", size=80, dest="EA")])
add_group("fld",
    cpu=["FPU"],
    opcode=[0xD9, 0xC0],
    operands=[Operand(type="Reg", size=80, dest="Op1Add")])

add_insn("fld", "fld")

add_group("fstp",
    suffix="s",
    cpu=["FPU"],
    opcode=[0xD9],
    spare=3,
    operands=[Operand(type="Mem", size=32, dest="EA")])
add_group("fstp",
    suffix="l",
    cpu=["FPU"],
    opcode=[0xDD],
    spare=3,
    operands=[Operand(type="Mem", size=64, dest="EA")])
add_group("fstp",
    cpu=["FPU"],
    opcode=[0xDB],
    spare=7,
    operands=[Operand(type="Mem", size=80, dest="EA")])
add_group("fstp",
    cpu=["FPU"],
    opcode=[0xDD, 0xD8],
    operands=[Operand(type="Reg", size=80, dest="Op1Add")])

add_insn("fstp", "fstp")

#
# Long memory version of floating point load/store for GAS
#
add_group("fldstpt",
    cpu=["FPU"],
    modifiers=["SpAdd"],
    opcode=[0xDB],
    spare=0,
    operands=[Operand(type="Mem", size=80, dest="EA")])

add_insn("fldt", "fldstpt", suffix="WEAK", modifiers=[5])
add_insn("fstpt", "fldstpt", suffix="WEAK", modifiers=[7])

add_group("fildstp",
    suffix="s",
    cpu=["FPU"],
    modifiers=["SpAdd"],
    opcode=[0xDF],
    spare=0,
    operands=[Operand(type="Mem", size=16, dest="EA")])
add_group("fildstp",
    suffix="l",
    cpu=["FPU"],
    modifiers=["SpAdd"],
    opcode=[0xDB],
    spare=0,
    operands=[Operand(type="Mem", size=32, dest="EA")])
add_group("fildstp",
    suffix="q",
    cpu=["FPU"],
    modifiers=["Gap", "Op0Add", "SpAdd"],
    opcode=[0xDD],
    spare=0,
    operands=[Operand(type="Mem", size=64, dest="EA")])

add_insn("fild", "fildstp", modifiers=[0, 2, 5])
add_insn("fistp", "fildstp", modifiers=[3, 2, 7])

add_group("fbldstp",
    cpu=["FPU"],
    modifiers=["SpAdd"],
    opcode=[0xDF],
    spare=0,
    operands=[Operand(type="Mem", size=80, relaxed=True, dest="EA")])

add_insn("fbld", "fbldstp", modifiers=[4])
add_insn("fildll", "fbldstp", parser="gas", modifiers=[5])
add_insn("fbstp", "fbldstp", modifiers=[6])
add_insn("fistpll", "fbldstp", parser="gas", modifiers=[7])

#
# store (normal)
#
add_group("fst",
    suffix="s",
    cpu=["FPU"],
    opcode=[0xD9],
    spare=2,
    operands=[Operand(type="Mem", size=32, dest="EA")])
add_group("fst",
    suffix="l",
    cpu=["FPU"],
    opcode=[0xDD],
    spare=2,
    operands=[Operand(type="Mem", size=64, dest="EA")])
add_group("fst",
    cpu=["FPU"],
    opcode=[0xDD, 0xD0],
    operands=[Operand(type="Reg", size=80, dest="Op1Add")])

add_insn("fst", "fst")

#
# exchange (with ST0)
#
add_group("fxch",
    cpu=["FPU"],
    opcode=[0xD9, 0xC8],
    operands=[Operand(type="Reg", size=80, dest="Op1Add")])
add_group("fxch",
    cpu=["FPU"],
    opcode=[0xD9, 0xC8],
    operands=[Operand(type="ST0", size=80, dest=None),
              Operand(type="Reg", size=80, dest="Op1Add")])
add_group("fxch",
    cpu=["FPU"],
    opcode=[0xD9, 0xC8],
    operands=[Operand(type="Reg", size=80, dest="Op1Add"),
              Operand(type="ST0", size=80, dest=None)])
add_group("fxch",
    cpu=["FPU"],
    opcode=[0xD9, 0xC9],
    operands=[])

add_insn("fxch", "fxch")

#
# comparisons
#
add_group("fcom",
    suffix="s",
    cpu=["FPU"],
    modifiers=["Gap", "SpAdd"],
    opcode=[0xD8],
    spare=0,
    operands=[Operand(type="Mem", size=32, dest="EA")])
add_group("fcom",
    suffix="l",
    cpu=["FPU"],
    modifiers=["Gap", "SpAdd"],
    opcode=[0xDC],
    spare=0,
    operands=[Operand(type="Mem", size=64, dest="EA")])
add_group("fcom",
    cpu=["FPU"],
    modifiers=["Op1Add"],
    opcode=[0xD8, 0x00],
    operands=[Operand(type="Reg", size=80, dest="Op1Add")])
# Alias for fcom %st(1) for GAS compat
add_group("fcom",
    cpu=["FPU"],
    parsers=["gas"],
    modifiers=["Op1Add"],
    opcode=[0xD8, 0x01],
    operands=[])
add_group("fcom",
    cpu=["FPU"],
    parsers=["nasm"],
    modifiers=["Op1Add"],
    opcode=[0xD8, 0x00],
    operands=[Operand(type="ST0", size=80, dest=None),
              Operand(type="Reg", size=80, dest="Op1Add")])

add_insn("fcom", "fcom", modifiers=[0xD0, 2])
add_insn("fcomp", "fcom", modifiers=[0xD8, 3])

#
# extended comparisons
#
add_group("fcom2",
    cpu=["FPU", "286"],
    modifiers=["Op0Add", "Op1Add"],
    opcode=[0x00, 0x00],
    operands=[Operand(type="Reg", size=80, dest="Op1Add")])
add_group("fcom2",
    cpu=["FPU", "286"],
    modifiers=["Op0Add", "Op1Add"],
    opcode=[0x00, 0x00],
    operands=[Operand(type="ST0", size=80, dest=None),
              Operand(type="Reg", size=80, dest="Op1Add")])

add_insn("fucom", "fcom2", modifiers=[0xDD, 0xE0])
add_insn("fucomp", "fcom2", modifiers=[0xDD, 0xE8])

#
# arithmetic
#
add_group("farith",
    suffix="s",
    cpu=["FPU"],
    modifiers=["Gap", "Gap", "SpAdd"],
    opcode=[0xD8],
    spare=0,
    operands=[Operand(type="Mem", size=32, dest="EA")])
add_group("farith",
    suffix="l",
    cpu=["FPU"],
    modifiers=["Gap", "Gap", "SpAdd"],
    opcode=[0xDC],
    spare=0,
    operands=[Operand(type="Mem", size=64, dest="EA")])
add_group("farith",
    cpu=["FPU"],
    modifiers=["Gap", "Op1Add"],
    opcode=[0xD8, 0x00],
    operands=[Operand(type="Reg", size=80, dest="Op1Add")])
add_group("farith",
    cpu=["FPU"],
    modifiers=["Gap", "Op1Add"],
    opcode=[0xD8, 0x00],
    operands=[Operand(type="ST0", size=80, dest=None),
              Operand(type="Reg", size=80, dest="Op1Add")])
add_group("farith",
    cpu=["FPU"],
    modifiers=["Op1Add"],
    opcode=[0xDC, 0x00],
    operands=[Operand(type="Reg", size=80, tmod="To", dest="Op1Add")])
add_group("farith",
    cpu=["FPU"],
    parsers=["nasm"],
    modifiers=["Op1Add"],
    opcode=[0xDC, 0x00],
    operands=[Operand(type="Reg", size=80, dest="Op1Add"),
              Operand(type="ST0", size=80, dest=None)])
add_group("farith",
    cpu=["FPU"],
    parsers=["gas"],
    modifiers=["Gap", "Op1Add"],
    opcode=[0xDC, 0x00],
    operands=[Operand(type="Reg", size=80, dest="Op1Add"),
              Operand(type="ST0", size=80, dest=None)])

add_insn("fadd", "farith", modifiers=[0xC0, 0xC0, 0])
add_insn("fsub", "farith", modifiers=[0xE8, 0xE0, 4])
add_insn("fsubr", "farith", modifiers=[0xE0, 0xE8, 5])
add_insn("fmul", "farith", modifiers=[0xC8, 0xC8, 1])
add_insn("fdiv", "farith", modifiers=[0xF8, 0xF0, 6])
add_insn("fdivr", "farith", modifiers=[0xF0, 0xF8, 7])

add_group("farithp",
    cpu=["FPU"],
    modifiers=["Op1Add"],
    opcode=[0xDE, 0x01],
    operands=[])
add_group("farithp",
    cpu=["FPU"],
    modifiers=["Op1Add"],
    opcode=[0xDE, 0x00],
    operands=[Operand(type="Reg", size=80, dest="Op1Add")])
add_group("farithp",
    cpu=["FPU"],
    modifiers=["Op1Add"],
    opcode=[0xDE, 0x00],
    operands=[Operand(type="Reg", size=80, dest="Op1Add"),
              Operand(type="ST0", size=80, dest=None)])

add_insn("faddp", "farithp", modifiers=[0xC0])
add_insn("fsubp", "farithp", parser="nasm", modifiers=[0xE8])
add_insn("fsubp", "farithp", parser="gas", modifiers=[0xE0])
add_insn("fsubrp", "farithp", parser="nasm", modifiers=[0xE0])
add_insn("fsubrp", "farithp", parser="gas", modifiers=[0xE8])
add_insn("fmulp", "farithp", modifiers=[0xC8])
add_insn("fdivp", "farithp", parser="nasm", modifiers=[0xF8])
add_insn("fdivp", "farithp", parser="gas", modifiers=[0xF0])
add_insn("fdivrp", "farithp", parser="nasm", modifiers=[0xF0])
add_insn("fdivrp", "farithp", parser="gas", modifiers=[0xF8])

#
# integer arith/store wo pop/compare
#
add_group("fiarith",
    suffix="s",
    cpu=["FPU"],
    modifiers=["SpAdd", "Op0Add"],
    opcode=[0x04],
    spare=0,
    operands=[Operand(type="Mem", size=16, dest="EA")])
add_group("fiarith",
    suffix="l",
    cpu=["FPU"],
    modifiers=["SpAdd", "Op0Add"],
    opcode=[0x00],
    spare=0,
    operands=[Operand(type="Mem", size=32, dest="EA")])

add_insn("fist",   "fiarith", modifiers=[2, 0xDB])
add_insn("ficom",  "fiarith", modifiers=[2, 0xDA])
add_insn("ficomp", "fiarith", modifiers=[3, 0xDA])
add_insn("fiadd",  "fiarith", modifiers=[0, 0xDA])
add_insn("fisub",  "fiarith", modifiers=[4, 0xDA])
add_insn("fisubr", "fiarith", modifiers=[5, 0xDA])
add_insn("fimul",  "fiarith", modifiers=[1, 0xDA])
add_insn("fidiv",  "fiarith", modifiers=[6, 0xDA])
add_insn("fidivr", "fiarith", modifiers=[7, 0xDA])

#
# processor control
#
add_group("fldnstcw",
    suffix="w",
    cpu=["FPU"],
    modifiers=["SpAdd"],
    opcode=[0xD9],
    spare=0,
    operands=[Operand(type="Mem", size=16, relaxed=True, dest="EA")])

add_insn("fldcw", "fldnstcw", modifiers=[5])
add_insn("fnstcw", "fldnstcw", modifiers=[7])

add_group("fstcw",
    suffix="w",
    cpu=["FPU"],
    opcode=[0x9B, 0xD9],
    spare=7,
    operands=[Operand(type="Mem", size=16, relaxed=True, dest="EA")])

add_insn("fstcw", "fstcw")

add_group("fnstsw",
    suffix="w",
    cpu=["FPU"],
    opcode=[0xDD],
    spare=7,
    operands=[Operand(type="Mem", size=16, relaxed=True, dest="EA")])
add_group("fnstsw",
    suffix="w",
    cpu=["FPU"],
    opcode=[0xDF, 0xE0],
    operands=[Operand(type="Areg", size=16, dest=None)])

add_insn("fnstsw", "fnstsw")

add_group("fstsw",
    suffix="w",
    cpu=["FPU"],
    opcode=[0x9B, 0xDD],
    spare=7,
    operands=[Operand(type="Mem", size=16, relaxed=True, dest="EA")])
add_group("fstsw",
    suffix="w",
    cpu=["FPU"],
    opcode=[0x9B, 0xDF, 0xE0],
    operands=[Operand(type="Areg", size=16, dest=None)])

add_insn("fstsw", "fstsw")

add_group("ffree",
    cpu=["FPU"],
    modifiers=["Op0Add"],
    opcode=[0x00, 0xC0],
    operands=[Operand(type="Reg", size=80, dest="Op1Add")])

add_insn("ffree", "ffree", modifiers=[0xDD])
add_insn("ffreep", "ffree", modifiers=[0xDF], cpu=["686", "FPU", "Undoc"])

#####################################################################
# 486 extensions
#####################################################################
add_group("bswap",
    suffix="l",
    cpu=["486"],
    opersize=32,
    opcode=[0x0F, 0xC8],
    operands=[Operand(type="Reg", size=32, dest="Op1Add")])
add_group("bswap",
    suffix="q",
    opersize=64,
    opcode=[0x0F, 0xC8],
    operands=[Operand(type="Reg", size=64, dest="Op1Add")])

add_insn("bswap", "bswap")

for sfx, sz in zip("bwlq", [8, 16, 32, 64]):
    add_group("cmpxchgxadd",
        suffix=sfx,
        cpu=["486"],
        modifiers=["Op1Add"],
        opersize=sz,
        opcode=[0x0F, 0x00+(sz!=8)],
        operands=[Operand(type="RM", size=sz, relaxed=True, dest="EA"),
                  Operand(type="Reg", size=sz, dest="Spare")])

add_insn("xadd", "cmpxchgxadd", modifiers=[0xC0])
add_insn("cmpxchg", "cmpxchgxadd", modifiers=[0xB0])
add_insn("cmpxchg486", "cmpxchgxadd", parser="nasm", modifiers=[0xA6],
         cpu=["486", "Undoc"])

add_insn("invd", "twobyte", modifiers=[0x0F, 0x08], cpu=["486", "Priv"])
add_insn("wbinvd", "twobyte", modifiers=[0x0F, 0x09], cpu=["486", "Priv"])
add_insn("invlpg", "twobytemem", modifiers=[7, 0x0F, 0x01],
         cpu=["486", "Priv"])

#####################################################################
# 586+ and late 486 extensions
#####################################################################
add_insn("cpuid", "twobyte", modifiers=[0x0F, 0xA2], cpu=["486"])

#####################################################################
# Pentium extensions
#####################################################################
add_insn("wrmsr", "twobyte", modifiers=[0x0F, 0x30], cpu=["586", "Priv"])
add_insn("rdtsc", "twobyte", modifiers=[0x0F, 0x31], cpu=["586"])
add_insn("rdmsr", "twobyte", modifiers=[0x0F, 0x32], cpu=["586", "Priv"])

add_group("cmpxchg8b",
    suffix="q",
    cpu=["586"],
    opcode=[0x0F, 0xC7],
    spare=1,
    operands=[Operand(type="Mem", size=64, relaxed=True, dest="EA")])

add_insn("cmpxchg8b", "cmpxchg8b")

#####################################################################
# Pentium II/Pentium Pro extensions
#####################################################################
add_insn("sysenter", "twobyte", modifiers=[0x0F, 0x34], cpu=["686"],
         not64=True)
add_insn("sysexit",  "twobyte", modifiers=[0x0F, 0x35], cpu=["686", "Priv"],
         not64=True)
for sfx in [None, "q"]:
    add_insn("fxsave"+(sfx or ""),  "twobytemem", suffix=sfx,
             modifiers=[0, 0x0F, 0xAE], cpu=["686", "FPU"])
    add_insn("fxrstor"+(sfx or ""), "twobytemem", suffix=sfx,
             modifiers=[1, 0x0F, 0xAE], cpu=["686", "FPU"])
add_insn("rdpmc", "twobyte", modifiers=[0x0F, 0x33], cpu=["686"])
add_insn("ud2",   "twobyte", modifiers=[0x0F, 0x0B], cpu=["286"])
add_insn("ud1",   "twobyte", modifiers=[0x0F, 0xB9], cpu=["286", "Undoc"])

for sfx, sz in zip("wlq", [16, 32, 64]):
    add_group("cmovcc",
        suffix=sfx,
        cpu=["686"],
        modifiers=["Op1Add"],
        opersize=sz,
        opcode=[0x0F, 0x40],
        operands=[Operand(type="Reg", size=sz, dest="Spare"),
                  Operand(type="RM", size=sz, relaxed=True, dest="EA")])

add_insn("cmovo", "cmovcc", modifiers=[0x00])
add_insn("cmovno", "cmovcc", modifiers=[0x01])
add_insn("cmovb", "cmovcc", modifiers=[0x02])
add_insn("cmovc", "cmovcc", modifiers=[0x02])
add_insn("cmovnae", "cmovcc", modifiers=[0x02])
add_insn("cmovnb", "cmovcc", modifiers=[0x03])
add_insn("cmovnc", "cmovcc", modifiers=[0x03])
add_insn("cmovae", "cmovcc", modifiers=[0x03])
add_insn("cmove", "cmovcc", modifiers=[0x04])
add_insn("cmovz", "cmovcc", modifiers=[0x04])
add_insn("cmovne", "cmovcc", modifiers=[0x05])
add_insn("cmovnz", "cmovcc", modifiers=[0x05])
add_insn("cmovbe", "cmovcc", modifiers=[0x06])
add_insn("cmovna", "cmovcc", modifiers=[0x06])
add_insn("cmovnbe", "cmovcc", modifiers=[0x07])
add_insn("cmova", "cmovcc", modifiers=[0x07])
add_insn("cmovs", "cmovcc", modifiers=[0x08])
add_insn("cmovns", "cmovcc", modifiers=[0x09])
add_insn("cmovp", "cmovcc", modifiers=[0x0A])
add_insn("cmovpe", "cmovcc", modifiers=[0x0A])
add_insn("cmovnp", "cmovcc", modifiers=[0x0B])
add_insn("cmovpo", "cmovcc", modifiers=[0x0B])
add_insn("cmovl", "cmovcc", modifiers=[0x0C])
add_insn("cmovnge", "cmovcc", modifiers=[0x0C])
add_insn("cmovnl", "cmovcc", modifiers=[0x0D])
add_insn("cmovge", "cmovcc", modifiers=[0x0D])
add_insn("cmovle", "cmovcc", modifiers=[0x0E])
add_insn("cmovng", "cmovcc", modifiers=[0x0E])
add_insn("cmovnle", "cmovcc", modifiers=[0x0F])
add_insn("cmovg", "cmovcc", modifiers=[0x0F])

add_group("fcmovcc",
    cpu=["FPU", "686"],
    modifiers=["Op0Add", "Op1Add"],
    opcode=[0x00, 0x00],
    operands=[Operand(type="ST0", size=80, dest=None),
              Operand(type="Reg", size=80, dest="Op1Add")])

add_insn("fcmovb",   "fcmovcc", modifiers=[0xDA, 0xC0])
add_insn("fcmove",   "fcmovcc", modifiers=[0xDA, 0xC8])
add_insn("fcmovbe",  "fcmovcc", modifiers=[0xDA, 0xD0])
add_insn("fcmovu",   "fcmovcc", modifiers=[0xDA, 0xD8])
add_insn("fcmovnb",  "fcmovcc", modifiers=[0xDB, 0xC0])
add_insn("fcmovne",  "fcmovcc", modifiers=[0xDB, 0xC8])
add_insn("fcmovnbe", "fcmovcc", modifiers=[0xDB, 0xD0])
add_insn("fcmovnu",  "fcmovcc", modifiers=[0xDB, 0xD8])

add_insn("fcomi", "fcom2", modifiers=[0xDB, 0xF0], cpu=["686", "FPU"])
add_insn("fucomi", "fcom2", modifiers=[0xDB, 0xE8], cpu=["686", "FPU"])
add_insn("fcomip", "fcom2", modifiers=[0xDF, 0xF0], cpu=["686", "FPU"])
add_insn("fucomip", "fcom2", modifiers=[0xDF, 0xE8], cpu=["686", "FPU"])

#####################################################################
# Pentium4 extensions
#####################################################################
add_group("movnti",
    suffix="l",
    cpu=["P4"],
    opcode=[0x0F, 0xC3],
    operands=[Operand(type="Mem", size=32, relaxed=True, dest="EA"),
              Operand(type="Reg", size=32, dest="Spare")])
add_group("movnti",
    suffix="q",
    cpu=["P4"],
    opersize=64,
    opcode=[0x0F, 0xC3],
    operands=[Operand(type="Mem", size=64, relaxed=True, dest="EA"),
              Operand(type="Reg", size=64, dest="Spare")])

add_insn("movnti", "movnti")

add_group("clflush",
    cpu=["P3"],
    opcode=[0x0F, 0xAE],
    spare=7,
    operands=[Operand(type="Mem", size=8, relaxed=True, dest="EA")])

add_insn("clflush", "clflush")

add_insn("lfence", "threebyte", modifiers=[0x0F, 0xAE, 0xE8], cpu=["P3"])
add_insn("mfence", "threebyte", modifiers=[0x0F, 0xAE, 0xF0], cpu=["P3"])
add_insn("pause", "onebyte_prefix", modifiers=[0xF3, 0x90], cpu=["P4"])

#####################################################################
# MMX/SSE2 instructions
#####################################################################

add_insn("emms", "twobyte", modifiers=[0x0F, 0x77], cpu=["MMX"])

#
# movd
#
add_group("movd",
    cpu=["MMX"],
    opcode=[0x0F, 0x6E],
    operands=[Operand(type="SIMDReg", size=64, dest="Spare"),
              Operand(type="RM", size=32, relaxed=True, dest="EA")])
add_group("movd",
    cpu=["MMX"],
    opersize=64,
    opcode=[0x0F, 0x6E],
    operands=[Operand(type="SIMDReg", size=64, dest="Spare"),
              Operand(type="RM", size=64, relaxed=True, dest="EA")])
add_group("movd",
    cpu=["MMX"],
    opcode=[0x0F, 0x7E],
    operands=[Operand(type="RM", size=32, relaxed=True, dest="EA"),
              Operand(type="SIMDReg", size=64, dest="Spare")])
add_group("movd",
    cpu=["MMX"],
    opersize=64,
    opcode=[0x0F, 0x7E],
    operands=[Operand(type="RM", size=64, relaxed=True, dest="EA"),
              Operand(type="SIMDReg", size=64, dest="Spare")])
add_group("movd",
    cpu=["SSE2"],
    prefix=0x66,
    opcode=[0x0F, 0x6E],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="RM", size=32, relaxed=True, dest="EA")])
add_group("movd",
    cpu=["SSE2"],
    opersize=64,
    prefix=0x66,
    opcode=[0x0F, 0x6E],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="RM", size=64, relaxed=True, dest="EA")])
add_group("movd",
    cpu=["SSE2"],
    prefix=0x66,
    opcode=[0x0F, 0x7E],
    operands=[Operand(type="RM", size=32, relaxed=True, dest="EA"),
              Operand(type="SIMDReg", size=128, dest="Spare")])
add_group("movd",
    cpu=["SSE2"],
    opersize=64,
    prefix=0x66,
    opcode=[0x0F, 0x7E],
    operands=[Operand(type="RM", size=64, relaxed=True, dest="EA"),
              Operand(type="SIMDReg", size=128, dest="Spare")])

add_insn("movd", "movd")

#
# movq
#

# MMX forms
add_group("movq",
    cpu=["MMX"],
    parsers=["nasm"],
    opcode=[0x0F, 0x6F],
    operands=[Operand(type="SIMDReg", size=64, dest="Spare"),
              Operand(type="SIMDRM", size=64, relaxed=True, dest="EA")])
add_group("movq",
    cpu=["MMX"],
    parsers=["nasm"],
    opersize=64,
    opcode=[0x0F, 0x6E],
    operands=[Operand(type="SIMDReg", size=64, dest="Spare"),
              Operand(type="RM", size=64, relaxed=True, dest="EA")])
add_group("movq",
    cpu=["MMX"],
    parsers=["nasm"],
    opcode=[0x0F, 0x7F],
    operands=[Operand(type="SIMDRM", size=64, relaxed=True, dest="EA"),
              Operand(type="SIMDReg", size=64, dest="Spare")])
add_group("movq",
    cpu=["MMX"],
    parsers=["nasm"],
    opersize=64,
    opcode=[0x0F, 0x7E],
    operands=[Operand(type="RM", size=64, relaxed=True, dest="EA"),
              Operand(type="SIMDReg", size=64, dest="Spare")])

# SSE2 forms
add_group("movq",
    cpu=["SSE2"],
    parsers=["nasm"],
    prefix=0xF3,
    opcode=[0x0F, 0x7E],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="SIMDReg", size=128, dest="EA")])
add_group("movq",
    cpu=["SSE2"],
    parsers=["nasm"],
    prefix=0xF3,
    opcode=[0x0F, 0x7E],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="SIMDRM", size=64, relaxed=True, dest="EA")])
add_group("movq",
    cpu=["SSE2"],
    parsers=["nasm"],
    opersize=64,
    prefix=0x66,
    opcode=[0x0F, 0x6E],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="RM", size=64, relaxed=True, dest="EA")])
add_group("movq",
    cpu=["SSE2"],
    parsers=["nasm"],
    prefix=0x66,
    opcode=[0x0F, 0xD6],
    operands=[Operand(type="SIMDRM", size=64, relaxed=True, dest="EA"),
              Operand(type="SIMDReg", size=128, dest="Spare")])
add_group("movq",
    cpu=["SSE2"],
    parsers=["nasm"],
    opersize=64,
    prefix=0x66,
    opcode=[0x0F, 0x7E],
    operands=[Operand(type="RM", size=64, relaxed=True, dest="EA"),
              Operand(type="SIMDReg", size=128, dest="Spare")])

add_insn("movq", "movq")

add_group("mmxsse2",
    cpu=["MMX"],
    modifiers=["Op1Add"],
    opcode=[0x0F, 0x00],
    operands=[Operand(type="SIMDReg", size=64, dest="Spare"),
              Operand(type="SIMDRM", size=64, relaxed=True, dest="EA")])
add_group("mmxsse2",
    cpu=["SSE2"],
    modifiers=["Op1Add"],
    prefix=0x66,
    opcode=[0x0F, 0x00],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="SIMDRM", size=128, relaxed=True, dest="EA")])

add_insn("packssdw",  "mmxsse2", modifiers=[0x6B])
add_insn("packsswb",  "mmxsse2", modifiers=[0x63])
add_insn("packuswb",  "mmxsse2", modifiers=[0x67])
add_insn("paddb",     "mmxsse2", modifiers=[0xFC])
add_insn("paddw",     "mmxsse2", modifiers=[0xFD])
add_insn("paddd",     "mmxsse2", modifiers=[0xFE])
add_insn("paddq",     "mmxsse2", modifiers=[0xD4])
add_insn("paddsb",    "mmxsse2", modifiers=[0xEC])
add_insn("paddsw",    "mmxsse2", modifiers=[0xED])
add_insn("paddusb",   "mmxsse2", modifiers=[0xDC])
add_insn("paddusw",   "mmxsse2", modifiers=[0xDD])
add_insn("pand",      "mmxsse2", modifiers=[0xDB])
add_insn("pandn",     "mmxsse2", modifiers=[0xDF])
add_insn("pcmpeqb",   "mmxsse2", modifiers=[0x74])
add_insn("pcmpeqw",   "mmxsse2", modifiers=[0x75])
add_insn("pcmpeqd",   "mmxsse2", modifiers=[0x76])
add_insn("pcmpgtb",   "mmxsse2", modifiers=[0x64])
add_insn("pcmpgtw",   "mmxsse2", modifiers=[0x65])
add_insn("pcmpgtd",   "mmxsse2", modifiers=[0x66])
add_insn("pmaddwd",   "mmxsse2", modifiers=[0xF5])
add_insn("pmulhw",    "mmxsse2", modifiers=[0xE5])
add_insn("pmullw",    "mmxsse2", modifiers=[0xD5])
add_insn("por",       "mmxsse2", modifiers=[0xEB])
add_insn("psubb",     "mmxsse2", modifiers=[0xF8])
add_insn("psubw",     "mmxsse2", modifiers=[0xF9])
add_insn("psubd",     "mmxsse2", modifiers=[0xFA])
add_insn("psubq",     "mmxsse2", modifiers=[0xFB])
add_insn("psubsb",    "mmxsse2", modifiers=[0xE8])
add_insn("psubsw",    "mmxsse2", modifiers=[0xE9])
add_insn("psubusb",   "mmxsse2", modifiers=[0xD8])
add_insn("psubusw",   "mmxsse2", modifiers=[0xD9])
add_insn("punpckhbw", "mmxsse2", modifiers=[0x68])
add_insn("punpckhwd", "mmxsse2", modifiers=[0x69])
add_insn("punpckhdq", "mmxsse2", modifiers=[0x6A])
add_insn("punpcklbw", "mmxsse2", modifiers=[0x60])
add_insn("punpcklwd", "mmxsse2", modifiers=[0x61])
add_insn("punpckldq", "mmxsse2", modifiers=[0x62])
add_insn("pxor",      "mmxsse2", modifiers=[0xEF])

add_group("pshift",
    cpu=["MMX"],
    modifiers=["Op1Add"],
    opcode=[0x0F, 0x00],
    operands=[Operand(type="SIMDReg", size=64, dest="Spare"),
              Operand(type="SIMDRM", size=64, relaxed=True, dest="EA")])
add_group("pshift",
    cpu=["MMX"],
    modifiers=["Gap", "Op1Add", "SpAdd"],
    opcode=[0x0F, 0x00],
    spare=0,
    operands=[Operand(type="SIMDReg", size=64, dest="EA"),
              Operand(type="Imm", size=8, relaxed=True, dest="Imm")])
add_group("pshift",
    cpu=["SSE2"],
    modifiers=["Op1Add"],
    prefix=0x66,
    opcode=[0x0F, 0x00],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="SIMDRM", size=128, relaxed=True, dest="EA")])
add_group("pshift",
    cpu=["SSE2"],
    modifiers=["Gap", "Op1Add", "SpAdd"],
    prefix=0x66,
    opcode=[0x0F, 0x00],
    spare=0,
    operands=[Operand(type="SIMDReg", size=128, dest="EA"),
              Operand(type="Imm", size=8, relaxed=True, dest="Imm")])

add_insn("psllw", "pshift", modifiers=[0xF1, 0x71, 6])
add_insn("pslld", "pshift", modifiers=[0xF2, 0x72, 6])
add_insn("psllq", "pshift", modifiers=[0xF3, 0x73, 6])
add_insn("psraw", "pshift", modifiers=[0xE1, 0x71, 4])
add_insn("psrad", "pshift", modifiers=[0xE2, 0x72, 4])
add_insn("psrlw", "pshift", modifiers=[0xD1, 0x71, 2])
add_insn("psrld", "pshift", modifiers=[0xD2, 0x72, 2])
add_insn("psrlq", "pshift", modifiers=[0xD3, 0x73, 2])

#
# PIII (Katmai) new instructions / SIMD instructions
#
add_insn("pavgb",   "mmxsse2", modifiers=[0xE0], cpu=["P3", "MMX"])
add_insn("pavgw",   "mmxsse2", modifiers=[0xE3], cpu=["P3", "MMX"])
add_insn("pmaxsw",  "mmxsse2", modifiers=[0xEE], cpu=["P3", "MMX"])
add_insn("pmaxub",  "mmxsse2", modifiers=[0xDE], cpu=["P3", "MMX"])
add_insn("pminsw",  "mmxsse2", modifiers=[0xEA], cpu=["P3", "MMX"])
add_insn("pminub",  "mmxsse2", modifiers=[0xDA], cpu=["P3", "MMX"])
add_insn("pmulhuw", "mmxsse2", modifiers=[0xE4], cpu=["P3", "MMX"])
add_insn("psadbw",  "mmxsse2", modifiers=[0xF6], cpu=["P3", "MMX"])

add_insn("prefetchnta", "twobytemem", modifiers=[0, 0x0F, 0x18], cpu=["P3"])
add_insn("prefetcht0", "twobytemem", modifiers=[1, 0x0F, 0x18], cpu=["P3"])
add_insn("prefetcht1", "twobytemem", modifiers=[2, 0x0F, 0x18], cpu=["P3"])
add_insn("prefetcht2", "twobytemem", modifiers=[3, 0x0F, 0x18], cpu=["P3"])

add_insn("sfence", "threebyte", modifiers=[0x0F, 0xAE, 0xF8], cpu=["P3"])

add_group("sseps",
    cpu=["SSE"],
    modifiers=["Op1Add"],
    opcode=[0x0F, 0x00],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="SIMDRM", size=128, relaxed=True, dest="EA")])

add_insn("addps",    "sseps", modifiers=[0x58])
add_insn("andnps",   "sseps", modifiers=[0x55])
add_insn("andps",    "sseps", modifiers=[0x54])
add_insn("comiss",   "sseps", modifiers=[0x2F])
add_insn("divps",    "sseps", modifiers=[0x5E])
add_insn("maxps",    "sseps", modifiers=[0x5F])
add_insn("minps",    "sseps", modifiers=[0x5D])
add_insn("mulps",    "sseps", modifiers=[0x59])
add_insn("orps",     "sseps", modifiers=[0x56])
add_insn("rcpps",    "sseps", modifiers=[0x53])
add_insn("rsqrtps",  "sseps", modifiers=[0x52])
add_insn("sqrtps",   "sseps", modifiers=[0x51])
add_insn("subps",    "sseps", modifiers=[0x5C])
add_insn("unpckhps", "sseps", modifiers=[0x15])
add_insn("unpcklps", "sseps", modifiers=[0x14])
add_insn("xorps",    "sseps", modifiers=[0x57])

add_group("cvt_rx_xmm32",
    suffix="l",
    cpu=["SSE"],
    modifiers=["PreAdd", "Op1Add"],
    prefix=0x00,
    opcode=[0x0F, 0x00],
    operands=[Operand(type="Reg", size=32, dest="Spare"),
              Operand(type="SIMDReg", size=128, dest="EA")])
add_group("cvt_rx_xmm32",
    suffix="l",
    cpu=["SSE"],
    modifiers=["PreAdd", "Op1Add"],
    prefix=0x00,
    opcode=[0x0F, 0x00],
    operands=[Operand(type="Reg", size=32, dest="Spare"),
              Operand(type="Mem", size=32, relaxed=True, dest="EA")])
# REX
add_group("cvt_rx_xmm32",
    suffix="q",
    cpu=["SSE"],
    modifiers=["PreAdd", "Op1Add"],
    opersize=64,
    prefix=0x00,
    opcode=[0x0F, 0x00],
    operands=[Operand(type="Reg", size=64, dest="Spare"),
              Operand(type="SIMDReg", size=128, dest="EA")])
add_group("cvt_rx_xmm32",
    suffix="q",
    cpu=["SSE"],
    modifiers=["PreAdd", "Op1Add"],
    opersize=64,
    prefix=0x00,
    opcode=[0x0F, 0x00],
    operands=[Operand(type="Reg", size=64, dest="Spare"),
              Operand(type="Mem", size=32, relaxed=True, dest="EA")])

add_insn("cvtss2si", "cvt_rx_xmm32", modifiers=[0xF3, 0x2D])
add_insn("cvttss2si", "cvt_rx_xmm32", modifiers=[0xF3, 0x2C])

add_group("cvt_mm_xmm64",
    cpu=["SSE"],
    modifiers=["Op1Add"],
    opcode=[0x0F, 0x00],
    operands=[Operand(type="SIMDReg", size=64, dest="Spare"),
              Operand(type="SIMDReg", size=128, dest="EA")])
add_group("cvt_mm_xmm64",
    cpu=["SSE"],
    modifiers=["Op1Add"],
    opcode=[0x0F, 0x00],
    operands=[Operand(type="SIMDReg", size=64, dest="Spare"),
              Operand(type="Mem", size=64, relaxed=True, dest="EA")])

add_insn("cvtps2pi", "cvt_mm_xmm64", modifiers=[0x2D])
add_insn("cvttps2pi", "cvt_mm_xmm64", modifiers=[0x2C])

add_group("cvt_xmm_mm_ps",
    cpu=["SSE"],
    modifiers=["Op1Add"],
    opcode=[0x0F, 0x00],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="SIMDRM", size=64, relaxed=True, dest="EA")])

add_insn("cvtpi2ps", "cvt_xmm_mm_ps", modifiers=[0x2A])

add_group("cvt_xmm_rmx",
    suffix="l",
    cpu=["SSE"],
    modifiers=["PreAdd", "Op1Add"],
    prefix=0x00,
    opcode=[0x0F, 0x00],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="RM", size=32, relaxed=True, dest="EA")])
# REX
add_group("cvt_xmm_rmx",
    suffix="q",
    cpu=["SSE"],
    modifiers=["PreAdd", "Op1Add"],
    opersize=64,
    prefix=0x00,
    opcode=[0x0F, 0x00],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="RM", size=64, relaxed=True, dest="EA")])

add_insn("cvtsi2ss", "cvt_xmm_rmx", modifiers=[0xF3, 0x2A])

add_group("ssess",
    cpu=["SSE"],
    modifiers=["PreAdd", "Op1Add"],
    prefix=0x00,
    opcode=[0x0F, 0x00],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="SIMDRM", size=128, relaxed=True, dest="EA")])

add_insn("addss",   "ssess", modifiers=[0xF3, 0x58])
add_insn("divss",   "ssess", modifiers=[0xF3, 0x5E])
add_insn("maxss",   "ssess", modifiers=[0xF3, 0x5F])
add_insn("minss",   "ssess", modifiers=[0xF3, 0x5D])
add_insn("mulss",   "ssess", modifiers=[0xF3, 0x59])
add_insn("rcpss",   "ssess", modifiers=[0xF3, 0x53])
add_insn("rsqrtss", "ssess", modifiers=[0xF3, 0x52])
add_insn("sqrtss",  "ssess", modifiers=[0xF3, 0x51])
add_insn("subss",   "ssess", modifiers=[0xF3, 0x5C])
add_insn("ucomiss", "ssess", modifiers=[0, 0x2E])

add_group("ssecmpps",
    cpu=["SSE"],
    modifiers=["Imm8"],
    opcode=[0x0F, 0xC2],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="SIMDRM", size=128, relaxed=True, dest="EA")])

add_insn("cmpeqps",    "ssecmpps", modifiers=[0x00])
add_insn("cmpleps",    "ssecmpps", modifiers=[0x02])
add_insn("cmpltps",    "ssecmpps", modifiers=[0x01])
add_insn("cmpneqps",   "ssecmpps", modifiers=[0x04])
add_insn("cmpnleps",   "ssecmpps", modifiers=[0x06])
add_insn("cmpnltps",   "ssecmpps", modifiers=[0x05])
add_insn("cmpordps",   "ssecmpps", modifiers=[0x07])
add_insn("cmpunordps", "ssecmpps", modifiers=[0x03])

add_group("ssecmpss",
    cpu=["SSE"],
    modifiers=["Imm8", "PreAdd"],
    prefix=0x00,
    opcode=[0x0F, 0xC2],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="SIMDRM", size=128, relaxed=True, dest="EA")])

add_insn("cmpeqss",    "ssecmpss", modifiers=[0, 0xF3])
add_insn("cmpless",    "ssecmpss", modifiers=[2, 0xF3])
add_insn("cmpltss",    "ssecmpss", modifiers=[1, 0xF3])
add_insn("cmpneqss",   "ssecmpss", modifiers=[4, 0xF3])
add_insn("cmpnless",   "ssecmpss", modifiers=[6, 0xF3])
add_insn("cmpnltss",   "ssecmpss", modifiers=[5, 0xF3])
add_insn("cmpordss",   "ssecmpss", modifiers=[7, 0xF3])
add_insn("cmpunordss", "ssecmpss", modifiers=[3, 0xF3])

add_group("ssepsimm",
    cpu=["SSE"],
    modifiers=["Op1Add"],
    opcode=[0x0F, 0x00],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="SIMDRM", size=128, relaxed=True, dest="EA"),
              Operand(type="Imm", size=8, relaxed=True, dest="Imm")])

add_insn("cmpps", "ssepsimm", modifiers=[0xC2])
add_insn("shufps", "ssepsimm", modifiers=[0xC6])

add_group("ssessimm",
    cpu=["SSE"],
    modifiers=["PreAdd", "Op1Add"],
    prefix=0x00,
    opcode=[0x0F, 0x00],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="SIMDRM", size=128, relaxed=True, dest="EA"),
              Operand(type="Imm", size=8, relaxed=True, dest="Imm")])

add_insn("cmpss", "ssessimm", modifiers=[0xF3, 0xC2])

add_group("ldstmxcsr",
    cpu=["SSE"],
    modifiers=["SpAdd"],
    opcode=[0x0F, 0xAE],
    spare=0,
    operands=[Operand(type="Mem", size=32, relaxed=True, dest="EA")])

add_insn("ldmxcsr", "ldstmxcsr", modifiers=[2])
add_insn("stmxcsr", "ldstmxcsr", modifiers=[3])

add_group("maskmovq",
    cpu=["MMX", "P3"],
    opcode=[0x0F, 0xF7],
    operands=[Operand(type="SIMDReg", size=64, dest="Spare"),
              Operand(type="SIMDReg", size=64, dest="EA")])

add_insn("maskmovq", "maskmovq")

add_group("movaups",
    cpu=["SSE"],
    modifiers=["Op1Add"],
    opcode=[0x0F, 0x00],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="SIMDRM", size=128, relaxed=True, dest="EA")])
add_group("movaups",
    cpu=["SSE"],
    modifiers=["Op1Add"],
    opcode=[0x0F, 0x01],
    operands=[Operand(type="SIMDRM", size=128, relaxed=True, dest="EA"),
              Operand(type="SIMDReg", size=128, dest="Spare")])

add_insn("movaps", "movaups", modifiers=[0x28])
add_insn("movups", "movaups", modifiers=[0x10])

add_group("movhllhps",
    cpu=["SSE"],
    modifiers=["Op1Add"],
    opcode=[0x0F, 0x00],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="SIMDReg", size=128, dest="EA")])

add_insn("movhlps", "movhllhps", modifiers=[0x12])
add_insn("movlhps", "movhllhps", modifiers=[0x16])

add_group("movhlps",
    cpu=["SSE"],
    modifiers=["Op1Add"],
    opcode=[0x0F, 0x00],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="Mem", size=64, relaxed=True, dest="EA")])
add_group("movhlps",
    cpu=["SSE"],
    modifiers=["Op1Add"],
    opcode=[0x0F, 0x01],
    operands=[Operand(type="Mem", size=64, relaxed=True, dest="EA"),
              Operand(type="SIMDReg", size=128, dest="Spare")])

add_insn("movhps", "movhlps", modifiers=[0x16])
add_insn("movlps", "movhlps", modifiers=[0x12])

add_group("movmskps",
    suffix="l",
    cpu=["SSE"],
    opcode=[0x0F, 0x50],
    operands=[Operand(type="Reg", size=32, dest="Spare"),
              Operand(type="SIMDReg", size=128, dest="EA")])
add_group("movmskps",
    suffix="q",
    cpu=["SSE"],
    opersize=64,
    opcode=[0x0F, 0x50],
    operands=[Operand(type="Reg", size=64, dest="Spare"),
              Operand(type="SIMDReg", size=128, dest="EA")])

add_insn("movmskps", "movmskps")

add_group("movntps",
    cpu=["SSE"],
    opcode=[0x0F, 0x2B],
    operands=[Operand(type="Mem", size=128, relaxed=True, dest="EA"),
              Operand(type="SIMDReg", size=128, dest="Spare")])

add_insn("movntps", "movntps")

add_group("movntq",
    cpu=["SSE"],
    opcode=[0x0F, 0xE7],
    operands=[Operand(type="Mem", size=64, relaxed=True, dest="EA"),
              Operand(type="SIMDReg", size=64, dest="Spare")])

add_insn("movntq", "movntq")

add_group("movss",
    cpu=["SSE"],
    prefix=0xF3,
    opcode=[0x0F, 0x10],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="SIMDReg", size=128, dest="EA")])
add_group("movss",
    cpu=["SSE"],
    prefix=0xF3,
    opcode=[0x0F, 0x10],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="Mem", size=32, relaxed=True, dest="EA")])
add_group("movss",
    cpu=["SSE"],
    prefix=0xF3,
    opcode=[0x0F, 0x11],
    operands=[Operand(type="Mem", size=32, relaxed=True, dest="EA"),
              Operand(type="SIMDReg", size=128, dest="Spare")])
    
add_insn("movss", "movss")

add_group("pextrw",
    suffix="l",
    cpu=["MMX", "P3"],
    opcode=[0x0F, 0xC5],
    operands=[Operand(type="Reg", size=32, dest="Spare"),
              Operand(type="SIMDReg", size=64, dest="EA"),
              Operand(type="Imm", size=8, relaxed=True, dest="Imm")])
add_group("pextrw",
    suffix="l",
    cpu=["SSE2"],
    prefix=0x66,
    opcode=[0x0F, 0xC5],
    operands=[Operand(type="Reg", size=32, dest="Spare"),
              Operand(type="SIMDReg", size=128, dest="EA"),
              Operand(type="Imm", size=8, relaxed=True, dest="Imm")])
add_group("pextrw",
    suffix="q",
    cpu=["MMX", "P3"],
    opersize=64,
    opcode=[0x0F, 0xC5],
    operands=[Operand(type="Reg", size=64, dest="Spare"),
              Operand(type="SIMDReg", size=64, dest="EA"),
              Operand(type="Imm", size=8, relaxed=True, dest="Imm")])
add_group("pextrw",
    suffix="q",
    cpu=["SSE2"],
    opersize=64,
    prefix=0x66,
    opcode=[0x0F, 0xC5],
    operands=[Operand(type="Reg", size=64, dest="Spare"),
              Operand(type="SIMDReg", size=128, dest="EA"),
              Operand(type="Imm", size=8, relaxed=True, dest="Imm")])
# SSE41 instructions
add_group("pextrw",
    cpu=["SSE41"],
    prefix=0x66,
    opcode=[0x0F, 0x3A, 0x15],
    operands=[Operand(type="Mem", size=16, relaxed=True, dest="EA"),
              Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="Imm", size=8, relaxed=True, dest="Imm")])
add_group("pextrw",
    cpu=["SSE41"],
    opersize=32,
    prefix=0x66,
    opcode=[0x0F, 0x3A, 0x15],
    operands=[Operand(type="Reg", size=32, dest="EA"),
              Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="Imm", size=8, relaxed=True, dest="Imm")])
add_group("pextrw",
    cpu=["SSE41"],
    opersize=64,
    prefix=0x66,
    opcode=[0x0F, 0x3A, 0x15],
    operands=[Operand(type="Reg", size=64, dest="EA"),
              Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="Imm", size=8, relaxed=True, dest="Imm")])

add_insn("pextrw", "pextrw")

add_group("pinsrw",
    suffix="l",
    cpu=["MMX", "P3"],
    opcode=[0x0F, 0xC4],
    operands=[Operand(type="SIMDReg", size=64, dest="Spare"),
              Operand(type="Reg", size=32, dest="EA"),
              Operand(type="Imm", size=8, relaxed=True, dest="Imm")])
add_group("pinsrw",
    suffix="q",
    cpu=["MMX", "P3"],
    opersize=64,
    opcode=[0x0F, 0xC4],
    operands=[Operand(type="SIMDReg", size=64, dest="Spare"),
              Operand(type="Reg", size=64, dest="EA"),
              Operand(type="Imm", size=8, relaxed=True, dest="Imm")])
add_group("pinsrw",
    suffix="l",
    cpu=["MMX", "P3"],
    opcode=[0x0F, 0xC4],
    operands=[Operand(type="SIMDReg", size=64, dest="Spare"),
              Operand(type="Mem", size=16, relaxed=True, dest="EA"),
              Operand(type="Imm", size=8, relaxed=True, dest="Imm")])
add_group("pinsrw",
    suffix="l",
    cpu=["SSE2"],
    prefix=0x66,
    opcode=[0x0F, 0xC4],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="Reg", size=32, dest="EA"),
              Operand(type="Imm", size=8, relaxed=True, dest="Imm")])
add_group("pinsrw",
    suffix="q",
    cpu=["SSE2"],
    opersize=64,
    prefix=0x66,
    opcode=[0x0F, 0xC4],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="Reg", size=64, dest="EA"),
              Operand(type="Imm", size=8, relaxed=True, dest="Imm")])
add_group("pinsrw",
    suffix="l",
    cpu=["SSE2"],
    prefix=0x66,
    opcode=[0x0F, 0xC4],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="Mem", size=16, relaxed=True, dest="EA"),
              Operand(type="Imm", size=8, relaxed=True, dest="Imm")])

add_insn("pinsrw", "pinsrw")

add_group("pmovmskb",
    suffix="l",
    cpu=["MMX", "P3"],
    opcode=[0x0F, 0xD7],
    operands=[Operand(type="Reg", size=32, dest="Spare"),
              Operand(type="SIMDReg", size=64, dest="EA")])
add_group("pmovmskb",
    suffix="l",
    cpu=["SSE2"],
    prefix=0x66,
    opcode=[0x0F, 0xD7],
    operands=[Operand(type="Reg", size=32, dest="Spare"),
              Operand(type="SIMDReg", size=128, dest="EA")])
add_group("pmovmskb",
    suffix="q",
    cpu=["MMX", "P3"],
    opersize=64,
    opcode=[0x0F, 0xD7],
    operands=[Operand(type="Reg", size=64, dest="Spare"),
              Operand(type="SIMDReg", size=64, dest="EA")])
add_group("pmovmskb",
    suffix="q",
    cpu=["SSE2"],
    opersize=64,
    prefix=0x66,
    opcode=[0x0F, 0xD7],
    operands=[Operand(type="Reg", size=64, dest="Spare"),
              Operand(type="SIMDReg", size=128, dest="EA")])

add_insn("pmovmskb", "pmovmskb")

add_group("pshufw",
    cpu=["MMX", "P3"],
    opcode=[0x0F, 0x70],
    operands=[Operand(type="SIMDReg", size=64, dest="Spare"),
              Operand(type="SIMDRM", size=64, relaxed=True, dest="EA"),
              Operand(type="Imm", size=8, relaxed=True, dest="Imm")])

add_insn("pshufw", "pshufw")

#####################################################################
# SSE2 instructions
#####################################################################
add_insn("addpd",    "ssess", modifiers=[0x66, 0x58], cpu=["SSE2"])
add_insn("addsd",    "ssess", modifiers=[0xF2, 0x58], cpu=["SSE2"])
add_insn("andnpd",   "ssess", modifiers=[0x66, 0x55], cpu=["SSE2"])
add_insn("andpd",    "ssess", modifiers=[0x66, 0x54], cpu=["SSE2"])
add_insn("comisd",   "ssess", modifiers=[0x66, 0x2F], cpu=["SSE2"])
add_insn("divpd",    "ssess", modifiers=[0x66, 0x5E], cpu=["SSE2"])
add_insn("divsd",    "ssess", modifiers=[0xF2, 0x5E], cpu=["SSE2"])
add_insn("maxpd",    "ssess", modifiers=[0x66, 0x5F], cpu=["SSE2"])
add_insn("maxsd",    "ssess", modifiers=[0xF2, 0x5F], cpu=["SSE2"])
add_insn("minpd",    "ssess", modifiers=[0x66, 0x5D], cpu=["SSE2"])
add_insn("minsd",    "ssess", modifiers=[0xF2, 0x5D], cpu=["SSE2"])
add_insn("mulpd",    "ssess", modifiers=[0x66, 0x59], cpu=["SSE2"])
add_insn("mulsd",    "ssess", modifiers=[0xF2, 0x59], cpu=["SSE2"])
add_insn("orpd",     "ssess", modifiers=[0x66, 0x56], cpu=["SSE2"])
add_insn("sqrtpd",   "ssess", modifiers=[0x66, 0x51], cpu=["SSE2"])
add_insn("sqrtsd",   "ssess", modifiers=[0xF2, 0x51], cpu=["SSE2"])
add_insn("subpd",    "ssess", modifiers=[0x66, 0x5C], cpu=["SSE2"])
add_insn("subsd",    "ssess", modifiers=[0xF2, 0x5C], cpu=["SSE2"])
add_insn("ucomisd",  "ssess", modifiers=[0x66, 0x2E], cpu=["SSE2"])
add_insn("unpckhpd", "ssess", modifiers=[0x66, 0x15], cpu=["SSE2"])
add_insn("unpcklpd", "ssess", modifiers=[0x66, 0x14], cpu=["SSE2"])
add_insn("xorpd",    "ssess", modifiers=[0x66, 0x57], cpu=["SSE2"])
add_insn("cvtpd2dq", "ssess", modifiers=[0xF2, 0xE6], cpu=["SSE2"])
add_insn("cvtpd2ps", "ssess", modifiers=[0x66, 0x5A], cpu=["SSE2"])
add_insn("cvtps2dq", "ssess", modifiers=[0x66, 0x5B], cpu=["SSE2"])

add_insn("cvtdq2ps", "sseps", modifiers=[0x5B], cpu=["SSE2"])

add_insn("cmpeqpd",    "ssecmpss", modifiers=[0x00, 0x66], cpu=["SSE2"])
add_insn("cmpeqsd",    "ssecmpss", modifiers=[0x00, 0xF2], cpu=["SSE2"])
add_insn("cmplepd",    "ssecmpss", modifiers=[0x02, 0x66], cpu=["SSE2"])
add_insn("cmplesd",    "ssecmpss", modifiers=[0x02, 0xF2], cpu=["SSE2"])
add_insn("cmpltpd",    "ssecmpss", modifiers=[0x01, 0x66], cpu=["SSE2"])
add_insn("cmpltsd",    "ssecmpss", modifiers=[0x01, 0xF2], cpu=["SSE2"])
add_insn("cmpneqpd",   "ssecmpss", modifiers=[0x04, 0x66], cpu=["SSE2"])
add_insn("cmpneqsd",   "ssecmpss", modifiers=[0x04, 0xF2], cpu=["SSE2"])
add_insn("cmpnlepd",   "ssecmpss", modifiers=[0x06, 0x66], cpu=["SSE2"])
add_insn("cmpnlesd",   "ssecmpss", modifiers=[0x06, 0xF2], cpu=["SSE2"])
add_insn("cmpnltpd",   "ssecmpss", modifiers=[0x05, 0x66], cpu=["SSE2"])
add_insn("cmpnltsd",   "ssecmpss", modifiers=[0x05, 0xF2], cpu=["SSE2"])
add_insn("cmpordpd",   "ssecmpss", modifiers=[0x07, 0x66], cpu=["SSE2"])
add_insn("cmpordsd",   "ssecmpss", modifiers=[0x07, 0xF2], cpu=["SSE2"])
add_insn("cmpunordpd", "ssecmpss", modifiers=[0x03, 0x66], cpu=["SSE2"])
add_insn("cmpunordsd", "ssecmpss", modifiers=[0x03, 0xF2], cpu=["SSE2"])

add_insn("cmppd",  "ssessimm", modifiers=[0x66, 0xC2], cpu=["SSE2"])
add_insn("shufpd", "ssessimm", modifiers=[0x66, 0xC6], cpu=["SSE2"])

add_insn("cvtsi2sd", "cvt_xmm_rmx", modifiers=[0xF2, 0x2A], cpu=["SSE2"])

add_group("cvt_xmm_xmm64_ss",
    cpu=["SSE2"],
    modifiers=["PreAdd", "Op1Add"],
    prefix=0x00,
    opcode=[0x0F, 0x00],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="SIMDReg", size=128, dest="EA")])
add_group("cvt_xmm_xmm64_ss",
    cpu=["SSE2"],
    modifiers=["PreAdd", "Op1Add"],
    prefix=0x00,
    opcode=[0x0F, 0x00],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="Mem", size=64, relaxed=True, dest="EA")])

add_insn("cvtdq2pd", "cvt_xmm_xmm64_ss", modifiers=[0xF3, 0xE6])
add_insn("cvtsd2ss", "cvt_xmm_xmm64_ss", modifiers=[0xF2, 0x5A])

add_group("cvt_xmm_xmm64_ps",
    cpu=["SSE2"],
    modifiers=["Op1Add"],
    opcode=[0x0F, 0x00],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="SIMDReg", size=128, dest="EA")])
add_group("cvt_xmm_xmm64_ps",
    cpu=["SSE2"],
    modifiers=["Op1Add"],
    opcode=[0x0F, 0x00],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="Mem", size=64, relaxed=True, dest="EA")])

add_insn("cvtps2pd", "cvt_xmm_xmm64_ps", modifiers=[0x5A])

add_group("cvt_rx_xmm64",
    suffix="l",
    cpu=["SSE2"],
    modifiers=["PreAdd", "Op1Add"],
    prefix=0x00,
    opcode=[0x0F, 0x00],
    operands=[Operand(type="Reg", size=32, dest="Spare"),
              Operand(type="SIMDReg", size=128, dest="EA")])
add_group("cvt_rx_xmm64",
    suffix="l",
    cpu=["SSE2"],
    modifiers=["PreAdd", "Op1Add"],
    prefix=0x00,
    opcode=[0x0F, 0x00],
    operands=[Operand(type="Reg", size=32, dest="Spare"),
              Operand(type="Mem", size=64, relaxed=True, dest="EA")])
# REX
add_group("cvt_rx_xmm64",
    suffix="q",
    cpu=["SSE2"],
    modifiers=["PreAdd", "Op1Add"],
    opersize=64,
    prefix=0x00,
    opcode=[0x0F, 0x00],
    operands=[Operand(type="Reg", size=64, dest="Spare"),
              Operand(type="SIMDReg", size=128, dest="EA")])
add_group("cvt_rx_xmm64",
    suffix="q",
    cpu=["SSE2"],
    modifiers=["PreAdd", "Op1Add"],
    opersize=64,
    prefix=0x00,
    opcode=[0x0F, 0x00],
    operands=[Operand(type="Reg", size=64, dest="Spare"),
              Operand(type="Mem", size=64, relaxed=True, dest="EA")])

add_insn("cvtsd2si", "cvt_rx_xmm64", modifiers=[0xF2, 0x2D])

add_group("cvt_mm_xmm",
    cpu=["SSE2"],
    modifiers=["PreAdd", "Op1Add"],
    prefix=0x00,
    opcode=[0x0F, 0x00],
    operands=[Operand(type="SIMDReg", size=64, dest="Spare"),
              Operand(type="SIMDRM", size=128, relaxed=True, dest="EA")])

add_insn("cvtpd2pi", "cvt_mm_xmm", modifiers=[0x66, 0x2D], cpu=["SSE2"])

add_group("cvt_xmm_mm_ss",
    cpu=["SSE"],
    modifiers=["PreAdd", "Op1Add"],
    prefix=0x00,
    opcode=[0x0F, 0x00],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="SIMDRM", size=64, relaxed=True, dest="EA")])

add_insn("cvtpi2pd", "cvt_xmm_mm_ss", modifiers=[0x66, 0x2A], cpu=["SSE2"])

# cmpsd SSE2 form
add_group("cmpsd",
    cpu=["SSE2"],
    prefix=0xF2,
    opcode=[0x0F, 0xC2],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="SIMDRM", size=128, relaxed=True, dest="EA"),
              Operand(type="Imm", size=8, relaxed=True, dest="Imm")])
# cmpsd is added in string instructions above, so don't re-add_insn()

add_group("movaupd",
    cpu=["SSE2"],
    modifiers=["Op1Add"],
    prefix=0x66,
    opcode=[0x0F, 0x00],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="SIMDRM", size=128, relaxed=True, dest="EA")])
add_group("movaupd",
    cpu=["SSE2"],
    modifiers=["Op1Add"],
    prefix=0x66,
    opcode=[0x0F, 0x01],
    operands=[Operand(type="SIMDRM", size=128, relaxed=True, dest="EA"),
              Operand(type="SIMDReg", size=128, dest="Spare")])

add_insn("movapd", "movaupd", modifiers=[0x28])
add_insn("movupd", "movaupd", modifiers=[0x10])

add_group("movhlpd",
    cpu=["SSE2"],
    modifiers=["Op1Add"],
    prefix=0x66,
    opcode=[0x0F, 0x00],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="Mem", size=64, relaxed=True, dest="EA")])
add_group("movhlpd",
    cpu=["SSE2"],
    modifiers=["Op1Add"],
    prefix=0x66,
    opcode=[0x0F, 0x01],
    operands=[Operand(type="Mem", size=64, relaxed=True, dest="EA"),
              Operand(type="SIMDReg", size=128, dest="Spare")])

add_insn("movhpd", "movhlpd", modifiers=[0x16])
add_insn("movlpd", "movhlpd", modifiers=[0x12])

add_group("movmskpd",
    suffix="l",
    cpu=["SSE2"],
    prefix=0x66,
    opcode=[0x0F, 0x50],
    operands=[Operand(type="Reg", size=32, dest="Spare"),
              Operand(type="SIMDReg", size=128, dest="EA")])
add_group("movmskpd",
    suffix="q",
    cpu=["SSE2"],
    prefix=0x66,
    opcode=[0x0F, 0x50],
    operands=[Operand(type="Reg", size=64, dest="Spare"),
              Operand(type="SIMDReg", size=128, dest="EA")])

add_insn("movmskpd", "movmskpd")

add_group("movntpddq",
    cpu=["SSE2"],
    modifiers=["Op1Add"],
    prefix=0x66,
    opcode=[0x0F, 0x00],
    operands=[Operand(type="Mem", size=128, relaxed=True, dest="EA"),
              Operand(type="SIMDReg", size=128, dest="Spare")])

add_insn("movntpd", "movntpddq", modifiers=[0x2B])
add_insn("movntdq", "movntpddq", modifiers=[0xE7])

# movsd SSE2 forms
add_group("movsd",
    cpu=["SSE2"],
    prefix=0xF2,
    opcode=[0x0F, 0x10],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="SIMDReg", size=128, dest="EA")])
add_group("movsd",
    cpu=["SSE2"],
    prefix=0xF2,
    opcode=[0x0F, 0x10],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="Mem", size=64, relaxed=True, dest="EA")])
add_group("movsd",
    cpu=["SSE2"],
    prefix=0xF2,
    opcode=[0x0F, 0x11],
    operands=[Operand(type="Mem", size=64, relaxed=True, dest="EA"),
              Operand(type="SIMDReg", size=128, dest="Spare")])
# movsd is added in string instructions above, so don't re-add_insn()

#####################################################################
# P4 VMX Instructions
#####################################################################

add_insn("vmcall", "threebyte", modifiers=[0x0F, 0x01, 0xC1], cpu=["P4"])
add_insn("vmlaunch", "threebyte", modifiers=[0x0F, 0x01, 0xC2], cpu=["P4"])
add_insn("vmresume", "threebyte", modifiers=[0x0F, 0x01, 0xC3], cpu=["P4"])
add_insn("vmxoff", "threebyte", modifiers=[0x0F, 0x01, 0xC4], cpu=["P4"])

add_group("vmxmemrd",
    suffix="l",
    not64=True,
    cpu=["P4"],
    opersize=32,
    opcode=[0x0F, 0x78],
    operands=[Operand(type="RM", size=32, relaxed=True, dest="EA"),
              Operand(type="Reg", size=32, dest="Spare")])
add_group("vmxmemrd",
    suffix="q",
    cpu=["P4"],
    opersize=64,
    def_opersize_64=64,
    opcode=[0x0F, 0x78],
    operands=[Operand(type="RM", size=64, relaxed=True, dest="EA"),
              Operand(type="Reg", size=64, dest="Spare")])
add_insn("vmread", "vmxmemrd")

add_group("vmxmemwr",
    suffix="l",
    not64=True,
    cpu=["P4"],
    opersize=32,
    opcode=[0x0F, 0x79],
    operands=[Operand(type="Reg", size=32, dest="Spare"),
              Operand(type="RM", size=32, relaxed=True, dest="EA")])
add_group("vmxmemwr",
    suffix="q",
    cpu=["P4"],
    opersize=64,
    def_opersize_64=64,
    opcode=[0x0F, 0x79],
    operands=[Operand(type="Reg", size=64, dest="Spare"),
              Operand(type="RM", size=64, relaxed=True, dest="EA")])
add_insn("vmwrite", "vmxmemwr")

add_group("vmxtwobytemem",
    modifiers=["SpAdd"],
    cpu=["P4"],
    opcode=[0x0F, 0xC7],
    spare=0,
    operands=[Operand(type="Mem", size=64, relaxed=True, dest="EA")])
add_insn("vmptrld", "vmxtwobytemem", modifiers=[6])
add_insn("vmptrst", "vmxtwobytemem", modifiers=[7])

add_group("vmxthreebytemem",
    modifiers=["PreAdd"],
    cpu=["P4"],
    prefix=0x00,
    opcode=[0x0F, 0xC7],
    spare=6,
    operands=[Operand(type="Mem", size=64, relaxed=True, dest="EA")])
add_insn("vmclear", "vmxthreebytemem", modifiers=[0x66])
add_insn("vmxon", "vmxthreebytemem", modifiers=[0xF3])

add_insn("cvttpd2pi", "cvt_mm_xmm", modifiers=[0x66, 0x2C], cpu=["SSE2"])
add_insn("cvttsd2si", "cvt_rx_xmm64", modifiers=[0xF2, 0x2C], cpu=["SSE2"])
add_insn("cvttpd2dq", "ssess", modifiers=[0x66, 0xE6], cpu=["SSE2"])
add_insn("cvttps2dq", "ssess", modifiers=[0xF3, 0x5B], cpu=["SSE2"])
add_insn("pmuludq", "mmxsse2", modifiers=[0xF4], cpu=["SSE2"])
add_insn("pshufd", "ssessimm", modifiers=[0x66, 0x70], cpu=["SSE2"])
add_insn("pshufhw", "ssessimm", modifiers=[0xF3, 0x70], cpu=["SSE2"])
add_insn("pshuflw", "ssessimm", modifiers=[0xF2, 0x70], cpu=["SSE2"])
add_insn("punpckhqdq", "ssess", modifiers=[0x66, 0x6D], cpu=["SSE2"])
add_insn("punpcklqdq", "ssess", modifiers=[0x66, 0x6C], cpu=["SSE2"])

add_group("cvt_xmm_xmm32",
    cpu=["SSE2"],
    modifiers=["PreAdd", "Op1Add"],
    prefix=0x00,
    opcode=[0x0F, 0x00],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="SIMDReg", size=128, dest="EA")])
add_group("cvt_xmm_xmm32",
    cpu=["SSE2"],
    modifiers=["PreAdd", "Op1Add"],
    prefix=0x00,
    opcode=[0x0F, 0x00],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="Mem", size=32, relaxed=True, dest="EA")])

add_insn("cvtss2sd", "cvt_xmm_xmm32", modifiers=[0xF3, 0x5A])

add_group("maskmovdqu",
    cpu=["SSE2"],
    prefix=0x66,
    opcode=[0x0F, 0xF7],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="SIMDReg", size=128, dest="EA")])

add_insn("maskmovdqu", "maskmovdqu")

add_group("movdqau",
    cpu=["SSE2"],
    modifiers=["PreAdd"],
    prefix=0x00,
    opcode=[0x0F, 0x6F],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="SIMDRM", size=128, relaxed=True, dest="EA")])
add_group("movdqau",
    cpu=["SSE2"],
    modifiers=["PreAdd"],
    prefix=0x00,
    opcode=[0x0F, 0x7F],
    operands=[Operand(type="SIMDRM", size=128, relaxed=True, dest="EA"),
              Operand(type="SIMDReg", size=128, dest="Spare")])

add_insn("movdqa", "movdqau", modifiers=[0x66])
add_insn("movdqu", "movdqau", modifiers=[0xF3])

add_group("movdq2q",
    cpu=["SSE2"],
    prefix=0xF2,
    opcode=[0x0F, 0xD6],
    operands=[Operand(type="SIMDReg", size=64, dest="Spare"),
              Operand(type="SIMDReg", size=128, dest="EA")])

add_insn("movdq2q", "movdq2q")

add_group("movq2dq",
    cpu=["SSE2"],
    prefix=0xF3,
    opcode=[0x0F, 0xD6],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="SIMDReg", size=64, dest="EA")])

add_insn("movq2dq", "movq2dq")

add_group("pslrldq",
    cpu=["SSE2"],
    modifiers=["SpAdd"],
    prefix=0x66,
    opcode=[0x0F, 0x73],
    spare=0,
    operands=[Operand(type="SIMDReg", size=128, dest="EA"),
              Operand(type="Imm", size=8, relaxed=True, dest="Imm")])

add_insn("pslldq", "pslrldq", modifiers=[7])
add_insn("psrldq", "pslrldq", modifiers=[3])

#####################################################################
# SSE3 / PNI Prescott New Instructions instructions
#####################################################################
add_insn("addsubpd", "ssess", modifiers=[0x66, 0xD0], cpu=["SSE3"])
add_insn("addsubps", "ssess", modifiers=[0xF2, 0xD0], cpu=["SSE3"])
add_insn("haddpd",   "ssess", modifiers=[0x66, 0x7C], cpu=["SSE3"])
add_insn("haddps",   "ssess", modifiers=[0xF2, 0x7C], cpu=["SSE3"])
add_insn("hsubpd",   "ssess", modifiers=[0x66, 0x7D], cpu=["SSE3"])
add_insn("hsubps",   "ssess", modifiers=[0xF2, 0x7D], cpu=["SSE3"])
add_insn("movshdup", "ssess", modifiers=[0xF3, 0x16], cpu=["SSE3"])
add_insn("movsldup", "ssess", modifiers=[0xF3, 0x12], cpu=["SSE3"])
add_insn("fisttp",   "fildstp", modifiers=[1, 0, 1], cpu=["SSE3"])
add_insn("fisttpll", "fildstp", suffix="q", modifiers=[7], cpu=["SSE3"])
add_insn("movddup", "cvt_xmm_xmm64_ss", modifiers=[0xF2, 0x12], cpu=["SSE3"])
add_insn("monitor", "threebyte", modifiers=[0x0F, 0x01, 0xC8], cpu=["SSE3"])
add_insn("mwait",   "threebyte", modifiers=[0x0F, 0x01, 0xC9], cpu=["SSE3"])

add_group("lddqu",
    cpu=["SSE3"],
    prefix=0xF2,
    opcode=[0x0F, 0xF0],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="Mem", dest="EA")])

add_insn("lddqu", "lddqu")

#####################################################################
# SSSE3 / TNI Tejas New Intructions instructions
#####################################################################

add_group("ssse3",
    cpu=["SSSE3"],
    modifiers=["Op2Add"],
    opcode=[0x0F, 0x38, 0x00],
    operands=[Operand(type="SIMDReg", size=64, dest="Spare"),
              Operand(type="SIMDRM", size=64, relaxed=True, dest="EA")])
add_group("ssse3",
    cpu=["SSSE3"],
    modifiers=["Op2Add"],
    prefix=0x66,
    opcode=[0x0F, 0x38, 0x00],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="SIMDRM", size=128, relaxed=True, dest="EA")])

add_insn("pshufb",    "ssse3", modifiers=[0x00])
add_insn("phaddw",    "ssse3", modifiers=[0x01])
add_insn("phaddd",    "ssse3", modifiers=[0x02])
add_insn("phaddsw",   "ssse3", modifiers=[0x03])
add_insn("pmaddubsw", "ssse3", modifiers=[0x04])
add_insn("phsubw",    "ssse3", modifiers=[0x05])
add_insn("phsubd",    "ssse3", modifiers=[0x06])
add_insn("phsubsw",   "ssse3", modifiers=[0x07])
add_insn("psignb",    "ssse3", modifiers=[0x08])
add_insn("psignw",    "ssse3", modifiers=[0x09])
add_insn("psignd",    "ssse3", modifiers=[0x0A])
add_insn("pmulhrsw",  "ssse3", modifiers=[0x0B])
add_insn("pabsb",     "ssse3", modifiers=[0x1C])
add_insn("pabsw",     "ssse3", modifiers=[0x1D])
add_insn("pabsd",     "ssse3", modifiers=[0x1E])

add_group("ssse3imm",
    cpu=["SSSE3"],
    modifiers=["Op2Add"],
    opcode=[0x0F, 0x3A, 0x00],
    operands=[Operand(type="SIMDReg", size=64, dest="Spare"),
              Operand(type="SIMDRM", size=64, relaxed=True, dest="EA"),
              Operand(type="Imm", size=8, relaxed=True, dest="Imm")])
add_group("ssse3imm",
    cpu=["SSSE3"],
    modifiers=["Op2Add"],
    prefix=0x66,
    opcode=[0x0F, 0x3A, 0x00],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="SIMDRM", size=128, relaxed=True, dest="EA"),
              Operand(type="Imm", size=8, relaxed=True, dest="Imm")])

add_insn("palignr", "ssse3imm", modifiers=[0x0F])

#####################################################################
# SSE4.1 / SSE4.2 instructions
#####################################################################

add_group("sse4",
    cpu=["SSE41"],
    modifiers=["Op2Add"],
    prefix=0x66,
    opcode=[0x0F, 0x38, 0x00],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="SIMDRM", size=128, relaxed=True, dest="EA")])

add_insn("packusdw",   "sse4", modifiers=[0x2B])
add_insn("pcmpeqq",    "sse4", modifiers=[0x29])
add_insn("pcmpgtq",    "sse4", modifiers=[0x37])
add_insn("phminposuw", "sse4", modifiers=[0x41])
add_insn("pmaxsb",     "sse4", modifiers=[0x3C])
add_insn("pmaxsd",     "sse4", modifiers=[0x3D])
add_insn("pmaxud",     "sse4", modifiers=[0x3F])
add_insn("pmaxuw",     "sse4", modifiers=[0x3E])
add_insn("pminsb",     "sse4", modifiers=[0x38])
add_insn("pminsd",     "sse4", modifiers=[0x39])
add_insn("pminud",     "sse4", modifiers=[0x3B])
add_insn("pminuw",     "sse4", modifiers=[0x3A])
add_insn("pmuldq",     "sse4", modifiers=[0x28])
add_insn("pmulld",     "sse4", modifiers=[0x40])
add_insn("ptest",      "sse4", modifiers=[0x17])

add_group("sse4imm",
    cpu=["SSE41"],
    modifiers=["Op2Add"],
    prefix=0x66,
    opcode=[0x0F, 0x3A, 0x00],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="SIMDRM", size=128, relaxed=True, dest="EA"),
              Operand(type="Imm", size=8, relaxed=True, dest="Imm")])

add_insn("blendpd", "sse4imm", modifiers=[0x0D])
add_insn("blendps", "sse4imm", modifiers=[0x0C])
add_insn("dppd",    "sse4imm", modifiers=[0x41])
add_insn("dpps",    "sse4imm", modifiers=[0x40])
add_insn("mpsadbw", "sse4imm", modifiers=[0x42])
add_insn("pblendw", "sse4imm", modifiers=[0x0E])
add_insn("roundpd", "sse4imm", modifiers=[0x09])
add_insn("roundps", "sse4imm", modifiers=[0x08])
add_insn("roundsd", "sse4imm", modifiers=[0x0B])
add_insn("roundss", "sse4imm", modifiers=[0x0A])

add_group("sse4xmm0",
    cpu=["SSE41"],
    modifiers=["Op2Add"],
    prefix=0x66,
    opcode=[0x0F, 0x38, 0x00],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="SIMDRM", size=128, relaxed=True, dest="EA")])
add_group("sse4xmm0",
    cpu=["SSE41"],
    modifiers=["Op2Add"],
    prefix=0x66,
    opcode=[0x0F, 0x38, 0x00],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="SIMDRM", size=128, relaxed=True, dest="EA"),
              Operand(type="XMM0", size=128, dest=None)])

add_insn("blendvpd", "sse4xmm0", modifiers=[0x15])
add_insn("blendvps", "sse4xmm0", modifiers=[0x14])
add_insn("pblendvb", "sse4xmm0", modifiers=[0x10])

for sfx, sz in zip("bwl", [8, 16, 32]):
    add_group("crc32",
        suffix=sfx,
        cpu=["SSE42"],
        opersize=sz,
        prefix=0xF2,
        opcode=[0x0F, 0x38, 0xF0+(sz!=8)],
        operands=[Operand(type="Reg", size=32, dest="Spare"),
                  Operand(type="RM", size=sz, relaxed=(sz==32), dest="EA")])
for sfx, sz in zip("bq", [8, 64]):
    add_group("crc32",
        suffix=sfx,
        cpu=["SSE42"],
        opersize=64,
        prefix=0xF2,
        opcode=[0x0F, 0x38, 0xF0+(sz!=8)],
        operands=[Operand(type="Reg", size=64, dest="Spare"),
                  Operand(type="RM", size=sz, relaxed=(sz==64), dest="EA")])

add_insn("crc32", "crc32")

add_group("extractps",
    cpu=["SSE41"],
    opersize=32,
    prefix=0x66,
    opcode=[0x0F, 0x3A, 0x17],
    operands=[Operand(type="RM", size=32, relaxed=True, dest="EA"),
              Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="Imm", size=8, relaxed=True, dest="Imm")])
add_group("extractps",
    cpu=["SSE41"],
    opersize=64,
    prefix=0x66,
    opcode=[0x0F, 0x3A, 0x17],
    operands=[Operand(type="Reg", size=64, dest="EA"),
              Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="Imm", size=8, relaxed=True, dest="Imm")])

add_insn("extractps", "extractps")

add_group("insertps",
    cpu=["SSE41"],
    prefix=0x66,
    opcode=[0x0F, 0x3A, 0x21],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="Mem", size=32, relaxed=True, dest="EA"),
              Operand(type="Imm", size=8, relaxed=True, dest="Imm")])
add_group("insertps",
    cpu=["SSE41"],
    prefix=0x66,
    opcode=[0x0F, 0x3A, 0x21],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="SIMDReg", size=128, dest="EA"),
              Operand(type="Imm", size=8, relaxed=True, dest="Imm")])

add_insn("insertps", "insertps")

add_group("movntdqa",
    cpu=["SSE41"],
    prefix=0x66,
    opcode=[0x0F, 0x38, 0x2A],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="Mem", size=128, relaxed=True, dest="EA")])

add_insn("movntdqa", "movntdqa")

add_group("sse4pcmpstr",
    cpu=["SSE42"],
    modifiers=["Op2Add"],
    prefix=0x66,
    opcode=[0x0F, 0x3A, 0x00],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="SIMDRM", size=128, relaxed=True, dest="EA"),
              Operand(type="Imm", size=8, relaxed=True, dest="Imm")])

add_insn("pcmpestri", "sse4pcmpstr", modifiers=[0x61])
add_insn("pcmpestrm", "sse4pcmpstr", modifiers=[0x60])
add_insn("pcmpistri", "sse4pcmpstr", modifiers=[0x63])
add_insn("pcmpistrm", "sse4pcmpstr", modifiers=[0x62])

add_group("pextrb",
    cpu=["SSE41"],
    prefix=0x66,
    opcode=[0x0F, 0x3A, 0x14],
    operands=[Operand(type="Mem", size=8, relaxed=True, dest="EA"),
              Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="Imm", size=8, relaxed=True, dest="Imm")])
for sz in [32, 64]:
    add_group("pextrb",
        cpu=["SSE41"],
        opersize=sz,
        prefix=0x66,
        opcode=[0x0F, 0x3A, 0x14],
        operands=[Operand(type="Reg", size=sz, dest="EA"),
                  Operand(type="SIMDReg", size=128, dest="Spare"),
                  Operand(type="Imm", size=8, relaxed=True, dest="Imm")])

add_insn("pextrb", "pextrb")

add_group("pextrd",
    cpu=["SSE41"],
    opersize=32,
    prefix=0x66,
    opcode=[0x0F, 0x3A, 0x16],
    operands=[Operand(type="RM", size=32, relaxed=True, dest="EA"),
              Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="Imm", size=8, relaxed=True, dest="Imm")])

add_insn("pextrd", "pextrd")

add_group("pextrq",
    cpu=["SSE41"],
    opersize=64,
    prefix=0x66,
    opcode=[0x0F, 0x3A, 0x16],
    operands=[Operand(type="RM", size=64, relaxed=True, dest="EA"),
              Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="Imm", size=8, relaxed=True, dest="Imm")])

add_insn("pextrq", "pextrq")

add_group("pinsrb",
    cpu=["SSE41"],
    prefix=0x66,
    opcode=[0x0F, 0x3A, 0x20],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="Mem", size=8, relaxed=True, dest="EA"),
              Operand(type="Imm", size=8, relaxed=True, dest="Imm")])
add_group("pinsrb",
    cpu=["SSE41"],
    opersize=32,
    prefix=0x66,
    opcode=[0x0F, 0x3A, 0x20],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="Reg", size=32, dest="EA"),
              Operand(type="Imm", size=8, relaxed=True, dest="Imm")])

add_insn("pinsrb", "pinsrb")

add_group("pinsrd",
    cpu=["SSE41"],
    opersize=32,
    prefix=0x66,
    opcode=[0x0F, 0x3A, 0x22],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="RM", size=32, relaxed=True, dest="EA"),
              Operand(type="Imm", size=8, relaxed=True, dest="Imm")])

add_insn("pinsrd", "pinsrd")

add_group("pinsrq",
    cpu=["SSE41"],
    opersize=64,
    prefix=0x66,
    opcode=[0x0F, 0x3A, 0x22],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="RM", size=64, relaxed=True, dest="EA"),
              Operand(type="Imm", size=8, relaxed=True, dest="Imm")])

add_insn("pinsrq", "pinsrq")

add_group("sse4m64",
    cpu=["SSE41"],
    modifiers=["Op2Add"],
    prefix=0x66,
    opcode=[0x0F, 0x38, 0x00],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="Mem", size=64, relaxed=True, dest="EA")])
add_group("sse4m64",
    cpu=["SSE41"],
    modifiers=["Op2Add"],
    prefix=0x66,
    opcode=[0x0F, 0x38, 0x00],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="SIMDReg", size=128, dest="EA")])

add_insn("pmovsxbw", "sse4m64", modifiers=[0x20])
add_insn("pmovsxwd", "sse4m64", modifiers=[0x23])
add_insn("pmovsxdq", "sse4m64", modifiers=[0x25])
add_insn("pmovzxbw", "sse4m64", modifiers=[0x30])
add_insn("pmovzxwd", "sse4m64", modifiers=[0x33])
add_insn("pmovzxdq", "sse4m64", modifiers=[0x35])

add_group("sse4m32",
    cpu=["SSE41"],
    modifiers=["Op2Add"],
    prefix=0x66,
    opcode=[0x0F, 0x38, 0x00],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="Mem", size=32, relaxed=True, dest="EA")])
add_group("sse4m32",
    cpu=["SSE41"],
    modifiers=["Op2Add"],
    prefix=0x66,
    opcode=[0x0F, 0x38, 0x00],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="SIMDReg", size=128, dest="EA")])

add_insn("pmovsxbd", "sse4m32", modifiers=[0x21])
add_insn("pmovsxwq", "sse4m32", modifiers=[0x24])
add_insn("pmovzxbd", "sse4m32", modifiers=[0x31])
add_insn("pmovzxwq", "sse4m32", modifiers=[0x34])

add_group("sse4m16",
    cpu=["SSE41"],
    modifiers=["Op2Add"],
    prefix=0x66,
    opcode=[0x0F, 0x38, 0x00],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="Mem", size=16, relaxed=True, dest="EA")])
add_group("sse4m16",
    cpu=["SSE41"],
    modifiers=["Op2Add"],
    prefix=0x66,
    opcode=[0x0F, 0x38, 0x00],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="SIMDReg", size=128, dest="EA")])

add_insn("pmovsxbq", "sse4m16", modifiers=[0x22])
add_insn("pmovzxbq", "sse4m16", modifiers=[0x32])

for sfx, sz in zip("wlq", [16, 32, 64]):
    add_group("cnt",
        suffix=sfx,
        modifiers=["Op1Add"],
        opersize=sz,
        prefix=0xF3,
        opcode=[0x0F, 0x00],
        operands=[Operand(type="Reg", size=sz, dest="Spare"),
                  Operand(type="RM", size=sz, relaxed=True, dest="EA")])

add_insn("popcnt", "cnt", modifiers=[0xB8], cpu=["SSE42"])

#####################################################################
# AMD SSE4a instructions
#####################################################################

add_group("extrq",
    cpu=["SSE4a"],
    prefix=0x66,
    opcode=[0x0F, 0x78],
    operands=[Operand(type="SIMDReg", size=128, dest="EA"),
              Operand(type="Imm", size=8, relaxed=True, dest="EA"),
              Operand(type="Imm", size=8, relaxed=True, dest="Imm")])
add_group("extrq",
    cpu=["SSE4a"],
    prefix=0x66,
    opcode=[0x0F, 0x79],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="SIMDReg", size=128, dest="EA")])

add_insn("extrq", "extrq")

add_group("insertq",
    cpu=["SSE4a"],
    prefix=0xF2,
    opcode=[0x0F, 0x78],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="SIMDReg", size=128, dest="EA"),
              Operand(type="Imm", size=8, relaxed=True, dest="EA"),
              Operand(type="Imm", size=8, relaxed=True, dest="Imm")])
add_group("insertq",
    cpu=["SSE4a"],
    prefix=0xF2,
    opcode=[0x0F, 0x79],
    operands=[Operand(type="SIMDReg", size=128, dest="Spare"),
              Operand(type="SIMDReg", size=128, dest="EA")])

add_insn("insertq", "insertq")

add_group("movntsd",
    cpu=["SSE4a"],
    prefix=0xF2,
    opcode=[0x0F, 0x2B],
    operands=[Operand(type="Mem", size=64, relaxed=True, dest="EA"),
              Operand(type="SIMDReg", size=128, dest="Spare")])

add_insn("movntsd", "movntsd")

add_group("movntss",
    cpu=["SSE4a"],
    prefix=0xF3,
    opcode=[0x0F, 0x2B],
    operands=[Operand(type="Mem", size=32, relaxed=True, dest="EA"),
              Operand(type="SIMDReg", size=128, dest="Spare")])

add_insn("movntss", "movntss")

#####################################################################
# AMD 3DNow! instructions
#####################################################################

add_insn("prefetch", "twobytemem", modifiers=[0x00, 0x0F, 0x0D], cpu=["3DNow"])
add_insn("prefetchw", "twobytemem", modifiers=[0x01, 0x0F, 0x0D], cpu=["3DNow"])
add_insn("femms", "twobyte", modifiers=[0x0F, 0x0E], cpu=["3DNow"])

add_group("now3d",
    cpu=["3DNow"],
    modifiers=["Imm8"],
    opcode=[0x0F, 0x0F],
    operands=[Operand(type="SIMDReg", size=64, dest="Spare"),
              Operand(type="SIMDRM", size=64, relaxed=True, dest="EA")])

add_insn("pavgusb", "now3d", modifiers=[0xBF])
add_insn("pf2id", "now3d", modifiers=[0x1D])
add_insn("pf2iw", "now3d", modifiers=[0x1C], cpu=["Athlon", "3DNow"])
add_insn("pfacc", "now3d", modifiers=[0xAE])
add_insn("pfadd", "now3d", modifiers=[0x9E])
add_insn("pfcmpeq", "now3d", modifiers=[0xB0])
add_insn("pfcmpge", "now3d", modifiers=[0x90])
add_insn("pfcmpgt", "now3d", modifiers=[0xA0])
add_insn("pfmax", "now3d", modifiers=[0xA4])
add_insn("pfmin", "now3d", modifiers=[0x94])
add_insn("pfmul", "now3d", modifiers=[0xB4])
add_insn("pfnacc", "now3d", modifiers=[0x8A], cpu=["Athlon", "3DNow"])
add_insn("pfpnacc", "now3d", modifiers=[0x8E], cpu=["Athlon", "3DNow"])
add_insn("pfrcp", "now3d", modifiers=[0x96])
add_insn("pfrcpit1", "now3d", modifiers=[0xA6])
add_insn("pfrcpit2", "now3d", modifiers=[0xB6])
add_insn("pfrsqit1", "now3d", modifiers=[0xA7])
add_insn("pfrsqrt", "now3d", modifiers=[0x97])
add_insn("pfsub", "now3d", modifiers=[0x9A])
add_insn("pfsubr", "now3d", modifiers=[0xAA])
add_insn("pi2fd", "now3d", modifiers=[0x0D])
add_insn("pi2fw", "now3d", modifiers=[0x0C], cpu=["Athlon", "3DNow"])
add_insn("pmulhrwa", "now3d", modifiers=[0xB7])
add_insn("pswapd", "now3d", modifiers=[0xBB], cpu=["Athlon", "3DNow"])

#####################################################################
# AMD extensions
#####################################################################

add_insn("syscall", "twobyte", modifiers=[0x0F, 0x05], cpu=["686", "AMD"])
for sfx in [None, "l", "q"]:
    add_insn("sysret"+(sfx or ""), "twobyte", suffix=sfx, modifiers=[0x0F, 0x07],
             cpu=["686", "AMD", "Priv"])
add_insn("lzcnt", "cnt", modifiers=[0xBD], cpu=["686", "AMD"])

#####################################################################
# AMD x86-64 extensions
#####################################################################

add_insn("swapgs", "threebyte", modifiers=[0x0F, 0x01, 0xF8], only64=True)
add_insn("rdtscp", "threebyte", modifiers=[0x0F, 0x01, 0xF9],
         cpu=["686", "AMD", "Priv"])

add_group("cmpxchg16b",
    only64=True,
    opersize=64,
    opcode=[0x0F, 0xC7],
    spare=1,
    operands=[Operand(type="Mem", size=128, relaxed=True, dest="EA")])

add_insn("cmpxchg16b", "cmpxchg16b")

#####################################################################
# AMD Pacifica SVM instructions
#####################################################################

add_insn("clgi", "threebyte", modifiers=[0x0F, 0x01, 0xDD], cpu=["SVM"])
add_insn("stgi", "threebyte", modifiers=[0x0F, 0x01, 0xDC], cpu=["SVM"])
add_insn("vmmcall", "threebyte", modifiers=[0x0F, 0x01, 0xD9], cpu=["SVM"])

add_group("invlpga",
    cpu=["SVM"],
    opcode=[0x0F, 0x01, 0xDF],
    operands=[])
add_group("invlpga",
    cpu=["SVM"],
    opcode=[0x0F, 0x01, 0xDF],
    operands=[Operand(type="MemrAX", dest="AdSizeEA"),
              Operand(type="Creg", size=32, dest=None)])

add_insn("invlpga", "invlpga")

add_group("skinit",
    cpu=["SVM"],
    opcode=[0x0F, 0x01, 0xDE],
    operands=[])
add_group("skinit",
    cpu=["SVM"],
    opcode=[0x0F, 0x01, 0xDE],
    operands=[Operand(type="MemEAX", dest=None)])

add_insn("skinit", "skinit")

add_group("svm_rax",
    cpu=["SVM"],
    modifiers=["Op2Add"],
    opcode=[0x0F, 0x01, 0x00],
    operands=[])
add_group("svm_rax",
    cpu=["SVM"],
    modifiers=["Op2Add"],
    opcode=[0x0F, 0x01, 0x00],
    operands=[Operand(type="MemrAX", dest="AdSizeEA")])

add_insn("vmload", "svm_rax", modifiers=[0xDA])
add_insn("vmrun", "svm_rax", modifiers=[0xD8])
add_insn("vmsave", "svm_rax", modifiers=[0xDB])

#####################################################################
# VIA PadLock instructions
#####################################################################

add_group("padlock",
    cpu=["PadLock"],
    modifiers=["Imm8", "PreAdd", "Op1Add"],
    prefix=0x00,
    opcode=[0x0F, 0x00],
    operands=[])

add_insn("xstore", "padlock", modifiers=[0xC0, 0x00, 0xA7])
add_insn("xstorerng", "padlock", modifiers=[0xC0, 0x00, 0xA7])
add_insn("xcryptecb", "padlock", modifiers=[0xC8, 0xF3, 0xA7])
add_insn("xcryptcbc", "padlock", modifiers=[0xD0, 0xF3, 0xA7])
add_insn("xcryptctr", "padlock", modifiers=[0xD8, 0xF3, 0xA7])
add_insn("xcryptcfb", "padlock", modifiers=[0xE0, 0xF3, 0xA7])
add_insn("xcryptofb", "padlock", modifiers=[0xE8, 0xF3, 0xA7])
add_insn("montmul", "padlock", modifiers=[0xC0, 0xF3, 0xA6])
add_insn("xsha1", "padlock", modifiers=[0xC8, 0xF3, 0xA6])
add_insn("xsha256", "padlock", modifiers=[0xD0, 0xF3, 0xA6])

#####################################################################
# Cyrix MMX instructions
#####################################################################

add_group("cyrixmmx",
    cpu=["MMX", "Cyrix"],
    modifiers=["Op1Add"],
    opcode=[0x0F, 0x00],
    operands=[Operand(type="SIMDReg", size=64, dest="Spare"),
              Operand(type="SIMDRM", size=64, relaxed=True, dest="EA")])

add_insn("paddsiw", "cyrixmmx", modifiers=[0x51])
add_insn("paveb", "cyrixmmx", modifiers=[0x50])
add_insn("pdistib", "cyrixmmx", modifiers=[0x54])
add_insn("pmagw", "cyrixmmx", modifiers=[0x52])
add_insn("pmulhriw", "cyrixmmx", modifiers=[0x5D])
add_insn("pmulhrwc", "cyrixmmx", modifiers=[0x59])
add_insn("pmvgezb", "cyrixmmx", modifiers=[0x5C])
add_insn("pmvlzb", "cyrixmmx", modifiers=[0x5B])
add_insn("pmvnzb", "cyrixmmx", modifiers=[0x5A])
add_insn("pmvzb", "cyrixmmx", modifiers=[0x58])
add_insn("psubsiw", "cyrixmmx", modifiers=[0x55])

add_group("pmachriw",
    cpu=["MMX", "Cyrix"],
    opcode=[0x0F, 0x5E],
    operands=[Operand(type="SIMDReg", size=64, dest="Spare"),
              Operand(type="Mem", size=64, relaxed=True, dest="EA")])

add_insn("pmachriw", "pmachriw")

#####################################################################
# Cyrix extensions
#####################################################################

add_insn("smint", "twobyte", modifiers=[0x0F, 0x38], cpu=["686", "Cyrix"])
add_insn("smintold", "twobyte", modifiers=[0x0F, 0x7E], cpu=["486", "Cyrix", "Obs"])

add_group("rdwrshr",
    cpu=["Cyrix", "SMM", "686"],
    modifiers=["Op1Add"],
    opcode=[0x0F, 0x36],
    operands=[Operand(type="RM", size=32, relaxed=True, dest="EA")])

add_insn("rdshr", "rdwrshr", modifiers=[0x00])
add_insn("wrshr", "rdwrshr", modifiers=[0x01])

add_group("rsdc",
    cpu=["Cyrix", "SMM", "486"],
    opcode=[0x0F, 0x79],
    operands=[Operand(type="SegReg", size=16, relaxed=True, dest="Spare"),
              Operand(type="Mem", size=80, relaxed=True, dest="EA")])

add_insn("rsdc", "rsdc")

add_group("cyrixsmm",
    cpu=["Cyrix", "SMM", "486"],
    modifiers=["Op1Add"],
    opcode=[0x0F, 0x00],
    operands=[Operand(type="Mem", size=80, relaxed=True, dest="EA")])

add_insn("rsldt", "cyrixsmm", modifiers=[0x7B])
add_insn("rsts", "cyrixsmm", modifiers=[0x7D])
add_insn("svldt", "cyrixsmm", modifiers=[0x7A])
add_insn("svts", "cyrixsmm", modifiers=[0x7C])

add_group("svdc",
    cpu=["Cyrix", "SMM", "486"],
    opcode=[0x0F, 0x78],
    operands=[Operand(type="Mem", size=80, relaxed=True, dest="EA"),
              Operand(type="SegReg", size=16, relaxed=True, dest="Spare")])

add_insn("svdc", "svdc")

#####################################################################
# Obsolete/undocumented instructions
#####################################################################

add_insn("fsetpm", "twobyte", modifiers=[0xDB, 0xE4], cpu=["286", "FPU", "Obs"])
add_insn("loadall", "twobyte", modifiers=[0x0F, 0x07], cpu=["386", "Undoc"])
add_insn("loadall286", "twobyte", modifiers=[0x0F, 0x05], cpu=["286", "Undoc"])
add_insn("salc", "onebyte", modifiers=[0xD6], cpu=["Undoc", "Not64"])
add_insn("smi", "onebyte", modifiers=[0xF1], cpu=["386", "Undoc"])

add_group("ibts",
    cpu=["Undoc", "Obs", "386"],
    opersize=16,
    opcode=[0x0F, 0xA7],
    operands=[Operand(type="RM", size=16, relaxed=True, dest="EA"),
              Operand(type="Reg", size=16, dest="Spare")])
add_group("ibts",
    cpu=["Undoc", "Obs", "386"],
    opersize=32,
    opcode=[0x0F, 0xA7],
    operands=[Operand(type="RM", size=32, relaxed=True, dest="EA"),
              Operand(type="Reg", size=32, dest="Spare")])

add_insn("ibts", "ibts")

add_group("umov",
    cpu=["Undoc", "386"],
    opcode=[0x0F, 0x10],
    operands=[Operand(type="RM", size=8, relaxed=True, dest="EA"),
              Operand(type="Reg", size=8, dest="Spare")])
add_group("umov",
    cpu=["Undoc", "386"],
    opersize=16,
    opcode=[0x0F, 0x11],
    operands=[Operand(type="RM", size=16, relaxed=True, dest="EA"),
              Operand(type="Reg", size=16, dest="Spare")])
add_group("umov",
    cpu=["Undoc", "386"],
    opersize=32,
    opcode=[0x0F, 0x11],
    operands=[Operand(type="RM", size=32, relaxed=True, dest="EA"),
              Operand(type="Reg", size=32, dest="Spare")])
add_group("umov",
    cpu=["Undoc", "386"],
    opcode=[0x0F, 0x12],
    operands=[Operand(type="Reg", size=8, dest="Spare"),
              Operand(type="RM", size=8, relaxed=True, dest="EA")])
add_group("umov",
    cpu=["Undoc", "386"],
    opersize=16,
    opcode=[0x0F, 0x13],
    operands=[Operand(type="Reg", size=16, dest="Spare"),
              Operand(type="RM", size=16, relaxed=True, dest="EA")])
add_group("umov",
    cpu=["Undoc", "386"],
    opersize=32,
    opcode=[0x0F, 0x13],
    operands=[Operand(type="Reg", size=32, dest="Spare"),
              Operand(type="RM", size=32, relaxed=True, dest="EA")])

add_insn("umov", "umov")

add_group("xbts",
    cpu=["Undoc", "Obs", "386"],
    opersize=16,
    opcode=[0x0F, 0xA6],
    operands=[Operand(type="Reg", size=16, dest="Spare"),
              Operand(type="Mem", size=16, relaxed=True, dest="EA")])
add_group("xbts",
    cpu=["Undoc", "Obs", "386"],
    opersize=32,
    opcode=[0x0F, 0xA6],
    operands=[Operand(type="Reg", size=32, dest="Spare"),
              Operand(type="Mem", size=32, relaxed=True, dest="EA")])

add_insn("xbts", "xbts")

finalize_insns()

#####################################################################
# Prefixes
#####################################################################
# operand size overrides
for sz in [16, 32, 64]:
    add_prefix("o%d" % sz, "OPERSIZE", sz, parser="nasm", only64=(sz==64))
    add_prefix("data%d" % sz, "OPERSIZE", sz, parser="gas", only64=(sz==64))
add_prefix("word",      "OPERSIZE", 16, parser="gas")
add_prefix("dword",     "OPERSIZE", 32, parser="gas")
add_prefix("qword",     "OPERSIZE", 64, parser="gas", only64=True)

# address size overrides
for sz in [16, 32, 64]:
    add_prefix("a%d" % sz, "ADDRSIZE", sz, parser="nasm", only64=(sz==64))
    add_prefix("addr%d" % sz, "ADDRSIZE", sz, parser="gas", only64=(sz==64))
add_prefix("aword",     "ADDRSIZE", 16, parser="gas")
add_prefix("adword",    "ADDRSIZE", 32, parser="gas")
add_prefix("aqword",    "ADDRSIZE", 64, parser="gas", only64=True)

# instruction prefixes
add_prefix("lock",      "LOCKREP",  0xF0)
add_prefix("repne",     "LOCKREP",  0xF2)
add_prefix("repnz",     "LOCKREP",  0xF2)
add_prefix("rep",       "LOCKREP",  0xF3)
add_prefix("repe",      "LOCKREP",  0xF3)
add_prefix("repz",      "LOCKREP",  0xF3)

# other prefixes, limited to GAS-only at the moment
# Hint taken/not taken for jumps
add_prefix("ht",        "SEGREG",   0x3E, parser="gas")
add_prefix("hnt",       "SEGREG",   0x2E, parser="gas")

# REX byte explicit prefixes
for val, suf in enumerate(["", "z", "y", "yz", "x", "xz", "xy", "xyz"]):
    add_prefix("rex" + suf, "REX", 0x40+val, parser="gas", only64=True)
    add_prefix("rex64" + suf, "REX", 0x48+val, parser="gas", only64=True)

#####################################################################
# Output generation
#####################################################################

output_groups(file("x86insns.c", "wt"))
output_gas_insns(file("x86insn_gas.gperf", "wt"))
output_nasm_insns(file("x86insn_nasm.gperf", "wt"))
