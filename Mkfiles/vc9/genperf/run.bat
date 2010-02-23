cd ..\..\..
@echo off
for /f "usebackq tokens=2,3,4,5,6" %%f in (`reg query HKCR\Python.File\shell\open\command`) do (if %%f==REG_SZ echo %%g %%h %%i %%j >run_py.bat)
goto ftry%errorlevel%

:ftry0
goto pyfound
:ftry1
for /f "usebackq tokens=2,3,4" %%f in (`reg query HKCR\Python.File\shell\open\command`) do (if %%f==REG_SZ echo %%g %%h >run_py.bat) 
goto stry%errorlevel%

:stry0
goto pyfound
:stry1
goto pynotfound

:pyfound
if not exist run_py.bat goto notfound
echo ... building with Python ...
@echo on
call run_py.bat modules\arch\x86\gen_x86_insn.py
del run_py.bat
@echo off
goto end

:pynotfound
echo ... building without Python ...

:end
@echo on
%1 x86insn_nasm.gperf x86insn_nasm.c
%1 x86insn_gas.gperf x86insn_gas.c
%1 modules\arch\x86\x86cpu.gperf x86cpu.c
%1 modules\arch\x86\x86regtmod.gperf x86regtmod.c
