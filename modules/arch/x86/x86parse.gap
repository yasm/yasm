# GAP (gen_arch_parse) input file for x86 architecture
# $Id$

# Configure GAP for x86 generation mode
ARCH	x86

# Supported x86 parsers
PARSERS	nasm gas

# INSN parameters:
# - parser (- if any)
# - base name of instruction
# - if string, each character is an allowed GAS suffix
#   if defined name, value is GAS suffix mode set (no character suffix reqd)
# - instruction group (sans _insn suffix)
# - modifiers (up to 3 bytes)
# - CPU flags
#
# The string mode of the second parameter is a shortcut for GAS forms, e.g.:
# INSN	-	mov	"bwl"	mov	0	CPU_Any
# is equivalent to:
# INSN	-	mov	NONE	mov	0	CPU_Any
# INSN	gas	movb	SUF_B	mov	0	CPU_Any
# INSN	gas	movw	SUF_W	mov	0	CPU_Any
# INSN	gas	movl	SUF_L	mov	0	CPU_Any

# Move
INSN	-	mov	"bwl"	mov	0		CPU_Any
INSN	gas	movabs	"bwlq"	movabs	0		CPU_Hammer|CPU_64

# Move with sign/zero extend
INSN	gas	movsbw	SUF_B	movszx	0xBE		CPU_386
INSN	gas	movsbl	SUF_B	movszx	0xBE		CPU_386
INSN	gas	movswl	SUF_W	movszx	0xBE		CPU_386
INSN	gas	movsbq	SUF_B	movszx	0xBE		CPU_Hammer|CPU_64
INSN	gas	movswq	SUF_W	movszx	0xBE		CPU_Hammer|CPU_64
INSN	-	movsx	"bw"	movszx	0xBE		CPU_386
INSN	gas	movslq	SUF_L	movsxd	0		CPU_Hammer|CPU_64
INSN	nasm	movsxd	NONE	movsxd	0		CPU_Hammer|CPU_64
INSN	gas	movzbw	SUF_B	movszx	0xB6		CPU_386
INSN	gas	movzbl	SUF_B	movszx	0xB6		CPU_386
INSN	gas	movzwl	SUF_W	movszx	0xB6		CPU_386
INSN	gas	movzbq	SUF_B	movszx	0xB6		CPU_Hammer|CPU_64
INSN	gas	movzwq	SUF_W	movszx	0xB6		CPU_Hammer|CPU_64
INSN	-	movzx	NONE	movszx	0xB6		CPU_386

# Push instructions
INSN	-	push	"wlq"	push	0		CPU_Any
INSN	-	pusha	NONE	onebyte	0x0060		CPU_186|CPU_Not64
INSN	nasm	pushad	NONE	onebyte	0x2060		CPU_386|CPU_Not64
INSN	gas	pushal	NONE	onebyte	0x2060		CPU_386|CPU_Not64
INSN	-	pushaw	NONE	onebyte	0x1060		CPU_186|CPU_Not64

# Pop instructions
INSN	-	pop	"wlq"	pop	0		CPU_Any
INSN	-	popa	NONE	onebyte	0x0061		CPU_186|CPU_Not64
INSN	nasm	popad	NONE	onebyte	0x2061		CPU_386|CPU_Not64
INSN	gas	popal	NONE	onebyte	0x2061		CPU_386|CPU_Not64
INSN	-	popaw	NONE	onebyte	0x1061		CPU_186|CPU_Not64

# Exchange
INSN	-	xchg	"bwlq"	xchg	0		CPU_Any

