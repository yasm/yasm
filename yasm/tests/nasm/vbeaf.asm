


























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





[extern _OpenFile]
[extern _CloseFile]
[extern _ReadFile]
[extern _ReadFile_Sel]
[extern _WriteFile]
[extern _WriteFile_Sel]
[extern _SeekFile]

_OpenFile_arglen	equ	6
_CloseFile_arglen	equ	4
_ReadFile_arglen	equ	12
_ReadFile_Sel_arglen	equ	14
_WriteFile_arglen	equ	12
_WriteFile_Sel_arglen	equ	14
_SeekFile_arglen	equ	10

[bits 32]


[extern ___sbrk]
[extern _getenv]

___sbrk_arglen	equ	4
_getenv_arglen	equ	4


[extern _djgpp_es]
[extern _djgpp_fs]
[extern _djgpp_gs]


[extern ___dos_argv0]























afHaveMultiBuffer	equ	0001h
afHaveVirtualScroll	equ	0002h
afHaveBankedBuffer	equ	0004h
afHaveLinearBuffer	equ	0008h
afHaveAccel2D		equ	0010h
afHaveDualBuffers	equ	0020h
afHaveHWCursor		equ	0040h
afHave8BitDAC		equ	0080h
afNonVGAMode		equ	0100h
afHaveDoubleScan	equ	0200h
afHaveInterlaced	equ	0400h
afHaveTripleBuffer	equ	0800h
afHaveStereo		equ	1000h
afHaveROP2		equ	2000h
afHaveHWStereoSync	equ	4000h
afHaveEVCStereoSync	equ	8000h


[absolute 0]
AF_MODE_INFO:
.Attributes		resw	1
.XResolution		resw	1
.YResolution		resw	1
.BytesPerScanLine	resw	1
.BitsPerPixel		resw	1
.MaxBuffers		resw	1
.RedMaskSize		resb	1
.RedFieldPosition	resb	1
.GreenMaskSize		resb	1
.GreenFieldPosition	resb	1
.BlueMaskSize		resb	1
.BlueFieldPosition	resb	1
.RsvdMaskSize		resb	1
.RsvdFieldPosition	resb	1
.MaxBytesPerScanLine	resw	1
.MaxScanLineWidth	resw	1

.LinBytesPerScanLine	resw	1
.BnkMaxBuffers		resb	1
.LinMaxBuffers		resb	1
.LinRedMaskSize		resb	1
.LinRedFieldPosition	resb	1
.LinGreenMaskSize	resb	1
.LinGreenFieldPosition	resb	1
.LinBlueMaskSize	resb	1
.LinBlueFieldPosition	resb	1
.LinRsvdMaskSize	resb	1
.LinRsvdFieldPosition	resb	1
.MaxPixelClock		resd	1
.VideoCapabilities	resd	1
.VideoMinXScale		resw	1
.VideoMinYScale		resw	1
.VideoMaxXScale		resw	1
.VideoMaxYScale		resw	1
.Reserved		resb	76
AF_MODE_INFO_size:
[section .text]


[absolute 0]
AF_DRIVER:

.Signature		resb	12
.Version		resd	1
.DriverRev		resd	1
.OemVendorName		resb	80
.OemCopyright		resb	80
.AvailableModes		resd	1
.TotalMemory		resd	1
.Attributes		resd	1
.BankSize		resd	1
.BankedBasePtr		resd	1
.LinearSize		resd	1
.LinearBasePtr		resd	1
.LinearGranularity	resd	1
.IOPortsTable		resd	1
.IOMemoryBase		resd	4
.IOMemoryLen		resd	4
.LinearStridePad	resd	1
.PCIVendorID		resw	1
.PCIDeviceID		resw	1
.PCISubSysVendorID	resw	1
.PCISubSysID		resw	1
.Checksum		resd	1
.res2			resd	6

.IOMemMaps		resd	4
.BankedMem		resd	1
.LinearMem		resd	1
.res3			resd	5

.BufferEndX		resd	1
.BufferEndY		resd	1
.OriginOffset		resd	1
.OffscreenOffset	resd	1
.OffscreenStartY	resd	1
.OffscreenEndY		resd	1
.res4			resd	10

.SetBank32Len		resd	1
.SetBank32		resd	1

.Int86			resd	1
.CallRealMode		resd	1

.InitDriver		resd	1

.af10Funcs		resd	40

.PlugAndPlayInit	resd	1

.OemExt			resd	1

.SupplementalExt	resd	1

