# Microsoft Developer Studio Generated NMAKE File, Based on modules.dsp
!IF "$(CFG)" == ""
CFG=modules - Win32 Debug
!MESSAGE No configuration specified. Defaulting to modules - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "modules - Win32 Release" && "$(CFG)" != "modules - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "modules.mak" CFG="modules - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "modules - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "modules - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "modules - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\modules.lib"

!ELSE 

ALL : "libyasm - Win32 Release" "$(OUTDIR)\modules.lib"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"libyasm - Win32 ReleaseCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\basic-optimizer.obj"
	-@erase "$(INTDIR)\bin-objfmt.obj"
	-@erase "$(INTDIR)\coff-objfmt.obj"
	-@erase "$(INTDIR)\dbg-objfmt.obj"
	-@erase "$(INTDIR)\elf-objfmt.obj"
	-@erase "$(INTDIR)\elf.obj"
	-@erase "$(INTDIR)\lc3barch.obj"
	-@erase "$(INTDIR)\lc3bbc.obj"
	-@erase "$(INTDIR)\lc3bid.obj"
	-@erase "$(INTDIR)\nasm-bison.obj"
	-@erase "$(INTDIR)\nasm-eval.obj"
	-@erase "$(INTDIR)\nasm-macros.obj"
	-@erase "$(INTDIR)\nasm-parser.obj"
	-@erase "$(INTDIR)\nasm-pp.obj"
	-@erase "$(INTDIR)\nasm-preproc.obj"
	-@erase "$(INTDIR)\nasm-token.obj"
	-@erase "$(INTDIR)\nasmlib.obj"
	-@erase "$(INTDIR)\null-dbgfmt.obj"
	-@erase "$(INTDIR)\raw-preproc.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\x86arch.obj"
	-@erase "$(INTDIR)\x86bc.obj"
	-@erase "$(INTDIR)\x86expr.obj"
	-@erase "$(INTDIR)\x86id.obj"
	-@erase "$(OUTDIR)\modules.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /ML /W3 /GX /O2 /I ".." /I "../../.." /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\modules.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

RSC=rc.exe
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\modules.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\modules.lib" 
LIB32_OBJS= \
	"$(INTDIR)\x86arch.obj" \
	"$(INTDIR)\x86bc.obj" \
	"$(INTDIR)\x86expr.obj" \
	"$(INTDIR)\x86id.obj" \
	"$(INTDIR)\null-dbgfmt.obj" \
	"$(INTDIR)\bin-objfmt.obj" \
	"$(INTDIR)\coff-objfmt.obj" \
	"$(INTDIR)\dbg-objfmt.obj" \
	"$(INTDIR)\basic-optimizer.obj" \
	"$(INTDIR)\nasm-bison.obj" \
	"$(INTDIR)\nasm-parser.obj" \
	"$(INTDIR)\nasm-token.obj" \
	"$(INTDIR)\nasm-eval.obj" \
	"$(INTDIR)\nasm-macros.obj" \
	"$(INTDIR)\nasm-pp.obj" \
	"$(INTDIR)\nasm-preproc.obj" \
	"$(INTDIR)\nasmlib.obj" \
	"$(INTDIR)\raw-preproc.obj" \
	"$(INTDIR)\lc3bbc.obj" \
	"$(INTDIR)\lc3barch.obj" \
	"$(INTDIR)\lc3bid.obj" \
	"$(INTDIR)\elf-objfmt.obj" \
	"$(INTDIR)\elf.obj" \
	"..\libyasm\Release\libyasm.lib"

"$(OUTDIR)\modules.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

!ELSEIF  "$(CFG)" == "modules - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\modules.lib"

!ELSE 

ALL : "libyasm - Win32 Debug" "$(OUTDIR)\modules.lib"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"libyasm - Win32 DebugCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\basic-optimizer.obj"
	-@erase "$(INTDIR)\bin-objfmt.obj"
	-@erase "$(INTDIR)\coff-objfmt.obj"
	-@erase "$(INTDIR)\dbg-objfmt.obj"
	-@erase "$(INTDIR)\elf-objfmt.obj"
	-@erase "$(INTDIR)\elf.obj"
	-@erase "$(INTDIR)\lc3barch.obj"
	-@erase "$(INTDIR)\lc3bbc.obj"
	-@erase "$(INTDIR)\lc3bid.obj"
	-@erase "$(INTDIR)\nasm-bison.obj"
	-@erase "$(INTDIR)\nasm-eval.obj"
	-@erase "$(INTDIR)\nasm-macros.obj"
	-@erase "$(INTDIR)\nasm-parser.obj"
	-@erase "$(INTDIR)\nasm-pp.obj"
	-@erase "$(INTDIR)\nasm-preproc.obj"
	-@erase "$(INTDIR)\nasm-token.obj"
	-@erase "$(INTDIR)\nasmlib.obj"
	-@erase "$(INTDIR)\null-dbgfmt.obj"
	-@erase "$(INTDIR)\raw-preproc.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(INTDIR)\x86arch.obj"
	-@erase "$(INTDIR)\x86bc.obj"
	-@erase "$(INTDIR)\x86expr.obj"
	-@erase "$(INTDIR)\x86id.obj"
	-@erase "$(OUTDIR)\modules.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MLd /W3 /Gm /GX /ZI /Od /I ".." /I "../../.." /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\modules.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

