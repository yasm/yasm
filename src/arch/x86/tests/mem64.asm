[bits 64]
mov ax, [0]			; 66 A1 00 00 00 00 00 00 00 00
mov rax, [qword 0]		; 48 A1 00 00 00 00 00 00 00 00
mov rax, [dword 0]		; 67 48 A1 00 00 00 00
mov al, [0xfedcba9876543210]	; 48 A0 10 32 54 76 98 BA DC FE
a32 mov rax, [0]		; 67 48 A1 00 00 00 00
a32 mov eax, [0]		; 67 A1 00 00 00 00
mov ecx, [0]			; 8B 0C 25 00 00 00 00
mov edx, [dword 0]		; 8B 14 25 00 00 00 00
a32 mov rbx, [0]		; 67 48 8B 1C 25 00 00 00 00
mov ebx, [rcx]			; 8B 19
mov r8, [r9]			; 4D 8B 01
mov ecx, [ebx]			; 67 8B 0B
mov edx, [rip]			; 8B 15
a32 mov rcx, [rip+5]		; 67 48 8B 4D 05
mov rbx, [rax+rbx*4]		; 48 8B 1C 98
mov rdx, [rsp]			; 48 8B 14 24
mov rax, [r12]			; 49 8B 04 24
mov rcx, [rbp]			; 48 8B 4D 00
mov rbx, [r13]			; 49 8B 5D 00
mov ah, [rip]			; 8A 25
mov bh, [rcx]			; 8A 39