.GetVideoModeInfo	resd	1
.SetVideoMode		resd	1
.RestoreTextMode	resd	1
.GetClosestPixelClock	resd	1
.SaveRestoreState	resd	1
.SetDisplayStart	resd	1
.SetActiveBuffer	resd	1
.SetVisibleBuffer	resd	1
.GetDisplayStartStatus	resd	1
.EnableStereoMode	resd	1
.SetPaletteData		resd	1
.SetGammaCorrectData	resd	1
.SetBank		resd	1

.SetCursor		resd	1
.SetCursorPos		resd	1
.SetCursorColor		resd	1
.ShowCursor		resd	1

.WaitTillIdle		resd	1
.EnableDirectAccess	resd	1
.DisableDirectAccess	resd	1
.SetMix			resd	1
.Set8x8MonoPattern	resd	1
.Set8x8ColorPattern	resd	1
.Use8x8ColorPattern	resd	1
.SetLineStipple		resd	1
.SetLineStippleCount	resd	1
.SetClipRect		resd	1
.DrawScan		resd	1
.DrawPattScan		resd	1
.DrawColorPattScan	resd	1
.DrawScanList		resd	1
.DrawPattScanList	resd	1
.DrawColorPattScanList	resd	1
.DrawRect		resd	1
.DrawPattRect		resd	1
.DrawColorPattRect	resd	1
.DrawLine		resd	1
.DrawStippleLine	resd	1
.DrawTrap		resd	1
.DrawTri		resd	1
.DrawQuad		resd	1
.PutMonoImage		resd	1
.PutMonoImageLin	resd	1
.PutMonoImageBM		resd	1
.BitBlt			resd	1
.BitBltSys		resd	1
.BitBltLin		resd	1
.BitBltBM		resd	1
.SrcTransBlt		resd	1
.SrcTransBltSys		resd	1
.SrcTransBltLin		resd	1
.SrcTransBltBM		resd	1
.DstTransBlt		resd	1
.DstTransBltSys		resd	1
.DstTransBltLin		resd	1
.DstTransBltBM		resd	1
.StretchBlt		resd	1
.StretchBltSys		resd	1
.StretchBltLin		resd	1
.StretchBltBM		resd	1
.SrcTransStretchBlt	resd	1
.SrcTransStretchBltSys	resd	1
.SrcTransStretchBltLin	resd	1
.SrcTransStretchBltBM	resd	1
.DstTransStretchBlt	resd	1
.DstTransStretchBltSys	resd	1
.DstTransStretchBltLin	resd	1
.DstTransStretchBltBM	resd	1

.SetVideoInput		resd	1
.SetVideoOutput		resd	1
.StartVideoFrame	resd	1
.EndVideoFrame		resd	1
AF_DRIVER_size:
[section .text]


[absolute 0]
FAF_KEYBOARD_DATA:
.INT	resb	1
.IRQ	resb	1
.Port	resw	1
FAF_KEYBOARD_DATA_size:
[section .text]

[section .bss]

DriverOffset	resd	1
DriverSize	resd	1
InGraphicsMode	resb	1
ModeInfo	resb	AF_MODE_INFO_size
Filename	resb	256
FreeBEExt	resb	1

[section .data]

rcsid	db	'$Id: vbeaf.asm,v 1.1 2001/11/21 08:41:53 peter Exp $',0

VBEAFName	db	'VBEAF.DRV', 0
VBEAFEnv	db	'VBEAF_PATH', 0

[section .text]







_LoadGraphicsDriver_arglen	equ	4
[global _LoadGraphicsDriver]
_LoadGraphicsDriver:       push  ebp
          mov   ebp, esp


.Filename       equ 8



.file		equ	-4
.STACK_FRAME_SIZE	equ	4

	sub	esp, .STACK_FRAME_SIZE
	push	es
	push	fs
	push	gs
	mov	gs, [_djgpp_gs]
	mov	fs, [_djgpp_fs]
	mov	es, [_djgpp_es]










                push word 0








                push dword [ebp+.Filename]

call _OpenFile
        add esp, byte _OpenFile_arglen
	cmp	eax, -1
	je	near .error
	mov	[ebp+.file], eax










                push word 2








                push dword 0








                push dword [ebp+.file]

call _SeekFile
        add esp, byte _SeekFile_arglen
	cmp	eax, -1
	je	near .error
	mov	[DriverSize], eax










                push word 0








                push dword 0








                push dword [ebp+.file]

call _SeekFile
        add esp, byte _SeekFile_arglen
	cmp	eax, -1
	je	.error














                push dword [DriverSize]

