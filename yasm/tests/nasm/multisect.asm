[section .text]
aaa
aam
div ax
imul eax

[section .data]
db 'hello',0

[section .text]
div al
bswap eax

[section .bss]
resb 25

[section .data]
db 'bye!',0

[section .text]
mov ax, 5
