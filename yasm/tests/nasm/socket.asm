































INADDR_ANY		equ	0
INADDR_LOOPBACK		equ	7F000001h
INADDR_BROADCAST	equ	0FFFFFFFFh
INADDR_NONE		equ	0FFFFFFFFh

SOCK_STREAM		equ	1
SOCK_DGRAM		equ	2

SOCKEVENT_READ		equ	01h
SOCKEVENT_WRITE		equ	02h
SOCKEVENT_OOB		equ	04h
SOCKEVENT_ACCEPT	equ	08h
SOCKEVENT_CONNECT	equ	10h
SOCKEVENT_CLOSE		equ	20h


[absolute 0]
SOCKADDR:
.Port		resw	1
.Address	resd	1
SOCKADDR_size:
[section .text]

[absolute 0]
HOSTENT:
.Name		resd	1
.Aliases	resd	1

.AddrList	resd	1

HOSTENT_size:
[section .text]






[extern DPMI_Int]
[extern _AllocTransferBuf]
[extern _FreeTransferBuf]

[extern DPMI_Regs]
[extern DPMI_EDI]
[extern DPMI_ESI]
[extern DPMI_EBP]
[extern DPMI_EBX]
[extern DPMI_EDX]
[extern DPMI_ECX]
[extern DPMI_EAX]
[extern DPMI_FLAGS]
[extern DPMI_ES]
[extern DPMI_DS]
[extern DPMI_FS]
[extern DPMI_GS]
[extern DPMI_SP]
[extern DPMI_SS]

[extern _Transfer_Buf]
[extern _Transfer_Buf_Seg]
[extern _Transfer_Buf_Size]

DPMI_EDI_off    equ     00h
DPMI_ESI_off    equ     04h
DPMI_EBP_off    equ     08h

DPMI_EBX_off    equ     10h
DPMI_EDX_off    equ     14h
DPMI_ECX_off    equ     18h
DPMI_EAX_off    equ     1Ch
DPMI_FLAGS_off  equ     20h
DPMI_ES_off     equ     22h
DPMI_DS_off     equ     24h
DPMI_FS_off     equ     26h
DPMI_GS_off     equ     28h
DPMI_IP_off     equ     2Ah
DPMI_CS_off     equ     2Ch
DPMI_SP_off     equ     2Eh
DPMI_SS_off     equ     30h





[extern _AllocMem]
[extern _AllocSelector]
[extern _FreeSelector]
[extern _GetPhysicalMapping]
[extern _FreePhysicalMapping]
[extern _LockArea]

_AllocMem_arglen		equ	4
_AllocSelector_arglen		equ	4
_FreeSelector_arglen		equ	2
_GetPhysicalMapping_arglen      equ     16
_FreePhysicalMapping_arglen     equ     8
_LockArea_arglen                equ     10





[extern _Install_Int]
[extern _Remove_Int]
[extern _Exit_IRQ]
[extern _Init_IRQ]
[extern _Restore_IRQ]
[extern _Enable_IRQ]
[extern _Disable_IRQ]

_Install_Int_arglen             equ     8
_Remove_Int_arglen              equ     4
_Restore_IRQ_arglen             equ     4
_Enable_IRQ_arglen              equ     4
_Disable_IRQ_arglen             equ     4

[bits 32]




SOCKET_INIT		equ	5001h
SOCKET_EXIT		equ	5002h
SOCKET_ACCEPT		equ	5003h
SOCKET_BIND		equ	5004h
SOCKET_CLOSE		equ	5005h
SOCKET_CONNECT		equ	5006h
SOCKET_GETPEERNAME	equ	5007h
SOCKET_GETSOCKNAME	equ	5008h
SOCKET_INETADDR		equ	5009h
SOCKET_INETNTOA		equ	500Ah
SOCKET_LISTEN		equ	500Bh
SOCKET_RECV		equ	500Ch
SOCKET_RECVFROM		equ	500Dh
SOCKET_SEND		equ	500Eh
SOCKET_SENDTO		equ	500Fh
SOCKET_SHUTDOWN		equ	5010h
SOCKET_CREATE		equ	5011h
SOCKET_GETHOSTBYADDR	equ	5012h
SOCKET_GETHOSTBYNAME	equ	5013h
SOCKET_GETHOSTNAME	equ	5014h
SOCKET_INSTALLCALLBACK	equ	5015h
SOCKET_REMOVECALLBACK	equ	5016h
SOCKET_ADDCALLBACK	equ	5017h
SOCKET_GETCALLBACKINFO	equ	5018h