call ___sbrk
        add esp, byte ___sbrk_arglen
	test	eax, eax
	jz	.error
	mov	[DriverOffset], eax










                push dword [DriverSize]








                push dword [DriverOffset]








                push dword [ebp+.file]

call _ReadFile
        add esp, byte _ReadFile_arglen
	cmp	eax, [DriverSize]
	jne	.errorfree










                push dword [ebp+.file]

call _CloseFile
        add esp, byte _CloseFile_arglen










                push dword [DriverSize]








                push dword [DriverOffset]








                o16 push cs

call _LockArea
        add esp, byte _LockArea_arglen
	xor	eax, eax
	jmp	short .done

.errorfree:
	mov	eax, [DriverSize]
	neg	eax








                push eax

call ___sbrk
        add esp, byte ___sbrk_arglen
	mov	dword [DriverOffset], 0
.error:
	xor	eax, eax
	inc	eax
.done:
	pop	gs
	pop	fs
	pop	es
	mov	esp, ebp
	mov	esp, ebp
	pop	ebp
	ret	




















[global _InitGraphics]
_InitGraphics:       push  ebp
          mov   ebp, esp


.kbINT       equ 8


.kbIRQ       equ 12


.kbPort       equ 16



	push	es
	push	fs
	push	gs
	mov	gs, [_djgpp_gs]
	mov	fs, [_djgpp_fs]
	mov	es, [_djgpp_es]
	push	esi
	push	edi


	mov	esi, [DriverOffset]
	test	esi, esi
	jnz	near .foundit





	mov	ecx, 256
	mov	esi, [___dos_argv0]
	mov	edi, Filename
.argv0copy:
	mov	al, [esi]
	mov	[edi], al
	inc	esi
	inc	edi
	or	al, al
	jnz	.argv0copy

.findlastslash:
	cmp	byte [edi], '/'
	je	.lastslashfound
	cmp	byte [edi], '\'
	je	.lastslashfound
	cmp	edi, Filename
	je	.lastslashnotfound
	dec	edi
	jmp	short .findlastslash
.lastslashfound:
	inc	edi

	mov	ecx, 10
	mov	esi, VBEAFName
	rep	movsb










                push dword Filename

call _LoadGraphicsDriver
        add esp, byte _LoadGraphicsDriver_arglen
	test	eax, eax
	jz	near .foundit

.lastslashnotfound:

	mov	edi, Filename
	mov	dword [edi], 'C:\v'
	add	edi, byte 3
	mov	esi, VBEAFName
	mov	ecx, 10
	rep	movsb









                push dword Filename

call _LoadGraphicsDriver
        add esp, byte _LoadGraphicsDriver_arglen
	test	eax, eax
	jz	.foundit










                push dword VBEAFEnv

call _getenv
        add esp, byte _getenv_arglen
	test	eax, eax
	jz	near .error


	mov	ecx, 256
	mov	esi, eax
	mov	edi, Filename
.envcopy:
	mov	al, [esi]
	mov	[edi], al
	inc	esi
	inc	edi
	or	al, al
	jnz	.envcopy


	dec	edi
	cmp	byte [edi], '/'
	je	.slashpresent
	cmp	byte [edi], '\'
	je	.slashpresent
	mov	byte [edi], '/'
.slashpresent:

	inc	edi
	mov	esi, VBEAFName
	mov	ecx, 10
	rep	movsb









                push dword Filename

call _LoadGraphicsDriver
        add esp, byte _LoadGraphicsDriver_arglen
	test	eax, eax
	jz	.foundit

	jmp	.error
.foundit:
	mov	esi, [DriverOffset]


	cmp	dword [esi+AF_DRIVER.Signature], 'VBEA'
	jne	near .errorfree
	cmp	dword [esi+AF_DRIVER.Signature+4], 'F.DR'
	jne	near .errorfree
	cmp	byte [esi+AF_DRIVER.Signature+8], 'V'
	jne	near .errorfree


	cmp	word [esi+AF_DRIVER.Version], 0200h
	jb	near .errorfree


	mov	edi, [esi+AF_DRIVER.OemExt]
	add	edi, esi
	push	dword (('I'<<24) | ('N'<<16) | ('I'<<8) | 'T')
	push	esi
	call	edi
	add	esp, byte 8



	cmp	al, '0'
	jb	.noext
	cmp	al, '9'
	ja	.noext
	cmp	ah, '0'
	jb	.noext
	cmp	al, '9'
	ja	.noext

	and	eax, 0FFFF0000h
	cmp	eax, (('E'<<24) | ('X'<<16) | (0<<8) | 0)
	jne	.noext


	mov	byte [FreeBEExt], 1


	push	dword (('D'<<24) | ('I'<<16) | ('C'<<8) | 'L')
	push	esi
	call	[esi+AF_DRIVER.OemExt]
	add	esp, byte 8
	test	eax, eax
	jz	.noext

	mov	dword [eax], _DispatchCall