# In/out from ports
INSN	-	in	"bwl"	in	0		CPU_Any
INSN	-	out	"bwl"	out	0		CPU_Any
# Load effective address
INSN	-	lea	"wlq"	lea	0		CPU_Any
# Load segment registers from memory
INSN	-	lds	"wl"	ldes	0xC5		CPU_Not64
INSN	-	les	"wl"	ldes	0xC4		CPU_Not64
INSN	-	lfs	"wl"	lfgss	0xB4		CPU_386
INSN	-	lgs	"wl"	lfgss	0xB5		CPU_386
INSN	-	lss	"wl"	lfgss	0xB2		CPU_386
# Flags register instructions
INSN	-	clc	NONE	onebyte	0x00F8		CPU_Any
INSN	-	cld	NONE	onebyte	0x00FC		CPU_Any
INSN	-	cli	NONE	onebyte	0x00FA		CPU_Any
INSN	-	clts	NONE	twobyte	0x0F06		CPU_286|CPU_Priv
INSN	-	cmc	NONE	onebyte	0x00F5		CPU_Any
INSN	-	lahf	NONE	onebyte	0x009F		CPU_Any
INSN	-	sahf	NONE	onebyte	0x009E		CPU_Any
INSN	-	pushf	NONE	onebyte	0x40009C	CPU_Any
INSN	nasm	pushfd	NONE	onebyte	0x00209C	CPU_386|CPU_Not64
INSN	gas	pushfl	NONE	onebyte	0x00209C	CPU_386|CPU_Not64
INSN	-	pushfw	NONE	onebyte	0x40109C	CPU_Any
INSN	-	pushfq	NONE	onebyte	0x40409C	CPU_Hammer|CPU_64
INSN	-	popf	NONE	onebyte	0x40009D	CPU_Any
INSN	nasm	popfd	NONE	onebyte	0x00209D	CPU_386|CPU_Not64
INSN	gas	popfl   NONE	onebyte	0x00209D	CPU_386|CPU_Not64
INSN	-	popfw   NONE	onebyte	0x40109D	CPU_Any
INSN	-	popfq   NONE	onebyte	0x40409D	CPU_Hammer|CPU_64
INSN	-	stc	NONE	onebyte	0x00F9		CPU_Any
INSN	-	std	NONE	onebyte	0x00FD		CPU_Any
INSN	-	sti	NONE	onebyte	0x00FB		CPU_Any
# Arithmetic
INSN	-	add	"bwlq"	arith	0x0000		CPU_Any
INSN	-	inc	"bwlq"	incdec	0x0040		CPU_Any
INSN	-	sub	"bwlq"	arith	0x0528		CPU_Any
INSN	-	dec	"bwlq"	incdec	0x0148		CPU_Any
INSN	-	sbb	"bwlq"	arith	0x0318		CPU_Any
INSN	-	cmp	"bwlq"	arith	0x0738		CPU_Any
INSN	-	test    "bwlq"	test	0		CPU_Any
INSN	-	and	"bwlq"	arith	0x0420		CPU_Any
INSN	-	or	"bwlq"	arith	0x0108		CPU_Any
INSN	-	xor	"bwlq"	arith	0x0630		CPU_Any
INSN	-	adc	"bwlq"	arith	0x0210		CPU_Any
INSN	-	neg	"bwlq"	f6	0x03		CPU_Any
INSN	-	not	"bwlq"	f6	0x02		CPU_Any
INSN	-	aaa	NONE	onebyte	0x0037		CPU_Not64
INSN	-	aas	NONE	onebyte	0x003F		CPU_Not64
INSN	-	daa	NONE	onebyte	0x0027		CPU_Not64
INSN	-	das	NONE	onebyte	0x002F		CPU_Not64
INSN	-	aad	NONE	aadm	0x01		CPU_Not64
INSN	-	aam	NONE	aadm	0x00		CPU_Not64
# Conversion instructions
INSN	-	cbw	NONE	onebyte	0x1098		CPU_Any
INSN	-	cwde    NONE	onebyte	0x2098		CPU_386
INSN	-	cdqe    NONE	onebyte	0x4098		CPU_Hammer|CPU_64
INSN	-	cwd	NONE	onebyte	0x1099		CPU_Any
INSN	-	cdq	NONE	onebyte	0x2099		CPU_386
INSN	-	cqo	NONE	onebyte	0x4099		CPU_Hammer|CPU_64
# Conversion instructions - GAS / AT&T naming
INSN	gas	cbtw	NONE	onebyte	0x1098		CPU_Any
INSN	gas	cwtl	NONE	onebyte	0x2098		CPU_386
INSN	gas	cltq	NONE	onebyte	0x4098		CPU_Hammer|CPU_64
INSN	gas	cwtd	NONE	onebyte	0x1099		CPU_Any
INSN	gas	cltd	NONE	onebyte	0x2099		CPU_386
INSN	gas	cqto	NONE	onebyte	0x4099		CPU_Hammer|CPU_64
# Multiplication and division
INSN	-	mul	"bwlq"	f6	0x04		CPU_Any
INSN	-	imul	"bwlq"	imul	0		CPU_Any
INSN	-	div	"bwlq"	div	0x06		CPU_Any
INSN	-	idiv	"bwlq"	div	0x07		CPU_Any
# Shifts
INSN	-	rol	"bwlq"	shift	0x00		CPU_Any
INSN	-	ror	"bwlq"	shift	0x01		CPU_Any
INSN	-	rcl	"bwlq"	shift	0x02		CPU_Any
INSN	-	rcr	"bwlq"	shift	0x03		CPU_Any
INSN	-	sal	"bwlq"	shift	0x04		CPU_Any
INSN	-	shl	"bwlq"	shift	0x04		CPU_Any
INSN	-	shr	"bwlq"	shift	0x05		CPU_Any
INSN	-	sar	"bwlq"	shift	0x07		CPU_Any
INSN	-	shld	"wlq"	shlrd	0xA4		CPU_386
INSN	-	shrd	"wlq"	shlrd	0xAC		CPU_386
# Control transfer instructions unconditional)
INSN	-	call	NONE	call	0		CPU_Any
INSN	-	jmp	NONE	jmp	0		CPU_Any
INSN	-	ret	NONE	retnf	0x00C2		CPU_Any
INSN	gas	retw	NONE	retnf	0x10C2		CPU_Any
INSN	gas	retl	NONE	retnf	0x00C2		CPU_Not64
INSN	gas	retq	NONE	retnf	0x00C2		CPU_Hammer|CPU_64
INSN	nasm	retn	NONE	retnf	0x00C2		CPU_Any
INSN	nasm	retf	NONE	retnf	0x40CA		CPU_Any
INSN	gas	lretw	NONE	retnf	0x10CA		CPU_Any
INSN	gas	lretl	NONE	retnf	0x00CA		CPU_Any
INSN	gas	lretq	NONE	retnf	0x40CA		CPU_Hammer|CPU_64
INSN	-	enter	"wlq"	enter	0		CPU_186
INSN	-	leave	NONE	onebyte	0x4000C9	CPU_186
INSN	gas	leavew	NONE	onebyte	0x0010C9	CPU_186
INSN	gas	leavel	NONE	onebyte	0x4000C9	CPU_186
INSN	gas	leaveq	NONE	onebyte	0x4000C9	CPU_Hammer|CPU_64
# Conditional jumps
INSN	-	jo	NONE	jcc	0x00		CPU_Any
INSN	-	jno	NONE	jcc	0x01		CPU_Any
INSN	-	jb	NONE	jcc	0x02		CPU_Any
INSN	-	jc	NONE	jcc	0x02		CPU_Any
INSN	-	jnae	NONE	jcc	0x02		CPU_Any
INSN	-	jnb	NONE	jcc	0x03		CPU_Any
INSN	-	jnc	NONE	jcc	0x03		CPU_Any
INSN	-	jae	NONE	jcc	0x03		CPU_Any
INSN	-	je	NONE	jcc	0x04		CPU_Any
INSN	-	jz	NONE	jcc	0x04		CPU_Any
INSN	-	jne	NONE	jcc	0x05		CPU_Any
INSN	-	jnz	NONE	jcc	0x05		CPU_Any
INSN	-	jbe	NONE	jcc	0x06		CPU_Any
INSN	-	jna	NONE	jcc	0x06		CPU_Any
INSN	-	jnbe	NONE	jcc	0x07		CPU_Any
INSN	-	ja	NONE	jcc	0x07		CPU_Any
INSN	-	js	NONE	jcc	0x08		CPU_Any
INSN	-	jns	NONE	jcc	0x09		CPU_Any
INSN	-	jp	NONE	jcc	0x0A		CPU_Any
INSN	-	jpe	NONE	jcc	0x0A		CPU_Any
INSN	-	jnp	NONE	jcc	0x0B		CPU_Any
INSN	-	jpo	NONE	jcc	0x0B		CPU_Any
INSN	-	jl	NONE	jcc	0x0C		CPU_Any
INSN	-	jnge	NONE	jcc	0x0C		CPU_Any
INSN	-	jnl	NONE	jcc	0x0D		CPU_Any
INSN	-	jge	NONE	jcc	0x0D		CPU_Any
INSN	-	jle	NONE	jcc	0x0E		CPU_Any
INSN	-	jng	NONE	jcc	0x0E		CPU_Any
INSN	-	jnle	NONE	jcc	0x0F		CPU_Any
INSN	-	jg	NONE	jcc	0x0F		CPU_Any
INSN	-	jcxz	NONE	jcxz	0x10		CPU_Any
INSN	-	jecxz	NONE	jcxz	0x20		CPU_386
INSN	-	jrcxz	NONE	jcxz	0x40		CPU_Hammer|CPU_64
# Loop instructions
INSN	-	loop	NONE	loop	0x02		CPU_Any
INSN	-	loopz	NONE	loop	0x01		CPU_Any
INSN	-	loope	NONE	loop	0x01		CPU_Any
INSN	-	loopnz	NONE	loop	0x00		CPU_Any
INSN	-	loopne	NONE	loop	0x00		CPU_Any
# Set byte on flag instructions
INSN	-	seto	"b"	setcc	0x00		CPU_386
INSN	-	setno	"b"	setcc	0x01		CPU_386
INSN	-	setb	"b"	setcc	0x02		CPU_386
INSN	-	setc	"b"	setcc	0x02		CPU_386
INSN	-	setnae	"b"	setcc	0x02		CPU_386
INSN	-	setnb	"b"	setcc	0x03		CPU_386
INSN	-	setnc	"b"	setcc	0x03		CPU_386
INSN	-	setae	"b"	setcc	0x03		CPU_386
INSN	-	sete	"b"	setcc	0x04		CPU_386
INSN	-	setz	"b"	setcc	0x04		CPU_386
INSN	-	setne	"b"	setcc	0x05		CPU_386
INSN	-	setnz	"b"	setcc	0x05		CPU_386
INSN	-	setbe	"b"	setcc	0x06		CPU_386
INSN	-	setna	"b"	setcc	0x06		CPU_386
INSN	-	setnbe	"b"	setcc	0x07		CPU_386
INSN	-	seta	"b"	setcc	0x07		CPU_386
INSN	-	sets	"b"	setcc	0x08		CPU_386
INSN	-	setns	"b"	setcc	0x09		CPU_386
INSN	-	setp	"b"	setcc	0x0A		CPU_386
INSN	-	setpe	"b"	setcc	0x0A		CPU_386
INSN	-	setnp	"b"	setcc	0x0B		CPU_386
INSN	-	setpo	"b"	setcc	0x0B		CPU_386
INSN	-	setl	"b"	setcc	0x0C		CPU_386
INSN	-	setnge	"b"	setcc	0x0C		CPU_386
INSN	-	setnl	"b"	setcc	0x0D		CPU_386
INSN	-	setge	"b"	setcc	0x0D		CPU_386
INSN	-	setle	"b"	setcc	0x0E		CPU_386
INSN	-	setng	"b"	setcc	0x0E		CPU_386
INSN	-	setnle	"b"	setcc	0x0F		CPU_386
INSN	-	setg	"b"	setcc	0x0F		CPU_386
# String instructions
INSN	-	cmpsb	NONE	onebyte	0x00A6		CPU_Any
INSN	-	cmpsw	NONE	onebyte	0x10A7		CPU_Any
INSN	-	cmpsd	NONE	cmpsd	0		CPU_Any
INSN	gas	cmpsl	NONE	onebyte	0x20A7		CPU_386
INSN	-	cmpsq	NONE	onebyte	0x40A7		CPU_Hammer|CPU_64
INSN	-	insb	NONE	onebyte	0x006C		CPU_Any
INSN	-	insw	NONE	onebyte	0x106D		CPU_Any
INSN	nasm	insd	NONE	onebyte	0x206D		CPU_386
INSN	gas	insl	NONE	onebyte	0x206D		CPU_386
INSN	-	outsb	NONE	onebyte	0x006E		CPU_Any
INSN	-	outsw	NONE	onebyte	0x106F		CPU_Any
INSN	nasm	outsd	NONE	onebyte	0x206F		CPU_386
INSN	gas	outsl	NONE	onebyte	0x206F		CPU_386
INSN	-	lodsb	NONE	onebyte	0x00AC		CPU_Any
INSN	-	lodsw	NONE	onebyte	0x10AD		CPU_Any
INSN	nasm	lodsd	NONE	onebyte	0x20AD		CPU_386
INSN	gas	lodsl	NONE	onebyte	0x20AD		CPU_386
INSN	-	lodsq	NONE	onebyte	0x40AD		CPU_Hammer|CPU_64
INSN	-	movsb	NONE	onebyte	0x00A4		CPU_Any
INSN	-	movsw	NONE	onebyte	0x10A5		CPU_Any
INSN	-	movsd	NONE	movsd	0		CPU_Any
INSN	gas	movsl	NONE	onebyte	0x20A5		CPU_386
INSN	-	movsq	NONE	onebyte	0x40A5		CPU_Hammer|CPU_64
# smov alias for movs in GAS mode
INSN	gas	smovb	NONE	onebyte	0x00A4		CPU_Any
INSN	gas	smovw	NONE	onebyte	0x10A5		CPU_Any
INSN	gas	smovl	NONE	onebyte	0x20A5		CPU_386
INSN	gas	smovq	NONE	onebyte	0x40A5		CPU_Hammer|CPU_64
INSN	-	scasb	NONE	onebyte	0x00AE		CPU_Any
INSN	-	scasw	NONE	onebyte	0x10AF		CPU_Any
INSN	nasm	scasd	NONE	onebyte	0x20AF		CPU_386
INSN	gas	scasl	NONE	onebyte	0x20AF		CPU_386
INSN	-	scasq	NONE	onebyte	0x40AF		CPU_Hammer|CPU_64
# ssca alias for scas in GAS mode
INSN	gas	sscab	NONE	onebyte	0x00AE		CPU_Any
INSN	gas	sscaw	NONE	onebyte	0x10AF		CPU_Any
INSN	gas	sscal	NONE	onebyte	0x20AF		CPU_386
INSN	gas	sscaq	NONE	onebyte	0x40AF		CPU_Hammer|CPU_64
INSN	-	stosb	NONE	onebyte	0x00AA		CPU_Any
INSN	-	stosw	NONE	onebyte	0x10AB		CPU_Any
INSN	nasm	stosd	NONE	onebyte	0x20AB		CPU_386
INSN	gas	stosl	NONE	onebyte	0x20AB		CPU_386
INSN	-	stosq	NONE	onebyte	0x40AB		CPU_Hammer|CPU_64
INSN	-	xlatb	NONE	onebyte	0x00D7		CPU_Any
# Bit manipulation
INSN	-	bsf	"wlq"	bsfr	0xBC		CPU_386
INSN	-	bsr	"wlq"	bsfr	0xBD		CPU_386
INSN	-	bt	"wlq"	bittest	0x04A3		CPU_386
INSN	-	btc	"wlq"	bittest	0x07BB		CPU_386
INSN	-	btr	"wlq"	bittest	0x06B3		CPU_386
INSN	-	bts	"wlq"	bittest	0x05AB		CPU_386
# Interrupts and operating system instructions
INSN	-	int	NONE	int	0		CPU_Any
INSN	-	int3	NONE	onebyte	0x00CC		CPU_Any
INSN	nasm	int03	NONE	onebyte	0x00CC		CPU_Any
INSN	-	into	NONE	onebyte	0x00CE		CPU_Not64
INSN	-	iret	NONE	onebyte	0x00CF		CPU_Any
INSN	-	iretw	NONE	onebyte	0x10CF		CPU_Any
INSN	nasm	iretd	NONE	onebyte	0x20CF		CPU_386
INSN	gas	iretl	NONE	onebyte	0x20CF		CPU_386
INSN	-	iretq	NONE	onebyte	0x40CF		CPU_Hammer|CPU_64
INSN	-	rsm	NONE	twobyte	0x0FAA		CPU_586|CPU_SMM
INSN	-	bound	"wl"	bound	0		CPU_186|CPU_Not64
INSN	-	hlt	NONE	onebyte	0x00F4		CPU_Priv
INSN	-	nop	NONE	onebyte	0x0090		CPU_Any
# Protection control
INSN	-	arpl	"w"	arpl	0		CPU_286|CPU_Prot|CPU_Not64
INSN	-	lar	"wlq"	bsfr	0x02		CPU_286|CPU_Prot
INSN	-	lgdt	"wlq"	twobytemem  0x020F01	CPU_286|CPU_Priv
INSN	-	lidt	"wlq"	twobytemem  0x030F01	CPU_286|CPU_Priv
INSN	-	lldt	"w"	prot286	0x0200		CPU_286|CPU_Prot|CPU_Priv
INSN	-	lmsw	"w"	prot286	0x0601		CPU_286|CPU_Priv
INSN	-	lsl	"wlq"	bsfr	0x03		CPU_286|CPU_Prot
INSN	-	ltr	"w"	prot286	0x0300		CPU_286|CPU_Prot|CPU_Priv
INSN	-	sgdt	"wlq"	twobytemem  0x000F01	CPU_286|CPU_Priv
INSN	-	sidt	"wlq"	twobytemem  0x010F01	CPU_286|CPU_Priv
INSN	-	sldt	"wlq"	sldtmsw	0x0000		CPU_286
INSN	-	smsw	"wlq"	sldtmsw	0x0401		CPU_286
INSN	-	str	"wlq"	str	0		CPU_286|CPU_Prot
INSN	-	verr	"w"	prot286	0x0400		CPU_286|CPU_Prot
INSN	-	verw	"w"	prot286	0x0500		CPU_286|CPU_Prot
# Floating point instructions
INSN	-	fld	"ls"	fld	0		CPU_FPU
INSN	gas	fldt	WEAK	fldstpt	0x05		CPU_FPU
INSN	-	fild	"lqs"	fildstp	0x050200	CPU_FPU
INSN	gas	fildll	NONE	fbldstp	0x05		CPU_FPU
INSN	-	fbld	NONE	fbldstp	0x04		CPU_FPU
INSN	-	fst	"ls"	fst	0		CPU_FPU
INSN	-	fist	"ls"	fiarith	0x02DB		CPU_FPU
INSN	-	fstp	"ls"	fstp	0		CPU_FPU
INSN	gas	fstpt	WEAK	fldstpt	0x07		CPU_FPU
INSN	-	fistp	"lqs"	fildstp	0x070203	CPU_FPU
INSN	gas	fistpll	NONE	fbldstp	0x07		CPU_FPU
INSN	-	fbstp	NONE	fbldstp	0x06		CPU_FPU
INSN	-	fxch	NONE	fxch	0		CPU_FPU
INSN	-	fcom	"ls"	fcom	0x02D0		CPU_FPU
INSN	-	ficom	"ls"	fiarith	0x02DA		CPU_FPU
INSN	-	fcomp	"ls"	fcom	0x03D8		CPU_FPU
INSN	-	ficomp	"ls"	fiarith	0x03DA		CPU_FPU
INSN	-	fcompp	NONE	twobyte	0xDED9		CPU_FPU
INSN	-	fucom	NONE	fcom2	0xDDE0		CPU_286|CPU_FPU
INSN	-	fucomp	NONE	fcom2	0xDDE8		CPU_286|CPU_FPU
INSN	-	fucompp	NONE	twobyte	0xDAE9		CPU_286|CPU_FPU
INSN	-	ftst	NONE	twobyte	0xD9E4		CPU_FPU
INSN	-	fxam	NONE	twobyte	0xD9E5		CPU_FPU
INSN	-	fld1	NONE	twobyte	0xD9E8		CPU_FPU
INSN	-	fldl2t	NONE	twobyte	0xD9E9		CPU_FPU
INSN	-	fldl2e	NONE	twobyte	0xD9EA		CPU_FPU
INSN	-	fldpi	NONE	twobyte	0xD9EB		CPU_FPU
INSN	-	fldlg2	NONE	twobyte	0xD9EC		CPU_FPU
INSN	-	fldln2	NONE	twobyte	0xD9ED		CPU_FPU
INSN	-	fldz	NONE	twobyte	0xD9EE		CPU_FPU
INSN	-	fadd	"ls"	farith	0x00C0C0	CPU_FPU
INSN	-	faddp	NONE	farithp	0xC0		CPU_FPU
INSN	-	fiadd	"ls"	fiarith	0x00DA		CPU_FPU
INSN	-	fsub	"ls"	farith	0x04E0E8	CPU_FPU
INSN	-	fisub	"ls"	fiarith	0x04DA		CPU_FPU
INSN	-	fsubp	NONE	farithp	0xE8		CPU_FPU
INSN	-	fsubr	"ls"	farith	0x05E8E0	CPU_FPU
INSN	-	fisubr	"ls"	fiarith	0x05DA		CPU_FPU
INSN	-	fsubrp	NONE	farithp	0xE0		CPU_FPU
INSN	-	fmul	"ls"	farith	0x01C8C8	CPU_FPU
INSN	-	fimul	"ls"	fiarith	0x01DA		CPU_FPU
INSN	-	fmulp	NONE	farithp	0xC8		CPU_FPU
INSN	-	fdiv	"ls"	farith	0x06F0F8	CPU_FPU
INSN	-	fidiv	"ls"	fiarith	0x06DA		CPU_FPU
INSN	-	fdivp	NONE	farithp	0xF8		CPU_FPU
INSN	-	fdivr	"ls"	farith	0x07F8F0	CPU_FPU
INSN	-	fidivr	"ls"	fiarith	0x07DA		CPU_FPU
INSN	-	fdivrp	NONE	farithp	0xF0		CPU_FPU
INSN	-	f2xm1	NONE	twobyte	0xD9F0		CPU_FPU
INSN	-	fyl2x	NONE	twobyte	0xD9F1		CPU_FPU
INSN	-	fptan	NONE	twobyte	0xD9F2		CPU_FPU
INSN	-	fpatan	NONE	twobyte	0xD9F3		CPU_FPU
INSN	-	fxtract	NONE	twobyte	0xD9F4		CPU_FPU
INSN	-	fprem1	NONE	twobyte	0xD9F5		CPU_286|CPU_FPU
INSN	-	fdecstp	NONE	twobyte	0xD9F6		CPU_FPU
INSN	-	fincstp	NONE	twobyte	0xD9F7		CPU_FPU
INSN	-	fprem	NONE	twobyte	0xD9F8		CPU_FPU
INSN	-	fyl2xp1	NONE	twobyte	0xD9F9		CPU_FPU
INSN	-	fsqrt	NONE	twobyte	0xD9FA		CPU_FPU
INSN	-	fsincos	NONE	twobyte	0xD9FB		CPU_286|CPU_FPU
INSN	-	frndint	NONE	twobyte	0xD9FC		CPU_FPU
INSN	-	fscale	NONE	twobyte	0xD9FD		CPU_FPU
INSN	-	fsin	NONE	twobyte	0xD9FE		CPU_286|CPU_FPU
INSN	-	fcos	NONE	twobyte	0xD9FF		CPU_286|CPU_FPU
INSN	-	fchs	NONE	twobyte	0xD9E0		CPU_FPU
INSN	-	fabs	NONE	twobyte	0xD9E1		CPU_FPU
INSN	-	fninit	NONE	twobyte	0xDBE3		CPU_FPU
INSN	-	finit	NONE	threebyte   0x9BDBE3	CPU_FPU
INSN	-	fldcw	"w"	fldnstcw	0x05	CPU_FPU
INSN	-	fnstcw	"w"	fldnstcw	0x07	CPU_FPU
INSN	-	fstcw	"w"	fstcw	0		CPU_FPU
INSN	-	fnstsw	"w"	fnstsw	0		CPU_FPU
INSN	-	fstsw	"w"	fstsw	0		CPU_FPU
INSN	-	fnclex	NONE	twobyte	0xDBE2		CPU_FPU
INSN	-	fclex	NONE	threebyte   0x9BDBE2	CPU_FPU
INSN	-	fnstenv	"ls"	onebytemem 0x06D9	CPU_FPU
INSN	-	fstenv	"ls"	twobytemem 0x069BD9	CPU_FPU
INSN	-	fldenv	"ls"	onebytemem 0x04D9	CPU_FPU
INSN	-	fnsave	"ls"	onebytemem 0x06DD	CPU_FPU
INSN	-	fsave	"ls"	twobytemem 0x069BDD	CPU_FPU
INSN	-	frstor	"ls"	onebytemem 0x04DD	CPU_FPU
INSN	-	ffree	NONE	ffree	0xDD		CPU_FPU
INSN	-	ffreep	NONE	ffree	0xDF		CPU_686|CPU_FPU|CPU_Undoc
INSN	-	fnop	NONE	twobyte	0xD9D0		CPU_FPU
INSN	-	fwait	NONE	onebyte	0x009B		CPU_FPU
# Prefixes should the others be here too? should wait be a prefix?
INSN	-	wait	NONE	onebyte	0x009B		CPU_Any
# 486 extensions
INSN	-	bswap	"lq"	bswap	0		CPU_486
INSN	-	xadd	"bwlq"	cmpxchgxadd 0xC0	CPU_486
INSN	-	cmpxchg	"bwlq"	cmpxchgxadd 0xB0	CPU_486
INSN	nasm	cmpxchg486 NONE cmpxchgxadd 0xA6	CPU_486|CPU_Undoc
INSN	-	invd	NONE	twobyte	0x0F08		CPU_486|CPU_Priv
INSN	-	wbinvd	NONE	twobyte	0x0F09		CPU_486|CPU_Priv
INSN	-	invlpg	NONE	twobytemem  0x070F01	CPU_486|CPU_Priv
# 586+ and late 486 extensions
INSN	-	cpuid	NONE	twobyte	0x0FA2		CPU_486
# Pentium extensions
INSN	-	wrmsr	NONE	twobyte	0x0F30		CPU_586|CPU_Priv
INSN	-	rdtsc	NONE	twobyte	0x0F31		CPU_586
INSN	-	rdmsr	NONE	twobyte	0x0F32		CPU_586|CPU_Priv
INSN	-	cmpxchg8b "q"	cmpxchg8b	0	CPU_586
# Pentium II/Pentium Pro extensions
INSN	-	sysenter NONE	twobyte	0x0F34		CPU_686|CPU_Not64
INSN	-	sysexit	NONE	twobyte	0x0F35		CPU_686|CPU_Priv|CPU_Not64
INSN	-	fxsave	"q"	twobytemem  0x000FAE	CPU_686|CPU_FPU
INSN	-	fxrstor	"q"	twobytemem  0x010FAE	CPU_686|CPU_FPU
INSN	-	rdpmc	NONE	twobyte	0x0F33		CPU_686
INSN	-	ud2	NONE	twobyte	0x0F0B		CPU_286
INSN	-	ud1	NONE	twobyte	0x0FB9		CPU_286|CPU_Undoc
INSN	-	cmovo	"wlq"	cmovcc	0x00		CPU_686
INSN	-	cmovno	"wlq"	cmovcc	0x01		CPU_686
INSN	-	cmovb	"wlq"	cmovcc	0x02		CPU_686
INSN	-	cmovc	"wlq"	cmovcc	0x02		CPU_686
INSN	-	cmovnae	"wlq"	cmovcc	0x02		CPU_686
INSN	-	cmovnb	"wlq"	cmovcc	0x03		CPU_686
INSN	-	cmovnc	"wlq"	cmovcc	0x03		CPU_686
INSN	-	cmovae	"wlq"	cmovcc	0x03		CPU_686
INSN	-	cmove	"wlq"	cmovcc	0x04		CPU_686
INSN	-	cmovz	"wlq"	cmovcc	0x04		CPU_686
INSN	-	cmovne	"wlq"	cmovcc	0x05		CPU_686
INSN	-	cmovnz	"wlq"	cmovcc	0x05		CPU_686
INSN	-	cmovbe	"wlq"	cmovcc	0x06		CPU_686
INSN	-	cmovna	"wlq"	cmovcc	0x06		CPU_686
INSN	-	cmovnbe	"wlq"	cmovcc	0x07		CPU_686
INSN	-	cmova	"wlq"	cmovcc	0x07		CPU_686
INSN	-	cmovs	"wlq"	cmovcc	0x08		CPU_686
INSN	-	cmovns	"wlq"	cmovcc	0x09		CPU_686
INSN	-	cmovp	"wlq"	cmovcc	0x0A		CPU_686
INSN	-	cmovpe	"wlq"	cmovcc	0x0A		CPU_686
INSN	-	cmovnp	"wlq"	cmovcc	0x0B		CPU_686
INSN	-	cmovpo	"wlq"	cmovcc	0x0B		CPU_686
INSN	-	cmovl	"wlq"	cmovcc	0x0C		CPU_686
INSN	-	cmovnge	"wlq"	cmovcc	0x0C		CPU_686
INSN	-	cmovnl	"wlq"	cmovcc	0x0D		CPU_686
INSN	-	cmovge	"wlq"	cmovcc	0x0D		CPU_686
INSN	-	cmovle	"wlq"	cmovcc	0x0E		CPU_686
INSN	-	cmovng	"wlq"	cmovcc	0x0E		CPU_686
INSN	-	cmovnle	"wlq"	cmovcc	0x0F		CPU_686
INSN	-	cmovg	"wlq"	cmovcc	0x0F		CPU_686
INSN	-	fcmovb	NONE	fcmovcc	0xDAC0		CPU_686|CPU_FPU
INSN	-	fcmove	NONE	fcmovcc	0xDAC8		CPU_686|CPU_FPU
INSN	-	fcmovbe	NONE	fcmovcc	0xDAD0		CPU_686|CPU_FPU
INSN	-	fcmovu	NONE	fcmovcc	0xDAD8		CPU_686|CPU_FPU
INSN	-	fcmovnb	NONE	fcmovcc	0xDBC0		CPU_686|CPU_FPU
INSN	-	fcmovne	NONE	fcmovcc	0xDBC8		CPU_686|CPU_FPU
INSN	-	fcmovnbe NONE	fcmovcc	0xDBD0		CPU_686|CPU_FPU
INSN	-	fcmovnu	NONE	fcmovcc	0xDBD8		CPU_686|CPU_FPU
INSN	-	fcomi	NONE	fcom2	0xDBF0		CPU_686|CPU_FPU
INSN	-	fucomi	NONE	fcom2	0xDBE8		CPU_686|CPU_FPU
INSN	-	fcomip	NONE	fcom2	0xDFF0		CPU_686|CPU_FPU
INSN	-	fucomip	NONE	fcom2	0xDFE8		CPU_686|CPU_FPU
# Pentium4 extensions
INSN	-	movnti	"lq"	movnti	0		CPU_P4
INSN	-	clflush	NONE	clflush	0		CPU_P3
INSN	-	lfence	NONE	threebyte   0x0FAEE8	CPU_P3
INSN	-	mfence	NONE	threebyte   0x0FAEF0	CPU_P3
INSN	-	pause	NONE	onebyte_prefix	0xF390	CPU_P4
# MMX/SSE2 instructions
INSN	-	emms	NONE	twobyte	0x0F77		CPU_MMX
INSN	-	movd	NONE	movd	0		CPU_MMX
# For GAS movq must use standard mov instruction.
# For NASM it can use a dedicated instruction.
INSN	gas	movq	SUF_Q	mov	0		CPU_Any
INSN	nasm	movq	NONE	movq	0		CPU_MMX
INSN	-	packssdw NONE	mmxsse2	0x6B		CPU_MMX
INSN	-	packsswb NONE	mmxsse2	0x63		CPU_MMX
INSN	-	packuswb NONE	mmxsse2	0x67		CPU_MMX
INSN	-	paddb	NONE	mmxsse2	0xFC		CPU_MMX
INSN	-	paddw	NONE	mmxsse2	0xFD		CPU_MMX
INSN	-	paddd	NONE	mmxsse2	0xFE		CPU_MMX
INSN	-	paddq	NONE	mmxsse2	0xD4		CPU_MMX
INSN	-	paddsb	NONE	mmxsse2	0xEC		CPU_MMX
INSN	-	paddsw	NONE	mmxsse2	0xED		CPU_MMX
INSN	-	paddusb	NONE	mmxsse2	0xDC		CPU_MMX
INSN	-	paddusw	NONE	mmxsse2	0xDD		CPU_MMX
INSN	-	pand	NONE	mmxsse2	0xDB		CPU_MMX
INSN	-	pandn	NONE	mmxsse2	0xDF		CPU_MMX
INSN	-	pcmpeqb	NONE	mmxsse2	0x74		CPU_MMX
INSN	-	pcmpeqw	NONE	mmxsse2	0x75		CPU_MMX
INSN	-	pcmpeqd	NONE	mmxsse2	0x76		CPU_MMX
INSN	-	pcmpgtb	NONE	mmxsse2	0x64		CPU_MMX
INSN	-	pcmpgtw	NONE	mmxsse2	0x65		CPU_MMX
INSN	-	pcmpgtd	NONE	mmxsse2	0x66		CPU_MMX
INSN	-	pmaddwd	NONE	mmxsse2	0xF5		CPU_MMX
INSN	-	pmulhw	NONE	mmxsse2	0xE5		CPU_MMX
INSN	-	pmullw	NONE	mmxsse2	0xD5		CPU_MMX
INSN	-	por	NONE	mmxsse2	0xEB		CPU_MMX
INSN	-	psllw	NONE	pshift	0x0671F1	CPU_MMX
INSN	-	pslld	NONE	pshift	0x0672F2	CPU_MMX
INSN	-	psllq	NONE	pshift	0x0673F3	CPU_MMX
INSN	-	psraw	NONE	pshift	0x0471E1	CPU_MMX
INSN	-	psrad	NONE	pshift	0x0472E2	CPU_MMX
INSN	-	psrlw	NONE	pshift	0x0271D1	CPU_MMX
INSN	-	psrld	NONE	pshift	0x0272D2	CPU_MMX
INSN	-	psrlq	NONE	pshift	0x0273D3	CPU_MMX
INSN	-	psubb	NONE	mmxsse2	0xF8		CPU_MMX
INSN	-	psubw	NONE	mmxsse2	0xF9		CPU_MMX
INSN	-	psubd	NONE	mmxsse2	0xFA		CPU_MMX
INSN	-	psubq	NONE	mmxsse2	0xFB		CPU_MMX
INSN	-	psubsb	NONE	mmxsse2	0xE8		CPU_MMX
INSN	-	psubsw	NONE	mmxsse2	0xE9		CPU_MMX
INSN	-	psubusb	NONE	mmxsse2	0xD8		CPU_MMX
INSN	-	psubusw	NONE	mmxsse2	0xD9		CPU_MMX
INSN	-	punpckhbw NONE	mmxsse2	0x68		CPU_MMX
INSN	-	punpckhwd NONE	mmxsse2	0x69		CPU_MMX
INSN	-	punpckhdq NONE	mmxsse2	0x6A		CPU_MMX
INSN	-	punpcklbw NONE	mmxsse2	0x60		CPU_MMX
INSN	-	punpcklwd NONE	mmxsse2	0x61		CPU_MMX
INSN	-	punpckldq NONE	mmxsse2	0x62		CPU_MMX
INSN	-	pxor	NONE	mmxsse2	0xEF		CPU_MMX
# PIII Katmai new instructions / SIMD instructions
INSN	-	addps	NONE	sseps	0x58		CPU_SSE
INSN	-	addss	NONE	ssess	0xF358		CPU_SSE
INSN	-	andnps	NONE	sseps	0x55		CPU_SSE
INSN	-	andps	NONE	sseps	0x54		CPU_SSE
INSN	-	cmpeqps	NONE	ssecmpps	0x00	CPU_SSE
INSN	-	cmpeqss	NONE	ssecmpss	0x00F3	CPU_SSE
INSN	-	cmpleps	NONE	ssecmpps	0x02	CPU_SSE
INSN	-	cmpless	NONE	ssecmpss	0x02F3	CPU_SSE
INSN	-	cmpltps	NONE	ssecmpps	0x01	CPU_SSE
INSN	-	cmpltss	NONE	ssecmpss	0x01F3	CPU_SSE
INSN	-	cmpneqps NONE	ssecmpps	0x04	CPU_SSE
INSN	-	cmpneqss NONE	ssecmpss	0x04F3	CPU_SSE
INSN	-	cmpnleps NONE	ssecmpps	0x06	CPU_SSE
INSN	-	cmpnless NONE	ssecmpss	0x06F3	CPU_SSE
INSN	-	cmpnltps NONE	ssecmpps	0x05	CPU_SSE
INSN	-	cmpnltss NONE	ssecmpss	0x05F3	CPU_SSE
INSN	-	cmpordps NONE	ssecmpps	0x07	CPU_SSE
INSN	-	cmpordss NONE	ssecmpss	0x07F3	CPU_SSE
INSN	-	cmpunordps NONE	ssecmpps	0x03	CPU_SSE
INSN	-	cmpunordss NONE	ssecmpss	0x03F3	CPU_SSE
INSN	-	cmpps	NONE	ssepsimm	0xC2	CPU_SSE
INSN	-	cmpss	NONE	ssessimm	0xF3C2	CPU_SSE
INSN	-	comiss	NONE	sseps	0x2F		CPU_SSE
INSN	-	cvtpi2ps NONE	cvt_xmm_mm_ps  0x2A	CPU_SSE
INSN	-	cvtps2pi NONE	cvt_mm_xmm64   0x2D	CPU_SSE
INSN	-	cvtsi2ss "lq"	cvt_xmm_rmx    0xF32A	CPU_SSE
INSN	-	cvtss2si "lq"	cvt_rx_xmm32   0xF32D	CPU_SSE
INSN	-	cvttps2pi NONE	cvt_mm_xmm64   0x2C	CPU_SSE
INSN	-	cvttss2si "lq"	cvt_rx_xmm32   0xF32C	CPU_SSE
INSN	-	divps	NONE	sseps	0x5E		CPU_SSE
INSN	-	divss	NONE	ssess	0xF35E		CPU_SSE
INSN	-	ldmxcsr	NONE	ldstmxcsr	0x02	CPU_SSE
INSN	-	maskmovq NONE	maskmovq	0	CPU_P3|CPU_MMX
INSN	-	maxps	NONE	sseps	0x5F		CPU_SSE
INSN	-	maxss	NONE	ssess	0xF35F		CPU_SSE
INSN	-	minps	NONE	sseps	0x5D		CPU_SSE
INSN	-	minss	NONE	ssess	0xF35D		CPU_SSE
INSN	-	movaps	NONE	movaups	0x28		CPU_SSE
INSN	-	movhlps	NONE	movhllhps	0x12	CPU_SSE
INSN	-	movhps	NONE	movhlps	0x16		CPU_SSE
INSN	-	movlhps	NONE	movhllhps	0x16	CPU_SSE
INSN	-	movlps	NONE	movhlps	0x12		CPU_SSE
INSN	-	movmskps "lq"	movmskps	0	CPU_SSE
INSN	-	movntps	NONE	movntps	0		CPU_SSE
INSN	-	movntq	NONE	movntq	0		CPU_SSE
INSN	-	movss	NONE	movss	0		CPU_SSE
INSN	-	movups	NONE	movaups	0x10		CPU_SSE
INSN	-	mulps	NONE	sseps	0x59		CPU_SSE
INSN	-	mulss	NONE	ssess	0xF359		CPU_SSE
INSN	-	orps	NONE	sseps	0x56		CPU_SSE
INSN	-	pavgb	NONE	mmxsse2	0xE0		CPU_P3|CPU_MMX
INSN	-	pavgw	NONE	mmxsse2	0xE3		CPU_P3|CPU_MMX
INSN	-	pextrw	"lq"	pextrw	0		CPU_P3|CPU_MMX
INSN	-	pinsrw	"lq"	pinsrw	0		CPU_P3|CPU_MMX
INSN	-	pmaxsw	NONE	mmxsse2	0xEE		CPU_P3|CPU_MMX
INSN	-	pmaxub	NONE	mmxsse2	0xDE		CPU_P3|CPU_MMX
INSN	-	pminsw	NONE	mmxsse2	0xEA		CPU_P3|CPU_MMX
INSN	-	pminub	NONE	mmxsse2	0xDA		CPU_P3|CPU_MMX
INSN	-	pmovmskb "lq"	pmovmskb	0	CPU_SSE
INSN	-	pmulhuw NONE	mmxsse2	0xE4		CPU_P3|CPU_MMX
INSN	-	prefetchnta NONE twobytemem 0x000F18	CPU_P3
INSN	-	prefetcht0 NONE	twobytemem  0x010F18	CPU_P3
INSN	-	prefetcht1 NONE	twobytemem  0x020F18	CPU_P3
INSN	-	prefetcht2 NONE	twobytemem  0x030F18	CPU_P3
INSN	-	psadbw	NONE	mmxsse2	0xF6		CPU_P3|CPU_MMX
INSN	-	pshufw	NONE	pshufw	0		CPU_P3|CPU_MMX
INSN	-	rcpps	NONE	sseps	0x53		CPU_SSE
INSN	-	rcpss	NONE	ssess	0xF353		CPU_SSE
INSN	-	rsqrtps	NONE	sseps	0x52		CPU_SSE
INSN	-	rsqrtss	NONE	ssess	0xF352		CPU_SSE
INSN	-	sfence	NONE	threebyte   0x0FAEF8	CPU_P3
INSN	-	shufps	NONE	ssepsimm	0xC6	CPU_SSE
INSN	-	sqrtps	NONE	sseps	0x51		CPU_SSE
INSN	-	sqrtss	NONE	ssess	0xF351		CPU_SSE
INSN	-	stmxcsr	NONE	ldstmxcsr	0x03	CPU_SSE
INSN	-	subps	NONE	sseps	0x5C		CPU_SSE
INSN	-	subss	NONE	ssess	0xF35C		CPU_SSE
INSN	-	ucomiss	NONE	ssess	0x2E		CPU_SSE
INSN	-	unpckhps NONE	sseps	0x15		CPU_SSE
INSN	-	unpcklps NONE	sseps	0x14		CPU_SSE
INSN	-	xorps	NONE	sseps	0x57		CPU_SSE
# SSE2 instructions
INSN	-	addpd	NONE	ssess	0x6658		CPU_SSE2
INSN	-	addsd	NONE	ssess	0xF258		CPU_SSE2
INSN	-	andnpd	NONE	ssess	0x6655		CPU_SSE2
INSN	-	andpd	NONE	ssess	0x6654		CPU_SSE2
INSN	-	cmpeqpd	NONE	ssecmpss	0x0066	CPU_SSE2
INSN	-	cmpeqsd	NONE	ssecmpss	0x00F2	CPU_SSE2
INSN	-	cmplepd	NONE	ssecmpss	0x0266	CPU_SSE2
INSN	-	cmplesd	NONE	ssecmpss	0x02F2	CPU_SSE2
INSN	-	cmpltpd	NONE	ssecmpss	0x0166	CPU_SSE2
INSN	-	cmpltsd	NONE	ssecmpss	0x01F2	CPU_SSE2
INSN	-	cmpneqpd NONE	ssecmpss	0x0466	CPU_SSE2
INSN	-	cmpneqsd NONE	ssecmpss	0x04F2	CPU_SSE2
INSN	-	cmpnlepd NONE	ssecmpss	0x0666	CPU_SSE2
INSN	-	cmpnlesd NONE	ssecmpss	0x06F2	CPU_SSE2
INSN	-	cmpnltpd NONE	ssecmpss	0x0566	CPU_SSE2
INSN	-	cmpnltsd NONE	ssecmpss	0x05F2	CPU_SSE2
INSN	-	cmpordpd NONE	ssecmpss	0x0766	CPU_SSE2
INSN	-	cmpordsd NONE	ssecmpss	0x07F2	CPU_SSE2
INSN	-	cmpunordpd NONE	ssecmpss	0x0366	CPU_SSE2
INSN	-	cmpunordsd NONE	ssecmpss	0x03F2	CPU_SSE2
INSN	-	cmppd	NONE	ssessimm	0x66C2	CPU_SSE2
# cmpsd is in string instructions above
INSN	-	comisd	NONE	ssess	0x662F		CPU_SSE2
INSN	-	cvtpi2pd NONE	cvt_xmm_mm_ss	0x662A	CPU_SSE2
INSN	-	cvtsi2sd "lq"	cvt_xmm_rmx	0xF22A	CPU_SSE2
INSN	-	divpd	NONE	ssess	0x665E		CPU_SSE2
INSN	-	divsd	NONE	ssess	0xF25E		CPU_SSE2
INSN	-	maxpd	NONE	ssess	0x665F		CPU_SSE2
INSN	-	maxsd	NONE	ssess	0xF25F		CPU_SSE2
INSN	-	minpd	NONE	ssess	0x665D		CPU_SSE2
INSN	-	minsd	NONE	ssess	0xF25D		CPU_SSE2
INSN	-	movapd	NONE	movaupd	0x28		CPU_SSE2
INSN	-	movhpd	NONE	movhlpd	0x16		CPU_SSE2
INSN	-	movlpd	NONE	movhlpd	0x12		CPU_SSE2
INSN	-	movmskpd "lq"	movmskpd	0	CPU_SSE2
INSN	-	movntpd	NONE	movntpddq	0x2B	CPU_SSE2
INSN	-	movntdq	NONE	movntpddq	0xE7	CPU_SSE2
# movsd is in string instructions above
INSN	-	movupd	NONE	movaupd	0x10		CPU_SSE2
INSN	-	mulpd	NONE	ssess	0x6659		CPU_SSE2
INSN	-	mulsd	NONE	ssess	0xF259		CPU_SSE2
INSN	-	orpd	NONE	ssess	0x6656		CPU_SSE2
INSN	-	shufpd	NONE	ssessimm	0x66C6	CPU_SSE2
INSN	-	sqrtpd	NONE	ssess	0x6651		CPU_SSE2
INSN	-	sqrtsd	NONE	ssess	0xF251		CPU_SSE2
INSN	-	subpd	NONE	ssess	0x665C		CPU_SSE2
INSN	-	subsd	NONE	ssess	0xF25C		CPU_SSE2
INSN	-	ucomisd	NONE	ssess	0x662E		CPU_SSE2
INSN	-	unpckhpd NONE	ssess	0x6615		CPU_SSE2
INSN	-	unpcklpd NONE	ssess	0x6614		CPU_SSE2
INSN	-	xorpd	NONE	ssess	0x6657		CPU_SSE2
INSN	-	cvtdq2pd NONE	cvt_xmm_xmm64_ss 0xF3E6	CPU_SSE2
INSN	-	cvtpd2dq NONE	ssess	0xF2E6		CPU_SSE2
INSN	-	cvtdq2ps NONE	sseps	0x5B		CPU_SSE2
INSN	-	cvtpd2pi NONE	cvt_mm_xmm	0x662D	CPU_SSE2
INSN	-	cvtpd2ps NONE	ssess	0x665A		CPU_SSE2
INSN	-	cvtps2pd NONE	cvt_xmm_xmm64_ps 0x5A	CPU_SSE2
INSN	-	cvtps2dq NONE	ssess	0x665B		CPU_SSE2
INSN	-	cvtsd2si "lq"	cvt_rx_xmm64	0xF22D	CPU_SSE2
INSN	-	cvtsd2ss NONE	cvt_xmm_xmm64_ss 0xF25A	CPU_SSE2
# P4 VMX Instructions
INSN	-	vmcall	NONE	threebyte   0x0F01C1	CPU_P4
INSN	-	vmlaunch NONE	threebyte   0x0F01C2	CPU_P4
INSN	-	vmresume NONE	threebyte   0x0F01C3	CPU_P4
INSN	-	vmxoff	NONE	threebyte   0x0F01C4	CPU_P4
INSN	-	vmread	"lq"	vmxmemrd    0x0F78	CPU_P4
INSN	-	vmwrite	"lq"	vmxmemwr    0x0F79	CPU_P4
INSN	-	vmptrld	NONE	vmxtwobytemem	0x06C7	CPU_P4
INSN	-	vmptrst	NONE	vmxtwobytemem	0x07C7	CPU_P4
INSN	-	vmclear	NONE	vmxthreebytemem	0x0666C7 CPU_P4
INSN	-	vmxon	NONE	vmxthreebytemem	0x06F3C7 CPU_P4
INSN	-	cvtss2sd NONE	cvt_xmm_xmm32	0xF35A	CPU_SSE2
INSN	-	cvttpd2pi NONE	cvt_mm_xmm	0x662C	CPU_SSE2
INSN	-	cvttsd2si "lq"	cvt_rx_xmm64	0xF22C	CPU_SSE2
INSN	-	cvttpd2dq NONE	ssess	0x66E6		CPU_SSE2
INSN	-	cvttps2dq NONE	ssess	0xF35B		CPU_SSE2
INSN	-	maskmovdqu NONE	maskmovdqu	0	CPU_SSE2
INSN	-	movdqa	NONE	movdqau	0x66		CPU_SSE2
INSN	-	movdqu	NONE	movdqau	0xF3		CPU_SSE2
INSN	-	movdq2q	NONE	movdq2q	0		CPU_SSE2
INSN	-	movq2dq	NONE	movq2dq	0		CPU_SSE2
INSN	-	pmuludq	NONE	mmxsse2	0xF4		CPU_SSE2
INSN	-	pshufd	NONE	ssessimm	0x6670	CPU_SSE2
INSN	-	pshufhw	NONE	ssessimm	0xF370	CPU_SSE2
INSN	-	pshuflw	NONE	ssessimm	0xF270	CPU_SSE2
INSN	-	pslldq	NONE	pslrldq	0x07		CPU_SSE2
INSN	-	psrldq	NONE	pslrldq	0x03		CPU_SSE2
INSN	-	punpckhqdq NONE	ssess	0x666D		CPU_SSE2
INSN	-	punpcklqdq NONE	ssess	0x666C		CPU_SSE2
# SSE3 / PNI Prescott New Instructions instructions
INSN	-	addsubpd NONE	ssess	0x66D0		CPU_SSE3
INSN	-	addsubps NONE	ssess	0xF2D0		CPU_SSE3
INSN	-	fisttp	"lqs"	fildstp	0x010001	CPU_SSE3
INSN	gas	fisttpll SUF_Q	fildstp	0x07		CPU_FPU
INSN	-	haddpd	NONE	ssess	0x667C		CPU_SSE3
INSN	-	haddps	NONE	ssess	0xF27C		CPU_SSE3
INSN	-	hsubpd	NONE	ssess	0x667D		CPU_SSE3
INSN	-	hsubps	NONE	ssess	0xF27D		CPU_SSE3
INSN	-	lddqu	NONE	lddqu	0		CPU_SSE3
INSN	-	monitor	NONE	threebyte   0x0F01C8	CPU_SSE3
INSN	-	movddup	NONE	cvt_xmm_xmm64_ss 0xF212	CPU_SSE3
INSN	-	movshdup NONE	ssess	0xF316		CPU_SSE3
INSN	-	movsldup NONE	ssess	0xF312		CPU_SSE3
INSN	-	mwait	NONE	threebyte   0x0F01C9	CPU_SSE3
# AMD 3DNow! instructions
INSN	-	prefetch NONE	twobytemem  0x000F0D	CPU_3DNow
INSN	-	prefetchw NONE	twobytemem  0x010F0D	CPU_3DNow
INSN	-	femms	NONE	twobyte	0x0F0E		CPU_3DNow
INSN	-	pavgusb	NONE	now3d	0xBF		CPU_3DNow
INSN	-	pf2id	NONE	now3d	0x1D		CPU_3DNow
INSN	-	pf2iw	NONE	now3d	0x1C		CPU_Athlon|CPU_3DNow
INSN	-	pfacc	NONE	now3d	0xAE		CPU_3DNow
INSN	-	pfadd	NONE	now3d	0x9E		CPU_3DNow
INSN	-	pfcmpeq	NONE	now3d	0xB0		CPU_3DNow
INSN	-	pfcmpge NONE	now3d	0x90		CPU_3DNow
INSN	-	pfcmpgt	NONE	now3d	0xA0		CPU_3DNow
INSN	-	pfmax	NONE	now3d	0xA4		CPU_3DNow
INSN	-	pfmin	NONE	now3d	0x94		CPU_3DNow
INSN	-	pfmul	NONE	now3d	0xB4		CPU_3DNow
INSN	-	pfnacc	NONE	now3d	0x8A		CPU_Athlon|CPU_3DNow
INSN	-	pfpnacc	NONE	now3d	0x8E		CPU_Athlon|CPU_3DNow
INSN	-	pfrcp	NONE	now3d	0x96		CPU_3DNow
INSN	-	pfrcpit1 NONE	now3d	0xA6		CPU_3DNow
INSN	-	pfrcpit2 NONE	now3d	0xB6		CPU_3DNow
INSN	-	pfrsqit1 NONE	now3d	0xA7		CPU_3DNow
INSN	-	pfrsqrt	NONE	now3d	0x97		CPU_3DNow
INSN	-	pfsub	NONE	now3d	0x9A		CPU_3DNow
INSN	-	pfsubr	NONE	now3d	0xAA		CPU_3DNow
INSN	-	pi2fd	NONE	now3d	0x0D		CPU_3DNow
INSN	-	pi2fw	NONE	now3d	0x0C		CPU_Athlon|CPU_3DNow
INSN	-	pmulhrwa NONE	now3d	0xB7		CPU_3DNow
INSN	-	pswapd	NONE	now3d	0xBB		CPU_Athlon|CPU_3DNow
# AMD extensions
INSN	-	syscall	NONE	twobyte	0x0F05		CPU_686|CPU_AMD
INSN	-	sysret	"lq"	twobyte	0x0F07		CPU_686|CPU_AMD|CPU_Priv
# AMD x86-64 extensions
INSN	-	swapgs	NONE	threebyte   0x0F01F8	CPU_Hammer|CPU_64
INSN	-	rdtscp	NONE	threebyte   0x0F01F9	CPU_686|CPU_AMD|CPU_Priv
# AMD Pacifica SVM instructions
INSN	-	clgi	NONE	threebyte   0x0F01DD	CPU_Hammer|CPU_64|CPU_SVM
INSN	-	invlpga	NONE	invlpga	    0		CPU_Hammer|CPU_64|CPU_SVM
INSN	-	skinit	NONE	skinit	    0		CPU_Hammer|CPU_64|CPU_SVM
INSN	-	stgi	NONE	threebyte   0x0F01DC	CPU_Hammer|CPU_64|CPU_SVM
INSN	-	vmload	NONE	svm_rax	    0xDA	CPU_Hammer|CPU_64|CPU_SVM
INSN	-	vmmcall	NONE	threebyte   0x0F01D9	CPU_Hammer|CPU_64|CPU_SVM
INSN	-	vmrun	NONE	svm_rax	    0xD8	CPU_Hammer|CPU_64|CPU_SVM
INSN	-	vmsave	NONE	svm_rax	    0xDB	CPU_Hammer|CPU_64|CPU_SVM
# VIA PadLock instructions
INSN	-	xstore	NONE	padlock	0xC000A7   CPU_PadLock
INSN	-	xstorerng NONE	padlock	0xC000A7   CPU_PadLock
INSN	-	xcryptecb NONE	padlock	0xC8F3A7   CPU_PadLock
INSN	-	xcryptcbc NONE	padlock	0xD0F3A7   CPU_PadLock
INSN	-	xcryptctr NONE	padlock	0xD8F3A7   CPU_PadLock
INSN	-	xcryptcfb NONE	padlock	0xE0F3A7   CPU_PadLock
INSN	-	xcryptofb NONE	padlock	0xE8F3A7   CPU_PadLock
INSN	-	montmul	NONE	padlock	0xC0F3A6   CPU_PadLock
INSN	-	xsha1	NONE	padlock	0xC8F3A6   CPU_PadLock
INSN	-	xsha256	NONE	padlock	0xD0F3A6   CPU_PadLock
# Cyrix MMX instructions
INSN	-	paddsiw	NONE	cyrixmmx    0x51	CPU_Cyrix|CPU_MMX
INSN	-	paveb	NONE	cyrixmmx    0x50	CPU_Cyrix|CPU_MMX
INSN	-	pdistib	NONE	cyrixmmx    0x54	CPU_Cyrix|CPU_MMX
INSN	-	pmachriw NONE	pmachriw    0		CPU_Cyrix|CPU_MMX
INSN	-	pmagw	NONE	cyrixmmx    0x52	CPU_Cyrix|CPU_MMX
INSN	-	pmulhriw NONE	cyrixmmx    0x5D	CPU_Cyrix|CPU_MMX
INSN	-	pmulhrwc NONE	cyrixmmx    0x59	CPU_Cyrix|CPU_MMX
INSN	-	pmvgezb	NONE	cyrixmmx    0x5C	CPU_Cyrix|CPU_MMX
INSN	-	pmvlzb	NONE	cyrixmmx    0x5B	CPU_Cyrix|CPU_MMX
INSN	-	pmvnzb	NONE	cyrixmmx    0x5A	CPU_Cyrix|CPU_MMX
INSN	-	pmvzb	NONE	cyrixmmx    0x58	CPU_Cyrix|CPU_MMX
INSN	-	psubsiw	NONE	cyrixmmx    0x55	CPU_Cyrix|CPU_MMX
# Cyrix extensions
INSN	-	rdshr	NONE	twobyte	    0x0F36	CPU_686|CPU_Cyrix|CPU_SMM
INSN	-	rsdc	NONE	rsdc	    0		CPU_486|CPU_Cyrix|CPU_SMM
INSN	-	rsldt	NONE	cyrixsmm    0x7B	CPU_486|CPU_Cyrix|CPU_SMM
INSN	-	rsts	NONE	cyrixsmm    0x7D	CPU_486|CPU_Cyrix|CPU_SMM
INSN	-	svdc	NONE	svdc	    0		CPU_486|CPU_Cyrix|CPU_SMM
INSN	-	svldt	NONE	cyrixsmm    0x7A	CPU_486|CPU_Cyrix|CPU_SMM
INSN	-	svts	NONE	cyrixsmm    0x7C	CPU_486|CPU_Cyrix|CPU_SMM
INSN	-	smint	NONE	twobyte	    0x0F38	CPU_686|CPU_Cyrix
INSN	-	smintold NONE	twobyte	    0x0F7E	CPU_486|CPU_Cyrix|CPU_Obs
INSN	-	wrshr	NONE	twobyte	    0x0F37	CPU_686|CPU_Cyrix|CPU_SMM
# Obsolete/undocumented instructions
INSN	-	fsetpm	NONE	twobyte	0xDBE4		CPU_286|CPU_FPU|CPU_Obs
INSN	-	ibts	NONE	ibts	0		CPU_386|CPU_Undoc|CPU_Obs
INSN	-	loadall	NONE	twobyte	0x0F07		CPU_386|CPU_Undoc
INSN	-	loadall286 NONE	twobyte	0x0F05		CPU_286|CPU_Undoc
INSN	-	salc	NONE	onebyte	0x00D6		CPU_Undoc|CPU_Not64
INSN	-	smi	NONE	onebyte	0x00F1		CPU_386|CPU_Undoc
INSN	-	umov	NONE	umov	0		CPU_386|CPU_Undoc
INSN	-	xbts	NONE	xbts	0		CPU_386|CPU_Undoc|CPU_Obs


