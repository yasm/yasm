@echo off
cd ..\..\..
echo "" >temp.txt
for /R modules %%n in (*.c) do find "_LTX_" %%n | find "_module" | find "= {" >>temp.txt
type temp.txt | %1 libyasm\module.in
del temp.txt