STRING_MAX		equ	256
HOSTENT_ALIASES_MAX	equ	16
HOSTENT_ADDRLIST_MAX	equ	16

[section .bss]

Callback_Address	resd	1
Callback_Int		resb	1
Multiplex_Handle	resw	1
VDD_Handle		resw	1
LastError		resd	1
NetAddr_static		resb	STRING_MAX
HostEnt_Name_static	resb	STRING_MAX
HostEnt_Aliases_static	resd	HOSTENT_ALIASES_MAX
HostEnt_AddrList_static	resd	HOSTENT_ADDRLIST_MAX
HostEnt_Aliases_data	resb	STRING_MAX*HOSTENT_ALIASES_MAX
HostEnt_AddrList_data	resd	HOSTENT_ADDRLIST_MAX

[section .data]

rcsid	db	'$Id: socket.asm,v 1.1 2001/11/21 08:41:53 peter Exp $',0

ALTMPX_Signature	db	'ECE291  ', 'EX291   ', 0
ALTMPX_MinVersion	equ	0100h
SOCKET_Version		equ	0100h
HostEnt_static	:
..@44.strucstart:
times HOSTENT.Name-($-..@44.strucstart) db 0
dd	HostEnt_Name_static
times HOSTENT.Aliases-($-..@44.strucstart) db 0
dd	HostEnt_Aliases_static
times HOSTENT.AddrList-($-..@44.strucstart) db 0
dd	HostEnt_AddrList_static
times HOSTENT_size-($-..@44.strucstart) db 0


VDD_InitData
	dw	SOCKET_Version
	dd	STRING_MAX
	dd	HOSTENT_ALIASES_MAX
	dd	HOSTENT_ADDRLIST_MAX
	dd	LastError
	dd	NetAddr_static
	dd	HostEnt_Name_static
	dd	HostEnt_Aliases_static
	dd	HostEnt_AddrList_static
	dd	HostEnt_Aliases_data
	dd	HostEnt_AddrList_data

[section .text]







[global _InitSocket]
_InitSocket
	push	esi
	push	edi
	push	es



	cld


	xor	eax, eax
	mov	cx, 1
	int	31h
	jc	near .error
	mov	bx, ax


	mov	ax, 0008h
	xor	ecx, ecx
	mov	dx, 0FFFFh
	int	31h
	jc	near .errorfree

	xor	eax, eax

.loop:
	mov	esi, eax
	mov	[DPMI_EAX], eax
	push	bx
	mov	bx, 2Dh
	call	DPMI_Int
	pop	bx
	cmp	byte [DPMI_EAX], 0FFh
	jne	.next


	cmp	word [DPMI_ECX], ALTMPX_MinVersion
	jb	.next


	mov	ax, 0007h
	movzx	edx, word [DPMI_EDX]
	shl	edx, 4
	mov	ecx, edx
	shr	ecx, 16
	int	31h
	jc	.errorfree

	mov	es, ebx
	movzx	edi, word [DPMI_EDI]
	mov	ecx, 16/2
	mov	esi, ALTMPX_Signature
	rep	cmpsw
	jz	.foundmpx

.next:
	inc	ah
	jnz	.loop

	jmp	short .errorfree

.foundmpx:

	mov	ax, 0001h
	int	31h


	mov	eax, [DPMI_EAX]
	mov	[Multiplex_Handle], ax
	mov	al, 10h
	mov	[DPMI_EAX], ax
	push	bx
	mov	bx, 2Dh
	call	DPMI_Int
	pop	bx
	cmp	byte [DPMI_EAX], 1
	jne	.error
	mov	dx, [DPMI_EDX]
	mov	[VDD_Handle], dx


	mov	ecx, VDD_InitData
	clc
	movzx	eax, word [VDD_Handle]
	or	eax, SOCKET_INIT<<16
	db	0C4h, 0C4h, 058h, 002h
	jc	.error

	xor	eax, eax
	jmp	short .exit