# DEF_CPU parameters:
# - CPU name
# - CPU flags to set
# DEF_CPU_ALIAS parameters:
# - CPU alias name
# - CPU base name
# DEF_CPU_FEATURE parameters:
# - CPU feature name
# - CPU flag to set feature name alone or unset ("no" + feature name)

# The standard CPU names /set/ cpu_enabled.
CPU		8086	CPU_Priv
CPU		186	CPU_186|CPU_Priv
CPU_ALIAS	80186		186
CPU_ALIAS	i186		186
CPU		286	CPU_186|CPU_286|CPU_Priv
CPU_ALIAS	80286		286
CPU_ALIAS	i286		286
CPU		386	CPU_186|CPU_286|CPU_386|CPU_SMM|CPU_Prot|CPU_Priv
CPU_ALIAS	80386		386
CPU_ALIAS	i386		386
CPU		486	CPU_186|CPU_286|CPU_386|CPU_486|CPU_FPU|CPU_SMM|\
			CPU_Prot|CPU_Priv
CPU_ALIAS	80486		486
CPU_ALIAS	i486		486
CPU		586	CPU_186|CPU_286|CPU_386|CPU_486|CPU_586|CPU_FPU|\
			CPU_SMM|CPU_Prot|CPU_Priv
CPU_ALIAS	i586		586
CPU_ALIAS	pentium		586
CPU_ALIAS	p5		586
CPU		686	CPU_186|CPU_286|CPU_386|CPU_486|CPU_586|CPU_686|\
			CPU_FPU|CPU_SMM|CPU_Prot|CPU_Priv
