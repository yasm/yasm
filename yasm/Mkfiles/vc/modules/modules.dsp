# Microsoft Developer Studio Project File - Name="modules" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=modules - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "modules.mak".
!MESSAGE 
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

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "modules - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /I ".." /I "../../.." /D "NDEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "HAVE_CONFIG_H" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "modules - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ  /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /I ".." /I "../../.." /D "_DEBUG" /D "WIN32" /D "_MBCS" /D "_LIB" /D "HAVE_CONFIG_H" /YX /FD /GZ  /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "modules - Win32 Release"
# Name "modules - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Group "arch"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\modules\arch\x86\x86arch.c
# End Source File
# Begin Source File

SOURCE=..\..\..\modules\arch\x86\x86arch.h
# End Source File
# Begin Source File

SOURCE=..\..\..\modules\arch\x86\x86bc.c
# End Source File
# Begin Source File

SOURCE=..\..\..\modules\arch\x86\x86expr.c
# End Source File
# Begin Source File

SOURCE=..\..\..\x86id.c
# End Source File
# End Group
# Begin Group "dbgfmts"

# PROP Default_Filter ""
# Begin Source File

SOURCE="..\..\..\modules\dbgfmts\null\null-dbgfmt.c"
# End Source File
# End Group
# Begin Group "objfmts"

# PROP Default_Filter ""
# Begin Source File

SOURCE="..\..\..\modules\objfmts\bin\bin-objfmt.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\modules\objfmts\coff\coff-objfmt.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\modules\objfmts\dbg\dbg-objfmt.c"
# End Source File
# End Group
# Begin Group "optimizers"

# PROP Default_Filter ""
# Begin Source File

SOURCE="..\..\..\modules\optimizers\basic\basic-optimizer.c"
# End Source File
# End Group
# Begin Group "parsers"

# PROP Default_Filter ""
# Begin Source File

SOURCE="..\..\..\nasm-bison.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\nasm-bison.h"
# End Source File
# Begin Source File

SOURCE="..\..\..\modules\parsers\nasm\nasm-defs.h"
# End Source File
# Begin Source File

SOURCE="..\..\..\modules\parsers\nasm\nasm-parser.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\modules\parsers\nasm\nasm-parser.h"
# End Source File
# Begin Source File

SOURCE="..\..\..\nasm-token.c"
# End Source File
# End Group
# Begin Group "preprocs"

# PROP Default_Filter ""
# Begin Group "nasm"

# PROP Default_Filter ""
# Begin Source File

SOURCE="..\..\..\modules\preprocs\nasm\nasm-eval.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\modules\preprocs\nasm\nasm-eval.h"
# End Source File
# Begin Source File

SOURCE="..\..\..\nasm-macros.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\modules\preprocs\nasm\nasm-pp.c"
# End Source File
# Begin Source File

SOURCE="..\..\..\modules\preprocs\nasm\nasm-pp.h"
# End Source File
# Begin Source File

SOURCE="..\..\..\modules\preprocs\nasm\nasm-preproc.c"
# End Source File
# Begin Source File

SOURCE=..\..\..\modules\preprocs\nasm\nasm.h
# End Source File
# Begin Source File

SOURCE=..\..\..\modules\preprocs\nasm\nasmlib.c
# End Source File
# Begin Source File

SOURCE=..\..\..\modules\preprocs\nasm\nasmlib.h
# End Source File
# End Group
# Begin Group "raw"

# PROP Default_Filter ""
# Begin Source File

SOURCE="..\..\..\modules\preprocs\raw\raw-preproc.c"
# End Source File
# End Group
# End Group
# End Group
# End Target
# End Project
