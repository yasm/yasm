[bits 32]
mov ax,[eax+ebx+ecx-eax]
mov ax,[eax+ecx+ebx-eax]
label
dd 5
label2
mov ax,[eax+ebx*(label2-label)]
