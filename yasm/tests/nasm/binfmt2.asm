[extern blah]
data
db 4

mov ax, seg data

mov ax, data wrt 0

mov ax, blah

mov bx, [seg data]

mov bx, [data wrt 0]

resb 1

[section .bss]
db 5
