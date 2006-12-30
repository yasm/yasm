
Building YASM with Microsoft Visual Studio 2005
-----------------------------------------------

This note describes how to build YASM using Microsoft Visual Studio 2005.

1. The Compiler
---------------

If you want to build the 64-bit version of YASM you will need to install
the Visual Studio 2005 64-bit tools, which are not installed by default.

2. YASM Download
----------------

First YASM needs to be downloaded and the files placed within a suitable
directory, which will be called <yasm> here but can be named and located
as you wish.

3. Building YASM with Microsoft VC8
-----------------------------------

Now locate and double click on the yasm.sln solution file in the 'Mkfiles/vc8'
subdirectory to open the build project in the Visual Studio 2005 IDE and then
select:

    win32 or x64 build
    release or debug build

as appropriate to build the YASM binaries that you need.

4. Using YASM with Visual Sudio 2005 and VC++ version 8
-------------------------------------------------------

1. Firstly you need to locate the directory (or directories) where the VC++
compiler binaries are located and put copies of the appropriate yasm.exe
binary in these directories.

On a win32 system you will use the win32 version of YASM. On an x64 system
you can use either the 32 or the 64 bit versions.  The win32 YASM binary
should be placed in the 32-bit VC++ binary directory, which is typically
located at:

 Program Files (x86)\Microsoft Visual Studio 8\VC\bin

If needed the 64-bit YASM binary should be places in the 64-bit tools
binary directory, which is typically at:

 Program Files\Microsoft Visual Studio 8\VC\bin

2. To use the new custom tools facility in Visual Studio 2005, you need to
place a copy of the yasm.rules file in the Visual Studio 2005 VC project
defaults directory, which is typically located at:

 Program Files (x86)\Microsoft Visual Studio 8\VC\VCProjectDefaults

This allows you to configure YASM as an assembler within the VC++ IDE. To
use YASM in a project, right click on the project in the Solution Explorer
and select 'Custom Build Rules..'. This will give you a dialog box that
allows you to select YASM as an assembler (note that your assembler files
need to have the extension '.asm').

To assemble a file with YASM, select the Property Page for the file and the
select 'Yasm Assembler' in the Tool dialog entry. Then click 'Apply' and an
additional property page entry will appear and enable YASM settings to be
established.

As alternative to placing the yasm.rules files as described above is to set
the rules file path in the Visual Studio 2005 settings dialogue.

It is also important to note that the rules file passes the symbols 'Win32'
or 'x64' to YASM by using the Visual Studio 2005  $(PlatformName) macro in
order to obtain either a 32 or a 64 bit assembler mode. This is a recent
enhancement to YASM so you will need to be sure that your YASM files are at
revision r1331 or higher to use this facility.

5. A Linker Issue
-----------------

There appears to be a linker bug in the VC++ v8 linker that prevents symbols
with absolute addresses being linked in DLL builds.  This means, for example,
that LEA instructions of the general form:

   lea, rax,[rax+symbol]

cannot be used for DLL builds.  The following general form has to be used
instead:

   lea rcx,[symbol wrt rip]
   lea rax,[rax+rcx]

This limitation may also cause problems with other instruction that use
absolute addresses.

I am most grateful for the fantastic support that Peter Johnson, YASM's
creator, has given me in tracking down this issue.

  Brian Gladman, 16th January 2006
