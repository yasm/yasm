[bits 32]
[org 0x100]
je label
dd label
dw label
dd 3.14159
shl ax, 1
label:
mov byte [label2+ebx], 1
resb 1
[section .data align=16]
db 5
dd label2
[section .text]
mov dword [label2], 5
call label
je near label2
[section .bss]
label2:
resd 1
dd 1
mov cx, 5
[global label2]
[extern label3]
[section .data]
times 65536 db 0