CPU_ALIAS	i686		686
CPU_ALIAS	p6		686
CPU_ALIAS	ppro		686
CPU_ALIAS	pentiumpro	686
CPU		p2	CPU_186|CPU_286|CPU_386|CPU_486|CPU_586|CPU_686|\
			CPU_FPU|CPU_MMX|CPU_SMM|CPU_Prot|CPU_Priv
CPU_ALIAS	pentium2	p2
CPU_ALIAS	pentium-2	p2
CPU_ALIAS	pentiumii	p2
CPU_ALIAS	pentium-ii	p2
CPU		p3	CPU_186|CPU_286|CPU_386|CPU_486|CPU_586|CPU_686|\
			CPU_P3|CPU_FPU|CPU_MMX|CPU_SSE|CPU_SMM|CPU_Prot|\
			CPU_Priv
CPU_ALIAS	pentium3	p3
CPU_ALIAS	pentium-3	p3
CPU_ALIAS	pentiumiii	p3
CPU_ALIAS	pentium-iii	p3
CPU_ALIAS	katmai	p3
CPU		p4	CPU_186|CPU_286|CPU_386|CPU_486|CPU_586|CPU_686|\
			CPU_P3|CPU_P4|CPU_FPU|CPU_MMX|CPU_SSE|CPU_SSE2|\
			CPU_SMM|CPU_Prot|CPU_Priv
