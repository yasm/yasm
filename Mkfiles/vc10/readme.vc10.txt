Building YASM with Microsoft Visual Studio 2010 (C/C++ v10)
-----------------------------------------------------------

This note describes how to build YASM using Microsoft Visual
Studio 2010 (C/C++ v10 - currently releasxed as beta 2).  

1. The Compiler
---------------

If you want to build the 64-bit version of YASM you will need 
to install the Visual Studio 2010 64-bit tools, which may not 
be installed by default.  If using Visual C++ Express 2010, 
you will need to install the Windows SDK to obtain the 64-bit
build tools. 

2. YASM Download
----------------

First YASM needs to be downloaded and the files placed within 
a suitable directory, which will be called <yasm> here but can 
be named and located as you wish.

3. Building YASM with Microsoft 2010 (VC10)
-------------------------------------------

Now locate and double click on the yasm.sln solution file in 
the 'Mkfiles/vc10' subdirectory to open the build project in 
the Visual Studio 2010 IDE and then select:

    win32 or x64 build
    release or debug build

as appropriate to build the YASM binaries that you need.

4. Using YASM with Visual Sudio 2010 and VC++ version 10
--------------------------------------------------------

1. Firstly you need to locate the directory (or directories) 
where the VC++ compiler binaries are located and put copies 
of the appropriate yasm.exe binary in these directories. A
typical location is:

C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\bin

Depending on your system you can use either the win32 or the
x64 version of YASM.  It must be named yasm.exe.

2. To use the new custom tools facility in Visual Studio 2010,
you need to place a copy of three files - yasm.props, yasm.targets 
and yasm.xml - into the MSBUILD customisation directory, which is
typically at:

C:\Program Files (x86)\MSBuild\Microsoft.Cpp\v4.0\BuildCustomizations

This allows you to configure YASM as an assembler within the VC++
IDE. To use YASM in a project, right click on the project in the
Solution Explorer and select 'Build Customisations..'. This will 
give you a dialog box that allows you to select YASM as an 
assembler (note that your assembler files need to have the 
extension '.asm').

To assemble a file with YASM, select the Property Page for the 
file and the select 'Yasm Assembler' in the Tool dialog entry. 
Then click 'Apply' and an additional property page entry will 
appear and enable YASM settings to be established.

As alternative to placing the yasm.rules files as described 
above is to set the rules file path in the Visual Studio 2010
settings dialogue.

5. A Linker Issue
-----------------

There appears to be a linker bug in the VC++ linker that 
prevents symbols with absolute addresses being linked in DLL 
builds.  This means, for example, that LEA instructions of 
the general form:

   lea, rax,[rax+symbol]

cannot be used for DLL builds.  The following general form 
has to be used instead:

   lea rcx,[symbol wrt rip]
   lea rax,[rax+rcx]

This limitation may also cause problems with other instruction 
that use absolute addresses.

6. Acknowledgements
-------------------

I am most grateful for the fantastic support that Peter Johnson,
YASM's creator, has given me in tracking down issues in using
YASM for the production of Windows x64 code.

  Brian Gladman, 10th November 2009