.errorfree:
	mov	ax, 0001h
	int	31h

.error:
	xor	eax, eax
	inc	eax

.exit:
	pop	es
	pop	edi
	pop	esi
	ret	







[global _ExitSocket]
_ExitSocket

	clc
	movzx	eax, word [VDD_Handle]
	or	eax, SOCKET_EXIT<<16
	db	0C4h, 0C4h, 058h, 002h
	ret	











[global _Socket_SetCallback]
_Socket_SetCallback:       push  ebp
          mov   ebp, esp


.HandlerAddress       equ 8





	mov	ax, [Multiplex_Handle]
	mov	al, 11h
	mov	[DPMI_EAX], ax
	mov	bx, 2Dh
	call	DPMI_Int
	cmp	byte [DPMI_EAX], 1
	jne	near .error

	movzx	edx, byte [DPMI_EDX]
	mov	[Callback_Int], dl

	mov	ecx, [Callback_Address]
	mov	eax, [ebp+.HandlerAddress]
	mov	[Callback_Address], eax
	test	eax, eax
	jz	near .deinstall

	test	ecx, ecx
	jnz	near .ok










                push dword 4








                push dword Callback_Address








                o16 push ds

call _LockArea
        add esp, byte _LockArea_arglen








                push dword 1








                push dword Callback_Int








                o16 push ds

call _LockArea
        add esp, byte _LockArea_arglen








                push dword Socket_InterruptHandler_end-Socket_InterruptHandler








                push dword Socket_InterruptHandler








                o16 push cs

call _LockArea
        add esp, byte _LockArea_arglen
	movzx	edx, byte [DPMI_EDX]








                push dword Socket_InterruptHandler








                push edx

call _Install_Int
        add esp, byte _Install_Int_arglen
	test	eax, eax
	jnz	.error

	movzx	edx, byte [Callback_Int]
	clc
	movzx	eax, word [VDD_Handle]
	or	eax, SOCKET_INSTALLCALLBACK<<16
	db	0C4h, 0C4h, 058h, 002h
	test	eax, eax
	jz	.end

	mov	dword [Callback_Address], 0








                push edx

call _Remove_Int
        add esp, byte _Remove_Int_arglen
	jmp	short .error

.deinstall:
	clc
	movzx	eax, word [VDD_Handle]
	or	eax, SOCKET_REMOVECALLBACK<<16
	db	0C4h, 0C4h, 058h, 002h








                push edx

call _Remove_Int
        add esp, byte _Remove_Int_arglen
	xor	eax, eax
	jmp	short .end

.ok:
	xor	eax, eax
	mov	esp, ebp
	pop	ebp
	ret	
.error:
	xor	eax, eax
	inc	eax
.end:
	mov	esp, ebp
	pop	ebp
	ret	





























[global _Socket_AddCallback]
_Socket_AddCallback:       push  ebp
          mov   ebp, esp


.Socket       equ 8


.EventMask       equ 12



	clc
	movzx	eax, word [VDD_Handle]
	or	eax, SOCKET_ADDCALLBACK<<16
	db	0C4h, 0C4h, 058h, 002h
	mov	esp, ebp
	pop	ebp
	ret	












Socket_InterruptHandler

	mov	eax, [Callback_Address]
	test	eax, eax
	jz	.chain

	clc
	movzx	eax, word [VDD_Handle]
	or	eax, SOCKET_GETCALLBACKINFO<<16
	db	0C4h, 0C4h, 058h, 002h
	test	eax, eax
	jnz	.chain

	push	edx
	push	ecx
	call	dword [Callback_Address]
	add	esp, byte 8

.done:

	mov	al, 20h
	cmp	byte [Callback_Int], 50h
	jb	.lowirq
	out	0A0h, al