CPU_ALIAS	pentium4	p4
CPU_ALIAS	pentium-4	p4
CPU_ALIAS	pentiumiv	p4
CPU_ALIAS	pentium-iv	p4
CPU_ALIAS	williamette	p4
CPU		ia64	CPU_186|CPU_286|CPU_386|CPU_486|CPU_586|CPU_686|\
			CPU_P3|CPU_P4|CPU_IA64|CPU_FPU|CPU_MMX|CPU_SSE|\
			CPU_SSE2|CPU_SMM|CPU_Prot|CPU_Priv
CPU_ALIAS	ia-64		ia64
CPU_ALIAS	itanium		ia64
CPU		k6	CPU_186|CPU_286|CPU_386|CPU_486|CPU_586|CPU_686|\
			CPU_K6|CPU_FPU|CPU_MMX|CPU_3DNow|CPU_SMM|CPU_Prot|\
			CPU_Priv
CPU		k7	CPU_186|CPU_286|CPU_386|CPU_486|CPU_586|CPU_686|\
			CPU_K6|CPU_Athlon|CPU_FPU|CPU_MMX|CPU_SSE|CPU_3DNow|\
			CPU_SMM|CPU_Prot|CPU_Priv
CPU_ALIAS	athlon		k7
CPU		hammer	CPU_186|CPU_286|CPU_386|CPU_486|CPU_586|CPU_686|\
			CPU_K6|CPU_Athlon|CPU_Hammer|CPU_FPU|CPU_MMX|\
			CPU_SSE|CPU_SSE2|CPU_3DNow|CPU_SMM|CPU_Prot|\
			CPU_Priv