RSC=rc.exe
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\modules.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\modules.lib" 
LIB32_OBJS= \
	"$(INTDIR)\x86arch.obj" \
	"$(INTDIR)\x86bc.obj" \
	"$(INTDIR)\x86expr.obj" \
	"$(INTDIR)\x86id.obj" \
	"$(INTDIR)\null-dbgfmt.obj" \
	"$(INTDIR)\bin-objfmt.obj" \
	"$(INTDIR)\coff-objfmt.obj" \
	"$(INTDIR)\dbg-objfmt.obj" \
	"$(INTDIR)\basic-optimizer.obj" \
	"$(INTDIR)\nasm-bison.obj" \
	"$(INTDIR)\nasm-parser.obj" \
	"$(INTDIR)\nasm-token.obj" \
	"$(INTDIR)\nasm-eval.obj" \
	"$(INTDIR)\nasm-macros.obj" \
	"$(INTDIR)\nasm-pp.obj" \
	"$(INTDIR)\nasm-preproc.obj" \
	"$(INTDIR)\nasmlib.obj" \
	"$(INTDIR)\raw-preproc.obj" \
	"$(INTDIR)\lc3bbc.obj" \
	"$(INTDIR)\lc3barch.obj" \
	"$(INTDIR)\lc3bid.obj" \
	"$(INTDIR)\elf-objfmt.obj" \
	"$(INTDIR)\elf.obj" \
	"..\libyasm\Debug\libyasm.lib"

"$(OUTDIR)\modules.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

!ENDIF 


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("modules.dep")
!INCLUDE "modules.dep"
!ELSE 
!MESSAGE Warning: cannot find "modules.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "modules - Win32 Release" || "$(CFG)" == "modules - Win32 Debug"
SOURCE=..\..\..\modules\arch\lc3b\lc3barch.c

"$(INTDIR)\lc3barch.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\..\..\modules\arch\lc3b\lc3bbc.c

"$(INTDIR)\lc3bbc.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\..\..\lc3bid.c

"$(INTDIR)\lc3bid.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\..\..\modules\arch\x86\x86arch.c

"$(INTDIR)\x86arch.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\..\..\modules\arch\x86\x86bc.c

"$(INTDIR)\x86bc.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\..\..\modules\arch\x86\x86expr.c

"$(INTDIR)\x86expr.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\..\..\x86id.c

"$(INTDIR)\x86id.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\..\..\modules\dbgfmts\null\null-dbgfmt.c"

"$(INTDIR)\null-dbgfmt.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\..\..\modules\objfmts\bin\bin-objfmt.c"

"$(INTDIR)\bin-objfmt.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\..\..\modules\objfmts\coff\coff-objfmt.c"

"$(INTDIR)\coff-objfmt.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\..\..\modules\objfmts\dbg\dbg-objfmt.c"

"$(INTDIR)\dbg-objfmt.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\..\..\modules\objfmts\elf\elf-objfmt.c"

"$(INTDIR)\elf-objfmt.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\..\..\modules\objfmts\elf\elf.c

"$(INTDIR)\elf.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\..\..\modules\optimizers\basic\basic-optimizer.c"

"$(INTDIR)\basic-optimizer.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\..\..\nasm-bison.c"

"$(INTDIR)\nasm-bison.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\..\..\modules\parsers\nasm\nasm-parser.c"

"$(INTDIR)\nasm-parser.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\..\..\nasm-token.c"

"$(INTDIR)\nasm-token.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\..\..\modules\preprocs\nasm\nasm-eval.c"

"$(INTDIR)\nasm-eval.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\..\..\nasm-macros.c"

"$(INTDIR)\nasm-macros.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\..\..\modules\preprocs\nasm\nasm-pp.c"

"$(INTDIR)\nasm-pp.obj" : $(SOURCE) "$(INTDIR)" "..\..\..\nasm-macros.c"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\..\..\modules\preprocs\nasm\nasm-preproc.c"

"$(INTDIR)\nasm-preproc.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\..\..\modules\preprocs\nasm\nasmlib.c

"$(INTDIR)\nasmlib.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE="..\..\..\modules\preprocs\raw\raw-preproc.c"

"$(INTDIR)\raw-preproc.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!IF  "$(CFG)" == "modules - Win32 Release"

"libyasm - Win32 Release" : 
   cd "\cygwin\home\Pete\yasm\yasm\Mkfiles\vc\libyasm"
   $(MAKE) /$(MAKEFLAGS) /F .\libyasm.mak CFG="libyasm - Win32 Release" 
   cd "..\modules"

"libyasm - Win32 ReleaseCLEAN" : 
   cd "\cygwin\home\Pete\yasm\yasm\Mkfiles\vc\libyasm"
   $(MAKE) /$(MAKEFLAGS) /F .\libyasm.mak CFG="libyasm - Win32 Release" RECURSE=1 CLEAN 
   cd "..\modules"

!ELSEIF  "$(CFG)" == "modules - Win32 Debug"

"libyasm - Win32 Debug" : 
   cd "\cygwin\home\Pete\yasm\yasm\Mkfiles\vc\libyasm"
   $(MAKE) /$(MAKEFLAGS) /F .\libyasm.mak CFG="libyasm - Win32 Debug" 
   cd "..\modules"

"libyasm - Win32 DebugCLEAN" : 
   cd "\cygwin\home\Pete\yasm\yasm\Mkfiles\vc\libyasm"
   $(MAKE) /$(MAKEFLAGS) /F .\libyasm.mak CFG="libyasm - Win32 Debug" RECURSE=1 CLEAN 
   cd "..\modules"

!ENDIF 


!ENDIF 

