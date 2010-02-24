cd ..\..\..
@echo off
del run_py.bat
for /f "usebackq tokens=2*" %%f in (`reg query HKCR\Python.File\shell\open\command`) do (if %%f==REG_SZ echo %%g >run_py.bat) 
@echo on
if exist run_py.bat goto pyfound
echo ... building without Python ...
goto end

:pyfound
echo ... building with Python ...
call run_py.bat modules\arch\x86\gen_x86_insn.py
del run_py.bat

:end
%1 x86insn_nasm.gperf x86insn_nasm.c
%1 x86insn_gas.gperf x86insn_gas.c
%1 modules\arch\x86\x86cpu.gperf x86cpu.c
%1 modules\arch\x86\x86regtmod.gperf x86regtmod.c