CPU_ALIAS	sledgehammer	hammer
CPU_ALIAS	opteron		hammer
CPU_ALIAS	athlon64	hammer
CPU_ALIAS	athlon-64	hammer
CPU		prescott CPU_186|CPU_286|CPU_386|CPU_486|CPU_586|CPU_686|\
			CPU_K6|CPU_Athlon|CPU_Hammer|CPU_EM64T|CPU_FPU|CPU_MMX|\
			CPU_SSE|CPU_SSE2|CPU_SSE3|CPU_3DNow|CPU_SMM|\
			CPU_Prot|CPU_Priv

# Features have "no" versions to disable them, and only set/reset the
# specific feature being changed.  All other bits are left alone.

CPU_FEATURE	fpu	CPU_FPU
CPU_FEATURE	mmx	CPU_MMX
CPU_FEATURE	sse	CPU_SSE
CPU_FEATURE	sse2	CPU_SSE2
CPU_FEATURE	sse3	CPU_SSE3
#CPU_FEATURE	pni	CPU_PNI
CPU_FEATURE	3dnow	CPU_3DNow
CPU_FEATURE	cyrix	CPU_Cyrix
CPU_FEATURE	amd	CPU_AMD
CPU_FEATURE	smm	CPU_SMM
CPU_FEATURE	prot	CPU_Prot
CPU_FEATURE	undoc	CPU_Undoc
CPU_FEATURE	obs	CPU_Obs
CPU_FEATURE	priv	CPU_Priv
CPU_FEATURE	svm	CPU_SVM
CPU_FEATURE	padlock	CPU_PadLock
CPU_FEATURE	em64t	CPU_EM64T


