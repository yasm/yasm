# Microsoft Developer Studio Project File - Name="libyasm" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=libyasm - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "libyasm.mak".
!MESSAGE 
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

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "libyasm - Win32 Release"

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

!ELSEIF  "$(CFG)" == "libyasm - Win32 Debug"

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

# Name "libyasm - Win32 Release"
# Name "libyasm - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\..\..\libyasm\arch.c
# End Source File
# Begin Source File

SOURCE=..\..\..\libyasm\bitvect.c
# End Source File
# Begin Source File

SOURCE=..\..\..\libyasm\bytecode.c
# End Source File
# Begin Source File

SOURCE=..\..\..\libyasm\errwarn.c
# End Source File
# Begin Source File

SOURCE=..\..\..\libyasm\expr.c
# End Source File
# Begin Source File

SOURCE=..\..\..\libyasm\file.c
# End Source File
# Begin Source File

SOURCE=..\..\..\libyasm\floatnum.c
# End Source File
# Begin Source File

SOURCE=..\..\..\libyasm\hamt.c
# End Source File
# Begin Source File

SOURCE=..\..\..\libyasm\intnum.c
# End Source File
# Begin Source File

SOURCE=..\..\..\libyasm\linemgr.c
# End Source File
# Begin Source File

SOURCE=..\..\..\libyasm\mergesort.c
# End Source File
# Begin Source File

SOURCE=..\..\..\libyasm\section.c
# End Source File
# Begin Source File

SOURCE=..\..\..\libyasm\strcasecmp.c
# End Source File
# Begin Source File

SOURCE=..\..\..\libyasm\strsep.c
# End Source File
# Begin Source File

SOURCE=..\..\..\libyasm\symrec.c
# End Source File
# Begin Source File

SOURCE=..\..\..\libyasm\valparam.c
# End Source File
# Begin Source File

SOURCE=..\..\..\libyasm\xmalloc.c
# End Source File
# Begin Source File

SOURCE=..\..\..\libyasm\xstrdup.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\..\libyasm\arch.h
# End Source File
# Begin Source File

SOURCE="..\..\..\libyasm\bc-int.h"
# End Source File
# Begin Source File

SOURCE=..\..\..\libyasm\bitvect.h
# End Source File
# Begin Source File

SOURCE=..\..\..\libyasm\bytecode.h
# End Source File
# Begin Source File

SOURCE="..\..\..\libyasm\compat-queue.h"
# End Source File
# Begin Source File

SOURCE=.\config.h
# End Source File
# Begin Source File

SOURCE=..\..\..\libyasm\coretype.h
# End Source File
# Begin Source File

SOURCE=..\..\..\libyasm\dbgfmt.h
# End Source File
# Begin Source File

SOURCE=..\..\..\libyasm\errwarn.h
# End Source File
# Begin Source File

SOURCE="..\..\..\libyasm\expr-int.h"
# End Source File
# Begin Source File

SOURCE=..\..\..\libyasm\expr.h
# End Source File
# Begin Source File

SOURCE=..\..\..\libyasm\file.h
# End Source File
# Begin Source File

SOURCE=..\..\..\libyasm\floatnum.h
# End Source File
# Begin Source File

SOURCE=..\..\..\libyasm\hamt.h
# End Source File
# Begin Source File

SOURCE=..\..\..\libyasm\intnum.h
# End Source File
# Begin Source File

SOURCE=..\..\..\libyasm\linemgr.h
# End Source File
# Begin Source File

SOURCE=..\..\..\libyasm\objfmt.h
# End Source File
# Begin Source File

SOURCE=..\..\..\libyasm\optimizer.h
# End Source File
# Begin Source File

SOURCE=..\..\..\libyasm\parser.h
# End Source File
# Begin Source File

SOURCE=..\..\..\libyasm\preproc.h
# End Source File
# Begin Source File

SOURCE=..\..\..\libyasm\section.h
# End Source File
# Begin Source File

SOURCE=..\..\..\libyasm\symrec.h
# End Source File
# Begin Source File

SOURCE=..\..\..\libyasm\util.h
# End Source File
# Begin Source File

SOURCE=..\..\..\libyasm\valparam.h
# End Source File
# End Group
# End Target
# End Project
