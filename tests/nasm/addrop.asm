[BITS 32]
db 0FFh
idiv al		; F6 F8
db 0FEh
idiv ax		; 66 F7 F8
db 0FDh
idiv eax	; F7 F8
db 0FCh
idiv byte [word 0]		; 67 F6 3E 00 00
db 0FBh
idiv byte [dword 0xFFFFFFFF]	; F6 3D FF FF FF FF
db 0FAh
idiv byte [0]			; F6 3D 00 00 00 00
db 0F9h
a16 idiv byte [word 0]		; 67 67 F6 3E 00 00
db 0F8h
a16 idiv byte [dword 0]		; 67 F6 3D 00 00 00 00
db 0F7h
a16 idiv byte [0]		; 67 F6 3D 00 00
db 0F6h
a32 idiv byte [0]		; F6 3D 00 00 00 00
[BITS 16]
nop
idiv al
idiv ax
idiv eax
nop
idiv byte [word 0]
idiv byte [dword 0xFFFFFFFF]
idiv byte [0]
idiv dword [es:dword 5]
idiv dword [byte es:5]
idiv word [es:dword edi+5]
idiv word [es:edi+dword 5]
nop