# TARGETMOD parameters:
# - target modifier name
# - modifier to return

TARGETMOD	near	X86_NEAR
TARGETMOD	short	X86_SHORT
TARGETMOD	far	X86_FAR
TARGETMOD	to	X86_TO


# PREFIX parameters:
# - parser
# - prefix name
# - prefix type
# - prefix value

# operand size overrides
PREFIX	nasm	o16	X86_OPERSIZE	16
PREFIX	gas	data16	X86_OPERSIZE	16
PREFIX	gas	word	X86_OPERSIZE	16
PREFIX	nasm	o32	X86_OPERSIZE	32
PREFIX	gas	data32	X86_OPERSIZE	32
PREFIX	gas	dword	X86_OPERSIZE	32
PREFIX	nasm	o64	X86_OPERSIZE	64
PREFIX	gas	data64	X86_OPERSIZE	64
PREFIX	gas	qword	X86_OPERSIZE	64

# address size overrides
PREFIX	nasm	a16	X86_ADDRSIZE	16
PREFIX	gas	addr16	X86_ADDRSIZE	16
PREFIX	gas	aword	X86_ADDRSIZE	16
PREFIX	nasm	a32	X86_ADDRSIZE	32
PREFIX	gas	addr32	X86_ADDRSIZE	32
PREFIX	gas	adword	X86_ADDRSIZE	32
PREFIX	nasm	a64	X86_ADDRSIZE	64
PREFIX	gas	addr64	X86_ADDRSIZE	64
PREFIX	gas	aqword	X86_ADDRSIZE	64