.lowirq:
	out	20h, al

	xor	eax, eax
	ret	

.chain:
	mov	eax, 1
	ret	
Socket_InterruptHandler_end











[global _Socket_accept]
_Socket_accept:       push  ebp
          mov   ebp, esp


.Socket       equ 8


.Name       equ 12



	clc
	movzx	eax, word [VDD_Handle]
	or	eax, SOCKET_ACCEPT<<16
	db	0C4h, 0C4h, 058h, 002h
	mov	esp, ebp
	pop	ebp
	ret	




















[global _Socket_bind]
_Socket_bind:       push  ebp
          mov   ebp, esp


.Socket       equ 8


.Name       equ 12



	clc
	movzx	eax, word [VDD_Handle]
	or	eax, SOCKET_BIND<<16
	db	0C4h, 0C4h, 058h, 002h
	mov	esp, ebp
	pop	ebp
	ret	


















[global _Socket_close]
_Socket_close:       push  ebp
          mov   ebp, esp


.Socket       equ 8



	clc
	movzx	eax, word [VDD_Handle]
	or	eax, SOCKET_CLOSE<<16
	db	0C4h, 0C4h, 058h, 002h
	mov	esp, ebp
	pop	ebp
	ret	




















[global _Socket_connect]
_Socket_connect:       push  ebp
          mov   ebp, esp


.Socket       equ 8


.Name       equ 12



	clc
	movzx	eax, word [VDD_Handle]
	or	eax, SOCKET_CONNECT<<16
	db	0C4h, 0C4h, 058h, 002h
	mov	esp, ebp
	pop	ebp
	ret	




















[global _Socket_getpeername]
_Socket_getpeername:       push  ebp
          mov   ebp, esp


.Socket       equ 8


.Name       equ 12



	clc
	movzx	eax, word [VDD_Handle]
	or	eax, SOCKET_GETPEERNAME<<16
	db	0C4h, 0C4h, 058h, 002h
	mov	esp, ebp
	pop	ebp
	ret	




















[global _Socket_getsockname]
_Socket_getsockname:       push  ebp
          mov   ebp, esp


.Socket       equ 8


.Name       equ 12



	clc
	movzx	eax, word [VDD_Handle]
	or	eax, SOCKET_GETSOCKNAME<<16
	db	0C4h, 0C4h, 058h, 002h
	mov	esp, ebp
	pop	ebp
	ret	


















[global _Socket_ntohl]
_Socket_ntohl







[global _Socket_htonl]
_Socket_htonl:       push  ebp
          mov   ebp, esp


.HostVal       equ 8



	mov	eax, [ebp+.HostVal]
	bswap	eax

	mov	esp, ebp
	pop	ebp
	ret	


















[global _Socket_ntohs]
_Socket_ntohs







[global _Socket_htons]
_Socket_htons:       push  ebp
          mov   ebp, esp


.HostVal       equ 8



	mov	ax, [ebp+.HostVal]
	xchg	al, ah
	mov	esp, ebp
	pop	ebp
	ret	




















[global _Socket_inet_addr]
_Socket_inet_addr:       push  ebp
          mov   ebp, esp


.DottedAddress       equ 8



	clc
	movzx	eax, word [VDD_Handle]
	or	eax, SOCKET_INETADDR<<16
	db	0C4h, 0C4h, 058h, 002h
	mov	esp, ebp
	pop	ebp
	ret	




















[global _Socket_inet_ntoa]
_Socket_inet_ntoa:       push  ebp
          mov   ebp, esp


.Address       equ 8



	clc
	movzx	eax, word [VDD_Handle]
	or	eax, SOCKET_INETNTOA<<16
	db	0C4h, 0C4h, 058h, 002h
	mov	eax, NetAddr_static
	mov	esp, ebp
	pop	ebp
	ret	





















[global _Socket_listen]
_Socket_listen:       push  ebp
          mov   ebp, esp


.Socket       equ 8


.BackLog       equ 12



	clc
	movzx	eax, word [VDD_Handle]
	or	eax, SOCKET_LISTEN<<16
	db	0C4h, 0C4h, 058h, 002h
	mov	esp, ebp
	pop	ebp
	ret	


























