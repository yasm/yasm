[BITS 16]

label:
jmp 1
jmp +1
jmp -1
jmp label
nop
je $$+1
je $+1
je $-1
je label
