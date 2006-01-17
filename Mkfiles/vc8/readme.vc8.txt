
Building YASM with Microsoft Visual Studio 2005
-----------------------------------------------

This note describes how to build YASM for AMD64 and 
EM64T systems using Microsoft Visual Studio 2005. 

1. The Compiler
---------------

If you want to build the 64-bit version of YASM you 
will need to install the Visual Studio .NET 64-bit 
native or cross compiler tools (these tools are not 
installed by default).

2. YASM Download
----------------

The following files are not built on VC++ and are not 
contained in the YASM subversion repository (but they 
are included in the nightly YASM snapshots):

    gas-bison.c
    gas-bison.h
    nasm-bison.c
    nasm-bison.h
    re2c-parser.c
    re2c-parser.h

If you wish to build from the latest files in the 
subversion repository, you will need to add these files 
from the latest snapshot to the repository files.

3. Building YASM with Microsoft VC8
-----------------------------------

First YASM needs to be downloaded and the files placed 
within a suitable directory, which will be called <yasm> 
here but can be named and located as you wish. If the 
Visual Studio 2005 project files have been obtained 
seperately the subdirectory 'vc8' and its subdirectories 
and files need to be placed in the 'Mkfiles' subdirectory 
within the YASM root directory.

If building from the subversion repository, obtain the 
additional files discussed above and place them in the 
YASM root directory.
   
Now locate and double click on the yasm.sln solution file in 
the 'Mkfiles/vc8' subdirectory to open the build project in 
the Visual Studio 2005 IDE and then select:

    win32 or x64 build
    release or debug build
    
as appropriate to build the YASM binaries that you need.  

4. Using YASM with Visual C++ version 8
---------------------------------------

1. Firstly you need to locate the directory (or directories) 
where the VC++ compiler binaries are located and put copies of 
the yasm.exe binary in these directories. 

On a win32 system you will use the win32 version of YASM. On 
an x64 system you will need to put the win32 YASM binary in the 
32-bit VC++ binary directory, which is typically located at:

    Program Files (x86)\Microsoft Visual Studio 8\VC\bin

and the 64-bit YASM binary in in the 64-bit tools binary
directory, which is typically:

    Program Files\Microsoft Visual Studio 8\VC\bin

2. To use the new custom tools facility in VC++ .NET 2005 you need 
to place copies of the yasm32.rules and yasm32.rules files in the 
vc8 directory in the VC++ 'VCProjectDefaults' directory that is 
typically:

  Program Files (x86)\Microsoft Visual Studio 8\VC\VCProjectDefaults

This allows you to configure YASM as an assembler within the VC++ 
IDE. To use YASM in a project, right click on the project in the 
Solution Explorer and select 'Custom Build Rules..'. This will give
you a dialog box that allows you to select YASM as a 32 or 64 bit 
assembler (your assembler files need to have the extension '.asm').

To assemble a file with YASM, select the Property Page for the file
and the select 'Yasm Assembler' in the Tool dialog entry and set the
appropriate properties.

5. A Linker Issue
-----------------

There appears to be a linker bug in the VC++ v8 linker that prevents 
symbols with absolute addresses being linked in DLL builds.  This
means, for example, that LEA instructions of the general form:

   lea, rax,[rax+symbol]
   
cannot be used for DLL builds.  The following general form has to be
used instead:

   lea rcx,[symbol wrt rip]
   lea rax,[rax+rcx]
   
This limitation may also cause problems with other instruction that 
use absolute addresses.

I am most grateful for the fantastic support that Peter Johnson, YASM's 
creator, has given me in tracking down this issue.

  Brian Gladman, 22nd December 2005
