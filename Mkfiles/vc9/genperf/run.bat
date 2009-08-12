cd ..\..\..
@echo off
for /f "usebackq tokens=2,3,4" %%f in (`reg query HKCR\Python.File\shell\open\command`) do (if %%f==REG_SZ echo %%g %%h >run_py.bat)
goto answer%errorlevel%
:answer0
echo ... building with Python ...
@echo on
call run_py.bat modules\arch\x86\gen_x86_insn.py
del run_py.bat
@echo off
goto end
:answer1
echo ... building without Python ...
goto end
:end
@echo on
%1 x86insn_nasm.gperf x86insn_nasm.c
%1 x86insn_gas.gperf x86insn_gas.c
%1 modules\arch\x86\x86cpu.gperf x86cpu.c
%1 modules\arch\x86\x86regtmod.gperf x86regtmod.c
