# Microsoft Developer Studio Generated NMAKE File, Based on yasm.dsp
!IF "$(CFG)" == ""
CFG=yasm - Win32 Debug
!MESSAGE No configuration specified. Defaulting to yasm - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "yasm - Win32 Release" && "$(CFG)" != "yasm - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "yasm.mak" CFG="yasm - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "yasm - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "yasm - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "yasm - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\yasm.exe"

!ELSE 

ALL : "modules - Win32 Release" "libyasm - Win32 Release" "$(OUTDIR)\yasm.exe"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"libyasm - Win32 ReleaseCLEAN" "modules - Win32 ReleaseCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\yasm-module.obj"
	-@erase "$(INTDIR)\yasm-options.obj"
	-@erase "$(INTDIR)\yasm.obj"
	-@erase "$(OUTDIR)\yasm.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /ML /W3 /GX /O2 /I "." /I "../.." /D "NDEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\yasm.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\yasm.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /incremental:no /pdb:"$(OUTDIR)\yasm.pdb" /machine:I386 /out:"$(OUTDIR)\yasm.exe" 
LINK32_OBJS= \
	"$(INTDIR)\yasm-module.obj" \
	"$(INTDIR)\yasm-options.obj" \
	"$(INTDIR)\yasm.obj" \
	".\libyasm\Release\libyasm.lib" \
	".\modules\Release\modules.lib"

"$(OUTDIR)\yasm.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "yasm - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

!IF "$(RECURSE)" == "0" 

ALL : "$(OUTDIR)\yasm.exe"

!ELSE 

ALL : "modules - Win32 Debug" "libyasm - Win32 Debug" "$(OUTDIR)\yasm.exe"

!ENDIF 

!IF "$(RECURSE)" == "1" 
CLEAN :"libyasm - Win32 DebugCLEAN" "modules - Win32 DebugCLEAN" 
!ELSE 
CLEAN :
!ENDIF 
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(INTDIR)\yasm-module.obj"
	-@erase "$(INTDIR)\yasm-options.obj"
	-@erase "$(INTDIR)\yasm.obj"
	-@erase "$(OUTDIR)\yasm.exe"
	-@erase "$(OUTDIR)\yasm.ilk"
	-@erase "$(OUTDIR)\yasm.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MLd /W3 /Gm /GX /ZI /Od /I "." /I "../.." /D "_DEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\yasm.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

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
BSC32_FLAGS=/nologo /o"$(OUTDIR)\yasm.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /incremental:yes /pdb:"$(OUTDIR)\yasm.pdb" /debug /machine:I386 /out:"$(OUTDIR)\yasm.exe" /pdbtype:sept 
LINK32_OBJS= \
	"$(INTDIR)\yasm-module.obj" \
	"$(INTDIR)\yasm-options.obj" \
	"$(INTDIR)\yasm.obj" \
	".\libyasm\Debug\libyasm.lib" \
	".\modules\Debug\modules.lib"

"$(OUTDIR)\yasm.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("yasm.dep")
!INCLUDE "yasm.dep"
!ELSE 
!MESSAGE Warning: cannot find "yasm.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "yasm - Win32 Release" || "$(CFG)" == "yasm - Win32 Debug"
SOURCE=".\yasm-module.c"

!IF  "$(CFG)" == "yasm - Win32 Release"

CPP_SWITCHES=/nologo /ML /W3 /GX /O2 /I "." /I "../.." /I "../../frontends/yasm" /D "NDEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\yasm.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\yasm-module.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "yasm - Win32 Debug"

CPP_SWITCHES=/nologo /MLd /W3 /Gm /GX /ZI /Od /I "." /I "../.." /I "../../frontends/yasm" /D "_DEBUG" /D "WIN32" /D "_CONSOLE" /D "_MBCS" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\yasm.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

"$(INTDIR)\yasm-module.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE="..\..\frontends\yasm\yasm-options.c"

"$(INTDIR)\yasm-options.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\..\frontends\yasm\yasm.c

"$(INTDIR)\yasm.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!IF  "$(CFG)" == "yasm - Win32 Release"

"libyasm - Win32 Release" : 
   cd ".\libyasm"
   $(MAKE) /$(MAKEFLAGS) /F .\libyasm.mak CFG="libyasm - Win32 Release" 
   cd ".."

"libyasm - Win32 ReleaseCLEAN" : 
   cd ".\libyasm"
   $(MAKE) /$(MAKEFLAGS) /F .\libyasm.mak CFG="libyasm - Win32 Release" RECURSE=1 CLEAN 
   cd ".."

!ELSEIF  "$(CFG)" == "yasm - Win32 Debug"

"libyasm - Win32 Debug" : 
   cd ".\libyasm"
   $(MAKE) /$(MAKEFLAGS) /F .\libyasm.mak CFG="libyasm - Win32 Debug" 
   cd ".."

"libyasm - Win32 DebugCLEAN" : 
   cd ".\libyasm"
   $(MAKE) /$(MAKEFLAGS) /F .\libyasm.mak CFG="libyasm - Win32 Debug" RECURSE=1 CLEAN 
   cd ".."

!ENDIF 

!IF  "$(CFG)" == "yasm - Win32 Release"

"modules - Win32 Release" : 
   cd ".\modules"
   $(MAKE) /$(MAKEFLAGS) /F .\modules.mak CFG="modules - Win32 Release" 
   cd ".."

"modules - Win32 ReleaseCLEAN" : 
   cd ".\modules"
   $(MAKE) /$(MAKEFLAGS) /F .\modules.mak CFG="modules - Win32 Release" RECURSE=1 CLEAN 
   cd ".."

!ELSEIF  "$(CFG)" == "yasm - Win32 Debug"

"modules - Win32 Debug" : 
   cd ".\modules"
   $(MAKE) /$(MAKEFLAGS) /F .\modules.mak CFG="modules - Win32 Debug" 
   cd ".."

"modules - Win32 DebugCLEAN" : 
   cd ".\modules"
   $(MAKE) /$(MAKEFLAGS) /F .\modules.mak CFG="modules - Win32 Debug" RECURSE=1 CLEAN 
   cd ".."

!ENDIF 


!ENDIF 