# instruction prefixes
PREFIX	-	lock	X86_LOCKREP	0xF0
PREFIX	-	repne	X86_LOCKREP	0xF2
PREFIX	-	repnz	X86_LOCKREP	0xF2
PREFIX	-	rep	X86_LOCKREP	0xF3
PREFIX	-	repe	X86_LOCKREP	0xF3
PREFIX	-	repz	X86_LOCKREP	0xF3

# other prefixes, limited to GAS-only at the moment
# Hint taken/not taken for jumps
PREFIX	gas	ht	X86_SEGREG	0x3E
PREFIX	gas	hnt	X86_SEGREG	0x2E

# REX byte explicit prefixes
PREFIX	gas	rex	X86_REX		0x40
PREFIX	gas	rexz	X86_REX		0x41
PREFIX	gas	rexy	X86_REX		0x42
PREFIX	gas	rexyz	X86_REX		0x43
PREFIX	gas	rexx	X86_REX		0x44
PREFIX	gas	rexxz	X86_REX		0x45
PREFIX	gas	rexxy	X86_REX		0x46
PREFIX	gas	rexxyz	X86_REX		0x47
PREFIX	gas	rex64	X86_REX		0x48
PREFIX	gas	rex64z	X86_REX		0x49
PREFIX	gas	rex64y	X86_REX		0x4A
PREFIX	gas	rex64yz	X86_REX		0x4B
PREFIX	gas	rex64x	X86_REX		0x4C
PREFIX	gas	rex64xz	X86_REX		0x4D
PREFIX	gas	rex64xy	X86_REX		0x4E
PREFIX	gas	rex64xyz X86_REX	0x4F


# REG parameters:
# - register name
# - register type
# - register index
# - required BITS setting (0 for any)
#
# REGGROUP parameters:
# - register group name
# - register group type
#
# SEGREG parameters:
# - segment register name
# - prefix encoding
# - register encoding
# - BITS in which the segment is ignored

# control, debug, and test registers
REG	cr0	X86_CRREG	0	0
REG	cr2	X86_CRREG	2	0
REG	cr3	X86_CRREG	3	0
REG	cr4	X86_CRREG	4	0
REG	cr8	X86_CRREG	8	64

REG	dr0	X86_DRREG	0	0
REG	dr1	X86_DRREG	1	0
REG	dr2	X86_DRREG	2	0
REG	dr3	X86_DRREG	3	0
REG	dr4	X86_DRREG	4	0
REG	dr5	X86_DRREG	5	0
REG	dr6	X86_DRREG	6	0
REG	dr7	X86_DRREG	7	0

REG	tr0	X86_TRREG	0	0
REG	tr1	X86_TRREG	1	0
REG	tr2	X86_TRREG	2	0
REG	tr3	X86_TRREG	3	0
REG	tr4	X86_TRREG	4	0
REG	tr5	X86_TRREG	5	0
REG	tr6	X86_TRREG	6	0
REG	tr7	X86_TRREG	7	0

# floating point, MMX, and SSE/SSE2 registers
REG	st0	X86_FPUREG	0	0
REG	st1	X86_FPUREG	1	0
REG	st2	X86_FPUREG	2	0
REG	st3	X86_FPUREG	3	0
REG	st4	X86_FPUREG	4	0
REG	st5	X86_FPUREG	5	0
REG	st6	X86_FPUREG	6	0
REG	st7	X86_FPUREG	7	0

REG	mm0	X86_MMXREG	0	0
REG	mm1	X86_MMXREG	1	0
REG	mm2	X86_MMXREG	2	0
REG	mm3	X86_MMXREG	3	0
REG	mm4	X86_MMXREG	4	0
REG	mm5	X86_MMXREG	5	0
REG	mm6	X86_MMXREG	6	0
REG	mm7	X86_MMXREG	7	0

REG	xmm0	X86_XMMREG	0	0
REG	xmm1	X86_XMMREG	1	0
REG	xmm2	X86_XMMREG	2	0
REG	xmm3	X86_XMMREG	3	0
REG	xmm4	X86_XMMREG	4	0
REG	xmm5	X86_XMMREG	5	0
REG	xmm6	X86_XMMREG	6	0
REG	xmm7	X86_XMMREG	7	0
REG	xmm8	X86_XMMREG	8	64
REG	xmm9	X86_XMMREG	9	64
REG	xmm10	X86_XMMREG	10	64
REG	xmm11	X86_XMMREG	11	64
REG	xmm12	X86_XMMREG	12	64
REG	xmm13	X86_XMMREG	13	64
REG	xmm14	X86_XMMREG	14	64
REG	xmm15	X86_XMMREG	15	64

# integer registers
REG	rax	X86_REG64	0	64
REG	rcx	X86_REG64	1	64
REG	rdx	X86_REG64	2	64
REG	rbx	X86_REG64	3	64
REG	rsp	X86_REG64	4	64
REG	rbp	X86_REG64	5	64
REG	rsi	X86_REG64	6	64
REG	rdi	X86_REG64	7	64
REG	r8	X86_REG64	8	64
REG	r9	X86_REG64	9	64
REG	r10	X86_REG64	10	64
REG	r11	X86_REG64	11	64
REG	r12	X86_REG64	12	64
REG	r13	X86_REG64	13	64
REG	r14	X86_REG64	14	64
REG	r15	X86_REG64	15	64

REG	eax	X86_REG32	0	0
REG	ecx	X86_REG32	1	0
REG	edx	X86_REG32	2	0
REG	ebx	X86_REG32	3	0
REG	esp	X86_REG32	4	0
REG	ebp	X86_REG32	5	0
REG	esi	X86_REG32	6	0
REG	edi	X86_REG32	7	0
REG	r8d	X86_REG32	8	64
REG	r9d	X86_REG32	9	64
REG	r10d	X86_REG32	10	64
REG	r11d	X86_REG32	11	64
REG	r12d	X86_REG32	12	64
REG	r13d	X86_REG32	13	64
REG	r14d	X86_REG32	14	64
REG	r15d	X86_REG32	15	64

REG	ax	X86_REG16	0	0
REG	cx	X86_REG16	1	0
REG	dx	X86_REG16	2	0
REG	bx	X86_REG16	3	0
REG	sp	X86_REG16	4	0
REG	bp	X86_REG16	5	0
REG	si	X86_REG16	6	0
REG	di	X86_REG16	7	0
REG	r8w	X86_REG16	8	64
REG	r9w	X86_REG16	9	64
REG	r10w	X86_REG16	10	64
REG	r11w	X86_REG16	11	64
REG	r12w	X86_REG16	12	64
REG	r13w	X86_REG16	13	64
REG	r14w	X86_REG16	14	64
REG	r15w	X86_REG16	15	64

REG	al	X86_REG8	0	0
REG	cl	X86_REG8	1	0
REG	dl	X86_REG8	2	0
REG	bl	X86_REG8	3	0
REG	ah	X86_REG8	4	0
REG	ch	X86_REG8	5	0
REG	dh	X86_REG8	6	0
REG	bh	X86_REG8	7	0
REG	r8b	X86_REG8	8	64
REG	r9b	X86_REG8	9	64
REG	r10b	X86_REG8	10	64
REG	r11b	X86_REG8	11	64
REG	r12b	X86_REG8	12	64
REG	r13b	X86_REG8	13	64
REG	r14b	X86_REG8	14	64
REG	r15b	X86_REG8	15	64

REG	spl	X86_REG8X	4	64
REG	bpl	X86_REG8X	5	64
REG	sil	X86_REG8X	6	64
REG	dil	X86_REG8X	7	64

REG	rip	X86_RIP		0	64

# floating point, MMX, and SSE/SSE2 registers
REGGROUP	st	X86_FPUREG
REGGROUP	mm	X86_MMXREG
REGGROUP	xmm	X86_XMMREG

# segment registers
SEGREG	es	0x26	0x00	64
SEGREG	cs	0x2e	0x01	0
SEGREG	ss	0x36	0x02	64
SEGREG	ds	0x3e	0x03	64
SEGREG	fs	0x64	0x04	0
SEGREG	gs	0x65	0x05	0

