# Microsoft Developer Studio Generated NMAKE File, Based on libyasm.dsp
!IF "$(CFG)" == ""
CFG=libyasm - Win32 Debug
!MESSAGE No configuration specified. Defaulting to libyasm - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "libyasm - Win32 Release" && "$(CFG)" != "libyasm - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "libyasm.mak" CFG="libyasm - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "libyasm - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "libyasm - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "libyasm - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

ALL : "$(OUTDIR)\libyasm.lib"


CLEAN :
	-@erase "$(INTDIR)\arch.obj"
	-@erase "$(INTDIR)\bitvect.obj"
	-@erase "$(INTDIR)\bytecode.obj"
	-@erase "$(INTDIR)\errwarn.obj"
	-@erase "$(INTDIR)\expr.obj"
	-@erase "$(INTDIR)\file.obj"
	-@erase "$(INTDIR)\floatnum.obj"
	-@erase "$(INTDIR)\hamt.obj"
	-@erase "$(INTDIR)\intnum.obj"
	-@erase "$(INTDIR)\linemgr.obj"
	-@erase "$(INTDIR)\mergesort.obj"
	-@erase "$(INTDIR)\section.obj"
	-@erase "$(INTDIR)\strcasecmp.obj"
	-@erase "$(INTDIR)\strsep.obj"
	-@erase "$(INTDIR)\symrec.obj"
	-@erase "$(INTDIR)\valparam.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\xmalloc.obj"
	-@erase "$(INTDIR)\xstrdup.obj"
	-@erase "$(OUTDIR)\libyasm.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /ML /W3 /GX /O2 /I ".." /I "../../.." /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\libyasm.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\libyasm.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\libyasm.lib" 
LIB32_OBJS= \
	"$(INTDIR)\arch.obj" \
	"$(INTDIR)\bitvect.obj" \
	"$(INTDIR)\bytecode.obj" \
	"$(INTDIR)\errwarn.obj" \
	"$(INTDIR)\expr.obj" \
	"$(INTDIR)\file.obj" \
	"$(INTDIR)\floatnum.obj" \
	"$(INTDIR)\hamt.obj" \
	"$(INTDIR)\intnum.obj" \
	"$(INTDIR)\linemgr.obj" \
	"$(INTDIR)\mergesort.obj" \
	"$(INTDIR)\section.obj" \
	"$(INTDIR)\strcasecmp.obj" \
	"$(INTDIR)\strsep.obj" \
	"$(INTDIR)\symrec.obj" \
	"$(INTDIR)\valparam.obj" \
	"$(INTDIR)\xmalloc.obj" \
	"$(INTDIR)\xstrdup.obj"

"$(OUTDIR)\libyasm.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

!ELSEIF  "$(CFG)" == "libyasm - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

ALL : "$(OUTDIR)\libyasm.lib"


CLEAN :
	-@erase "$(INTDIR)\arch.obj"
	-@erase "$(INTDIR)\bitvect.obj"
	-@erase "$(INTDIR)\bytecode.obj"
	-@erase "$(INTDIR)\errwarn.obj"
	-@erase "$(INTDIR)\expr.obj"
	-@erase "$(INTDIR)\file.obj"
	-@erase "$(INTDIR)\floatnum.obj"
	-@erase "$(INTDIR)\hamt.obj"
	-@erase "$(INTDIR)\intnum.obj"
	-@erase "$(INTDIR)\linemgr.obj"
	-@erase "$(INTDIR)\mergesort.obj"
	-@erase "$(INTDIR)\section.obj"
	-@erase "$(INTDIR)\strcasecmp.obj"
	-@erase "$(INTDIR)\strsep.obj"
	-@erase "$(INTDIR)\symrec.obj"
	-@erase "$(INTDIR)\valparam.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(INTDIR)\xmalloc.obj"
	-@erase "$(INTDIR)\xstrdup.obj"
	-@erase "$(OUTDIR)\libyasm.lib"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MLd /W3 /Gm /GX /ZI /Od /I ".." /I "../../.." /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\libyasm.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\libyasm.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"$(OUTDIR)\libyasm.lib" 
LIB32_OBJS= \
	"$(INTDIR)\arch.obj" \
	"$(INTDIR)\bitvect.obj" \
	"$(INTDIR)\bytecode.obj" \
	"$(INTDIR)\errwarn.obj" \
	"$(INTDIR)\expr.obj" \
	"$(INTDIR)\file.obj" \
	"$(INTDIR)\floatnum.obj" \
	"$(INTDIR)\hamt.obj" \
	"$(INTDIR)\intnum.obj" \
	"$(INTDIR)\linemgr.obj" \
	"$(INTDIR)\mergesort.obj" \
	"$(INTDIR)\section.obj" \
	"$(INTDIR)\strcasecmp.obj" \
	"$(INTDIR)\strsep.obj" \
	"$(INTDIR)\symrec.obj" \
	"$(INTDIR)\valparam.obj" \
	"$(INTDIR)\xmalloc.obj" \
	"$(INTDIR)\xstrdup.obj"

"$(OUTDIR)\libyasm.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

!ENDIF 


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("libyasm.dep")
!INCLUDE "libyasm.dep"
!ELSE 
!MESSAGE Warning: cannot find "libyasm.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "libyasm - Win32 Release" || "$(CFG)" == "libyasm - Win32 Debug"
SOURCE=..\..\..\libyasm\arch.c

"$(INTDIR)\arch.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\..\..\libyasm\bitvect.c

"$(INTDIR)\bitvect.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\..\..\libyasm\bytecode.c

"$(INTDIR)\bytecode.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\..\..\libyasm\errwarn.c

"$(INTDIR)\errwarn.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\..\..\libyasm\expr.c

"$(INTDIR)\expr.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\..\..\libyasm\file.c

"$(INTDIR)\file.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\..\..\libyasm\floatnum.c

"$(INTDIR)\floatnum.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\..\..\libyasm\hamt.c

"$(INTDIR)\hamt.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\..\..\libyasm\intnum.c

"$(INTDIR)\intnum.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\..\..\libyasm\linemgr.c

"$(INTDIR)\linemgr.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\..\..\libyasm\mergesort.c

"$(INTDIR)\mergesort.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\..\..\libyasm\section.c

"$(INTDIR)\section.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\..\..\libyasm\strcasecmp.c

"$(INTDIR)\strcasecmp.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\..\..\libyasm\strsep.c

"$(INTDIR)\strsep.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\..\..\libyasm\symrec.c

"$(INTDIR)\symrec.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\..\..\libyasm\valparam.c

"$(INTDIR)\valparam.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\..\..\libyasm\xmalloc.c

"$(INTDIR)\xmalloc.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\..\..\libyasm\xstrdup.c

"$(INTDIR)\xstrdup.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)



!ENDIF 

