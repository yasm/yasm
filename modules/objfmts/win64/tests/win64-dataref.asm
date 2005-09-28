BITS 64

global		x86ident
global		__savident
extern		foobar		; :proc
extern		foobar2		; :abs
extern		foobar3		; :qword
extern		foobar4		; :byte

[SECTION .data]
__savident	dd 0              
savidentptr	dd __savident
savidentptr2	dq __savident
x86identptr	dd x86ident
x86identptr2	dq x86ident
foobarptr	dd foobar
foobarptr2	dq foobar
foobar2ptr	dd foobar2
foobar2ptr2	dq foobar2
foobar3ptr	dd foobar3
foobar3ptr2	dq foobar3
xptr		dd x
xptr2		dq x

[SECTION .bss]
x		resq	1

[SECTION .text]
x86ident:
		; with :proc
		mov	ebx, foobar		; WTF ML64.. this had []
		mov	rcx, foobar
		lea	rdx, [foobar wrt rip]
		mov	rax, [foobar+rcx]
		mov	rax, foobar
		mov	rbx, foobar
		movzx	rax, byte [foobar wrt rip]
		movzx	rax, byte [foobar+rax]

		; with :abs
		;mov	ebx,[foobar2]
		;mov	rcx,offset foobar2
		;lea	rdx, foobar2
		;mov	rax, qword ptr foobar2[rcx]
		;mov	rax, foobar2
		;mov	rbx, foobar2
		;movzx	rax, byte ptr foobar2
		;movzx	rax, byte ptr foobar2[rax]

		; with :qword
		mov	ebx, [foobar3 wrt rip]
		mov	rcx, foobar3
		lea	rdx, [foobar3 wrt rip]
		mov	rax, [foobar3+rcx]
		mov	rax, [foobar3 wrt rip]
		mov	rbx, [foobar3 wrt rip]
		movzx	rax, byte [foobar3 wrt rip]
		movzx	rax, byte [foobar3+rax]

		; local var (dword)
		mov	ebx,[__savident wrt rip]
		mov	rcx, __savident
		lea	rdx, [__savident wrt rip]
		mov	rax, [__savident+rcx]
		mov	rax, [__savident wrt rip]
		mov	rbx, [__savident wrt rip]
		movzx	rax, byte [__savident wrt rip]
		movzx	rax, byte [__savident+rax]

		; local var (qword)
		mov	ebx, [savidentptr2 wrt rip]
		mov	rcx, savidentptr2
		lea	rdx, [savidentptr2 wrt rip]
		mov	rax, [savidentptr2+rcx]
		mov	rax, [savidentptr2 wrt rip]
		mov	rbx, [savidentptr2 wrt rip]
		movzx	rax, byte [savidentptr2 wrt rip]
		movzx	rax, byte [savidentptr2+rax]

		call	foobar

		ret

trap:		sub	rsp, 256
		int3
		add	rsp, 256
.end

[SECTION .pdata]
dd	trap
dd	trap.end
dd	$xdatasym

[SECTION .xdata]
$xdatasym:
db	1, 7, 2, 0, 7, 1, 0x20, 0

[SECTION _FOO]
foo_foobar3ptr	dd foobar3
foo_foobar3ptr2	dq foobar3
		mov	ebx, [foobar3 wrt rip]
		mov	rcx, foobar3
		lea	rdx, [foobar3 wrt rip]
		mov	rax, [foobar3+rcx]
		mov	rax, [foobar3 wrt rip]
		mov	rbx, [foobar3 wrt rip]
		movzx	rax, byte [foobar3 wrt rip]
		movzx	rax, byte [foobar3+rax]