.noext:


	mov	edi, [esi+AF_DRIVER.PlugAndPlayInit]
	add	edi, esi
	mov	ebx, esi
	call	edi
	test	eax, eax
	jnz	near .errorfree

	mov	esi, [DriverOffset]




	mov	edi, [esi+AF_DRIVER.InitDriver]
	add	edi, esi
	mov	ebx, esi
	call	edi
	test	eax, eax
	jnz	near .errorfree

	mov	esi, [DriverOffset]


	cmp	byte [FreeBEExt], 0
	jz	.defaultkb


	push	dword (('K'<<24) | ('E'<<16) | ('Y'<<8) | 'S')
	push	esi
	call	[esi+AF_DRIVER.OemExt]
	add	esp, byte 8


	test	eax, eax
	jz	.defaultkb


	mov	edi, eax
	mov	al, [edi+FAF_KEYBOARD_DATA.INT]
	mov	ebx, [ebp+.kbINT]
	mov	[ebx], al
	mov	al, [edi+FAF_KEYBOARD_DATA.IRQ]
	mov	ebx, [ebp+.kbIRQ]
	mov	[ebx], al
	mov	ax, [edi+FAF_KEYBOARD_DATA.Port]
	mov	ebx, [ebp+.kbPort]
	mov	[ebx], ax

	jmp	short .retokay

.defaultkb:

	mov	edi, [ebp+.kbINT]
	mov	byte [edi], 9
	mov	edi, [ebp+.kbIRQ]
	mov	byte [edi], 1
	mov	edi, [ebp+.kbPort]
	mov	word [edi], 60h

.retokay:
	xor	eax, eax
	jmp	short .done

.errorfree:
	mov	eax, [DriverSize]
	neg	eax








                push eax

call ___sbrk
        add esp, byte ___sbrk_arglen
	mov	dword [DriverOffset], 0
.error:
	xor	eax, eax
	inc	eax
.done:
	pop	edi
	pop	esi
	pop	gs
	pop	fs
	pop	es
	mov	esp, ebp
	pop	ebp
	ret	


















[global _ExitGraphics]
_ExitGraphics


call _UnsetGraphicsMode

	push	es
	push	fs
	push	gs
	mov	gs, [_djgpp_gs]
	mov	fs, [_djgpp_fs]
	mov	es, [_djgpp_es]
	push	esi


	mov	esi, [DriverOffset]
	test	esi, esi
	jz	.notloaded
	mov	eax, [DriverSize]
	neg	eax








                push eax

call ___sbrk
        add esp, byte ___sbrk_arglen
	mov	dword [DriverOffset], 0
.notloaded:

	pop	esi
	pop	gs
	pop	fs
	pop	es
	ret	










[global _FindGraphicsMode]
_FindGraphicsMode:       push  ebp
          mov   ebp, esp

.Width       equ 8


.Height       equ 10


.Depth       equ 12


.Emulated       equ 14



	push	es
	push	fs
	push	gs
	mov	gs, [_djgpp_gs]
	mov	fs, [_djgpp_fs]
	mov	es, [_djgpp_es]
	push	esi
	push	edi

	mov	esi, [DriverOffset]


	mov	edi, [esi+AF_DRIVER.AvailableModes]
.modeloop:
	cmp	word [edi], -1
	je	.notfound


	push	dword ModeInfo
	push	dword [edi]
	push	esi
	call	[esi+AF_DRIVER.GetVideoModeInfo]
	add	esp, byte 12
	test	eax, eax
	jnz	.nextmode


	mov	ax, [ebp+.Width]
	cmp	[ModeInfo+AF_MODE_INFO.XResolution], ax
	jne	.nextmode

	mov	ax, [ebp+.Height]
	cmp	[ModeInfo+AF_MODE_INFO.YResolution], ax
	jne	.nextmode

	mov	ax, [ebp+.Depth]
	cmp	[ModeInfo+AF_MODE_INFO.BitsPerPixel], ax
	jne	.nextmode


	mov	eax, [ebp+.Emulated]
	test	eax, eax
	jnz	.foundit


	mov	eax, [ModeInfo+AF_MODE_INFO.VideoCapabilities]
	cmp	eax, (('E'<<24) | ('M'<<16) | ('U'<<8) | 'L')
	jne	.foundit

