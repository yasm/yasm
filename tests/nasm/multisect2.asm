[section text]
aaa
aam
div ax
imul eax

[section .data]
db 'hello',0

[section .TEXT]
div al
bswap eax

[section .bss]
resb 25

[section .data]
db 'bye!',0

[section .TEXT]
mov ax, 5
