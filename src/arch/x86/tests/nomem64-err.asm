[bits 64]
mov bh, r8b
mov r8b, ch
push es
pop fs
pushaw
popa
lds eax, [5]
aas
das
aad
into
salc
[bits 32]
xmm9:
rax:
rdx:
cdqe
swapgs