[global _Socket_recv]
_Socket_recv:       push  ebp
          mov   ebp, esp


.Socket       equ 8


.Buf       equ 12


.MaxLen       equ 16


.Flags       equ 20



	clc
	movzx	eax, word [VDD_Handle]
	or	eax, SOCKET_RECV<<16
	db	0C4h, 0C4h, 058h, 002h
	mov	esp, ebp
	pop	ebp
	ret	




























[global _Socket_recvfrom]
_Socket_recvfrom:       push  ebp
          mov   ebp, esp


.Socket       equ 8


.Buf       equ 12


.MaxLen       equ 16


.Flags       equ 20


.From       equ 24



	clc
	movzx	eax, word [VDD_Handle]
	or	eax, SOCKET_RECVFROM<<16
	db	0C4h, 0C4h, 058h, 002h
	mov	esp, ebp
	pop	ebp
	ret	























[global _Socket_send]
_Socket_send:       push  ebp
          mov   ebp, esp


.Socket       equ 8


.Buf       equ 12


.Len       equ 16


.Flags       equ 20



	clc
	movzx	eax, word [VDD_Handle]
	or	eax, SOCKET_SEND<<16
	db	0C4h, 0C4h, 058h, 002h
	mov	esp, ebp
	pop	ebp
	ret	
























[global _Socket_sendto]
_Socket_sendto:       push  ebp
          mov   ebp, esp


.Socket       equ 8


.Buf       equ 12


.Len       equ 16


.Flags       equ 20


.To       equ 24



	clc
	movzx	eax, word [VDD_Handle]
	or	eax, SOCKET_SENDTO<<16
	db	0C4h, 0C4h, 058h, 002h
	mov	esp, ebp
	pop	ebp
	ret	

























[global _Socket_shutdown]
_Socket_shutdown:       push  ebp
          mov   ebp, esp


.Socket       equ 8


.Flags       equ 12



	mov	eax, [ebp+.Flags]
	test	eax, eax
	jz	.done

	clc
	movzx	eax, word [VDD_Handle]
	or	eax, SOCKET_SHUTDOWN<<16
	db	0C4h, 0C4h, 058h, 002h
.done:
	mov	esp, ebp
	pop	ebp
	ret	




















[global _Socket_create]
_Socket_create:       push  ebp
          mov   ebp, esp


.Type       equ 8



	clc
	movzx	eax, word [VDD_Handle]
	or	eax, SOCKET_CREATE<<16
	db	0C4h, 0C4h, 058h, 002h
	mov	esp, ebp
	pop	ebp
	ret	




















[global _Socket_gethostbyaddr]
_Socket_gethostbyaddr:       push  ebp
          mov   ebp, esp


.Address       equ 8



	clc
	movzx	eax, word [VDD_Handle]
	or	eax, SOCKET_GETHOSTBYADDR<<16
	db	0C4h, 0C4h, 058h, 002h
	test	eax, eax
	jz	.done
	mov	eax, HostEnt_static
.done:
	mov	esp, ebp
	pop	ebp
	ret	



















[global _Socket_gethostbyname]
_Socket_gethostbyname:       push  ebp
          mov   ebp, esp


.Name       equ 8



	clc
	movzx	eax, word [VDD_Handle]
	or	eax, SOCKET_GETHOSTBYNAME<<16
	db	0C4h, 0C4h, 058h, 002h
	test	eax, eax
	jz	.done
	mov	eax, HostEnt_static
.done:
	mov	esp, ebp
	pop	ebp
	ret	




















[global _Socket_gethostname]
_Socket_gethostname:       push  ebp
          mov   ebp, esp


.Name       equ 8


.NameLen       equ 12



	clc
	movzx	eax, word [VDD_Handle]
	or	eax, SOCKET_GETHOSTNAME<<16
	db	0C4h, 0C4h, 058h, 002h
	mov	esp, ebp
	pop	ebp
	ret	


















[global _Socket_GetLastError]
_Socket_GetLastError

	mov	eax, [LastError]
	ret	