.nextmode:
	add	edi, byte 2
	jmp	short .modeloop

.foundit:
	movzx	eax, word [edi]
	jmp	short .done
.notfound:
	xor	eax, eax
.done:
	pop	edi
	pop	esi
	pop	gs
	pop	fs
	pop	es
	mov	esp, ebp
	pop	ebp
	ret	


















[global _SetGraphicsMode]
_SetGraphicsMode:       push  ebp
          mov   ebp, esp

.Mode       equ 8



	push	es
	push	fs
	push	gs
	mov	gs, [_djgpp_gs]
	mov	fs, [_djgpp_fs]
	mov	es, [_djgpp_es]

	mov	eax, [DriverOffset]
	xor	ecx, ecx
	push	ecx
	inc	ecx
	push	ecx
	push	dword ModeInfo+AF_MODE_INFO.BytesPerScanLine
	movzx	ecx, word [ModeInfo+AF_MODE_INFO.YResolution]
	push	ecx
	movzx	ecx, word [ModeInfo+AF_MODE_INFO.XResolution]
	push	ecx
	push	dword [ebp+.Mode]
	push	eax
	call	[eax+AF_DRIVER.SetVideoMode]
	add	esp, byte 28


	mov	byte [InGraphicsMode], 1



	pop	gs
	pop	fs
	pop	es
	mov	esp, ebp
	pop	ebp
	ret	


















[global _UnsetGraphicsMode]
_UnsetGraphicsMode


	cmp	byte [InGraphicsMode], 0
	jz	.notingraphics

	push	es
	push	fs
	push	gs
	mov	gs, [_djgpp_gs]
	mov	fs, [_djgpp_fs]
	mov	es, [_djgpp_es]
	push	esi
	push	edi


	mov	esi, [DriverOffset]


	mov	edi, [esi+AF_DRIVER.EnableDirectAccess]
	test	edi, edi
	jz	.noEDA
	push	esi
	call	edi
	add	esp, byte 4
.noEDA:

	mov	edi, [esi+AF_DRIVER.WaitTillIdle]
	test	edi, edi
	jz	.noWTI
	push	esi
	call	edi
	add	esp, byte 4
.noWTI:

	push	esi
	call	[esi+AF_DRIVER.RestoreTextMode]
	add	esp, byte 4


	mov	byte [InGraphicsMode], 0

	pop	edi
	pop	esi
	pop	gs
	pop	fs
	pop	es

.notingraphics:
	ret	
















[global _CopyToScreen]
_CopyToScreen:       push  ebp
          mov   ebp, esp


.Source       equ 8


.SourcePitch       equ 12


.SourceLeft       equ 16


.SourceTop       equ 20


.Width       equ 24


.Height       equ 28


.DestLeft       equ 32


.DestTop       equ 36



	push	es
	push	fs
	push	gs
	mov	gs, [_djgpp_gs]
	mov	fs, [_djgpp_fs]
	mov	es, [_djgpp_es]
	push	esi

	mov	esi, [DriverOffset]
	cmp	dword [esi+AF_DRIVER.BitBltSys], 0
	jz	.NoBitBltSys



	push	dword 0
	push	dword [ebp+.DestTop]
	push	dword [ebp+.DestLeft]
	push	dword [ebp+.Height]
	push	dword [ebp+.Width]
	push	dword [ebp+.SourceTop]
	push	dword [ebp+.SourceLeft]
	push	dword [ebp+.SourcePitch]
	push	dword [ebp+.Source]
	push	esi
	call	[esi+AF_DRIVER.BitBltSys]
	add	esp, byte 40


.NoBitBltSys:


.done:
	pop	esi
	pop	gs
	pop	fs
	pop	es
	mov	esp, ebp
	pop	ebp
	ret	





















[global _DispatchCall]
_DispatchCall:       push  ebp
          mov   ebp, esp


.Handle       equ 8


.Command       equ 12


.Data       equ 16



	push	fs
	push	esi

	mov	eax, ds
	mov	fs, eax

	mov	eax, [ebp+.Command]
	shl	eax, 16
	mov	ax, [ebp+.Handle]
	clc
	mov	esi, [ebp+.Data]
	db	0C4h, 0C4h, 058h, 002h
	salc
	movsx	eax, al

	pop	esi
	pop	fs
	mov	esp, ebp
	pop	ebp
	ret	












