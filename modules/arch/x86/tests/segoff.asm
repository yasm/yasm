; all of these should be legal and should just result in the offset portion

jmp far[1:2]
mov ax,[1:2]
push dword [1:2]
mov ax,1:2

foo equ 1:2
jmp far[foo]
mov ax,[foo]
push dword [foo]
mov ax,foo
