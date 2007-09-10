static const x86_insn_info onebyte_insn[] = {
    { CPU_Any, MOD_Op0Add|MOD_OpSizeR|MOD_DOpS64R, 0, 0, 0, 1, {0x00, 0, 0}, 0, 0, {0, 0, 0} }
};

static const x86_insn_info onebyte_prefix_insn[] = {
    { CPU_Any, MOD_Op0Add|MOD_PreAdd, 0, 0, 0x00, 1, {0x00, 0, 0}, 0, 0, {0, 0, 0} }
};

static const x86_insn_info twobyte_insn[] = {
    { CPU_Any, MOD_Op1Add|MOD_Op0Add|MOD_GasSufL|MOD_GasSufQ, 0, 0, 0, 2, {0x00, 0x00, 0}, 0, 0, {0, 0, 0} }
};

static const x86_insn_info threebyte_insn[] = {
    { CPU_Any, MOD_Op2Add|MOD_Op1Add|MOD_Op0Add, 0, 0, 0, 3, {0x00, 0x00, 0x00}, 0, 0, {0, 0, 0} }
};

static const x86_insn_info onebytemem_insn[] = {
    { CPU_Any, MOD_Op0Add|MOD_SpAdd|MOD_GasSufL|MOD_GasSufQ|MOD_GasSufS, 0, 0, 0, 1, {0x00, 0, 0}, 0, 1, {OPT_Mem|OPS_Any|OPA_EA, 0, 0} }
};

static const x86_insn_info twobytemem_insn[] = {
    { CPU_Any, MOD_Op1Add|MOD_Op0Add|MOD_SpAdd|MOD_GasSufL|MOD_GasSufQ|MOD_GasSufS|MOD_GasSufW, 0, 0, 0, 2, {0x00, 0x00, 0}, 0, 1, {OPT_Mem|OPS_Any|OPA_EA, 0, 0} }
};

static const x86_insn_info mov_insn[] = {
    { CPU_Not64, MOD_GasSufB, 0, 0, 0, 1, {0xA0, 0, 0}, 0, 2, {OPT_Areg|OPS_8|OPA_None, OPT_MemOffs|OPS_8|OPS_Relaxed|OPA_EA, 0} },
    { CPU_Not64, MOD_GasSufW, 16, 0, 0, 1, {0xA1, 0, 0}, 0, 2, {OPT_Areg|OPS_16|OPA_None, OPT_MemOffs|OPS_16|OPS_Relaxed|OPA_EA, 0} },
    { CPU_386|CPU_Not64, MOD_GasSufL, 32, 0, 0, 1, {0xA1, 0, 0}, 0, 2, {OPT_Areg|OPS_32|OPA_None, OPT_MemOffs|OPS_32|OPS_Relaxed|OPA_EA, 0} },
    { CPU_Not64, MOD_GasSufB, 0, 0, 0, 1, {0xA2, 0, 0}, 0, 2, {OPT_MemOffs|OPS_8|OPS_Relaxed|OPA_EA, OPT_Areg|OPS_8|OPA_None, 0} },
    { CPU_Not64, MOD_GasSufW, 16, 0, 0, 1, {0xA3, 0, 0}, 0, 2, {OPT_MemOffs|OPS_16|OPS_Relaxed|OPA_EA, OPT_Areg|OPS_16|OPA_None, 0} },
    { CPU_386|CPU_Not64, MOD_GasSufL, 32, 0, 0, 1, {0xA3, 0, 0}, 0, 2, {OPT_MemOffs|OPS_32|OPS_Relaxed|OPA_EA, OPT_Areg|OPS_32|OPA_None, 0} },
    { CPU_64, 0, 0, 0, 0, 1, {0xA0, 0, 0}, 0, 2, {OPT_Areg|OPS_8|OPA_None, OPT_MemOffs|OPS_8|OPS_Relaxed|OPEAS_64|OPA_EA, 0} },
    { CPU_64, 0, 16, 0, 0, 1, {0xA1, 0, 0}, 0, 2, {OPT_Areg|OPS_16|OPA_None, OPT_MemOffs|OPS_16|OPS_Relaxed|OPEAS_64|OPA_EA, 0} },
    { CPU_64, 0, 32, 0, 0, 1, {0xA1, 0, 0}, 0, 2, {OPT_Areg|OPS_32|OPA_None, OPT_MemOffs|OPS_32|OPS_Relaxed|OPEAS_64|OPA_EA, 0} },
    { CPU_64, 0, 64, 0, 0, 1, {0xA1, 0, 0}, 0, 2, {OPT_Areg|OPS_64|OPA_None, OPT_MemOffs|OPS_64|OPS_Relaxed|OPEAS_64|OPA_EA, 0} },
    { CPU_64, 0, 0, 0, 0, 1, {0xA2, 0, 0}, 0, 2, {OPT_MemOffs|OPS_8|OPS_Relaxed|OPEAS_64|OPA_EA, OPT_Areg|OPS_8|OPA_None, 0} },
    { CPU_64, 0, 16, 0, 0, 1, {0xA3, 0, 0}, 0, 2, {OPT_MemOffs|OPS_16|OPS_Relaxed|OPEAS_64|OPA_EA, OPT_Areg|OPS_16|OPA_None, 0} },
    { CPU_64, 0, 32, 0, 0, 1, {0xA3, 0, 0}, 0, 2, {OPT_MemOffs|OPS_32|OPS_Relaxed|OPEAS_64|OPA_EA, OPT_Areg|OPS_32|OPA_None, 0} },
    { CPU_64, 0, 64, 0, 0, 1, {0xA3, 0, 0}, 0, 2, {OPT_MemOffs|OPS_64|OPS_Relaxed|OPEAS_64|OPA_EA, OPT_Areg|OPS_64|OPA_None, 0} },
    { CPU_Any, MOD_GasSufB, 0, 0, 0, 1, {0x88, 0xA2, 0}, 0, 2, {OPT_RM|OPS_8|OPS_Relaxed|OPA_EA|OPAP_ShortMov, OPT_Areg|OPS_8|OPA_Spare, 0} },
    { CPU_Any, MOD_GasSufW, 16, 0, 0, 1, {0x89, 0xA3, 0}, 0, 2, {OPT_RM|OPS_16|OPS_Relaxed|OPA_EA|OPAP_ShortMov, OPT_Areg|OPS_16|OPA_Spare, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0x89, 0xA3, 0}, 0, 2, {OPT_RM|OPS_32|OPS_Relaxed|OPA_EA|OPAP_ShortMov, OPT_Areg|OPS_32|OPA_Spare, 0} },
    { CPU_64, MOD_GasSufQ, 64, 0, 0, 1, {0x89, 0xA3, 0}, 0, 2, {OPT_RM|OPS_64|OPS_Relaxed|OPA_EA|OPAP_ShortMov, OPT_Areg|OPS_64|OPA_Spare, 0} },
    { CPU_Any, MOD_GasSufB, 0, 0, 0, 1, {0x88, 0, 0}, 0, 2, {OPT_RM|OPS_8|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_8|OPA_Spare, 0} },
    { CPU_Any, MOD_GasSufW, 16, 0, 0, 1, {0x89, 0, 0}, 0, 2, {OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_16|OPA_Spare, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0x89, 0, 0}, 0, 2, {OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_32|OPA_Spare, 0} },
    { CPU_64, MOD_GasSufQ, 64, 0, 0, 1, {0x89, 0, 0}, 0, 2, {OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_64|OPA_Spare, 0} },
    { CPU_Any, MOD_GasSufB, 0, 0, 0, 1, {0x8A, 0xA0, 0}, 0, 2, {OPT_Areg|OPS_8|OPA_Spare, OPT_RM|OPS_8|OPS_Relaxed|OPA_EA|OPAP_ShortMov, 0} },
    { CPU_Any, MOD_GasSufW, 16, 0, 0, 1, {0x8B, 0xA1, 0}, 0, 2, {OPT_Areg|OPS_16|OPA_Spare, OPT_RM|OPS_16|OPS_Relaxed|OPA_EA|OPAP_ShortMov, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0x8B, 0xA1, 0}, 0, 2, {OPT_Areg|OPS_32|OPA_Spare, OPT_RM|OPS_32|OPS_Relaxed|OPA_EA|OPAP_ShortMov, 0} },
    { CPU_64, MOD_GasSufQ, 64, 0, 0, 1, {0x8B, 0xA1, 0}, 0, 2, {OPT_Areg|OPS_64|OPA_Spare, OPT_RM|OPS_64|OPS_Relaxed|OPA_EA|OPAP_ShortMov, 0} },
    { CPU_Any, MOD_GasSufB, 0, 0, 0, 1, {0x8A, 0, 0}, 0, 2, {OPT_Reg|OPS_8|OPA_Spare, OPT_RM|OPS_8|OPS_Relaxed|OPA_EA, 0} },
    { CPU_Any, MOD_GasSufW, 16, 0, 0, 1, {0x8B, 0, 0}, 0, 2, {OPT_Reg|OPS_16|OPA_Spare, OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0x8B, 0, 0}, 0, 2, {OPT_Reg|OPS_32|OPA_Spare, OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, 0} },
    { CPU_64, MOD_GasSufQ, 64, 0, 0, 1, {0x8B, 0, 0}, 0, 2, {OPT_Reg|OPS_64|OPA_Spare, OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, 0} },
    { CPU_Any, MOD_GasSufW, 0, 0, 0, 1, {0x8C, 0, 0}, 0, 2, {OPT_Mem|OPS_16|OPS_Relaxed|OPA_EA, OPT_SegReg|OPS_16|OPS_Relaxed|OPA_Spare, 0} },
    { CPU_Any, MOD_GasSufW, 16, 0, 0, 1, {0x8C, 0, 0}, 0, 2, {OPT_Reg|OPS_16|OPA_EA, OPT_SegReg|OPS_16|OPS_Relaxed|OPA_Spare, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0x8C, 0, 0}, 0, 2, {OPT_Reg|OPS_32|OPA_EA, OPT_SegReg|OPS_16|OPS_Relaxed|OPA_Spare, 0} },
    { CPU_64, MOD_GasSufQ, 64, 0, 0, 1, {0x8C, 0, 0}, 0, 2, {OPT_Reg|OPS_64|OPA_EA, OPT_SegReg|OPS_16|OPS_Relaxed|OPA_Spare, 0} },
    { CPU_Any, MOD_GasSufW, 0, 0, 0, 1, {0x8E, 0, 0}, 0, 2, {OPT_SegReg|OPS_16|OPS_Relaxed|OPA_Spare, OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, 0} },
    { CPU_386, MOD_GasSufL, 0, 0, 0, 1, {0x8E, 0, 0}, 0, 2, {OPT_SegReg|OPS_16|OPS_Relaxed|OPA_Spare, OPT_Reg|OPS_32|OPA_EA, 0} },
    { CPU_64, MOD_GasSufQ, 0, 0, 0, 1, {0x8E, 0, 0}, 0, 2, {OPT_SegReg|OPS_16|OPS_Relaxed|OPA_Spare, OPT_Reg|OPS_64|OPA_EA, 0} },
    { CPU_Any, MOD_GasSufB, 0, 0, 0, 1, {0xB0, 0, 0}, 0, 2, {OPT_Reg|OPS_8|OPA_Op0Add, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_Any, MOD_GasSufW, 16, 0, 0, 1, {0xB8, 0, 0}, 0, 2, {OPT_Reg|OPS_16|OPA_Op0Add, OPT_Imm|OPS_16|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0xB8, 0, 0}, 0, 2, {OPT_Reg|OPS_32|OPA_Op0Add, OPT_Imm|OPS_32|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_64, MOD_GasIllegal, 64, 0, 0, 1, {0xB8, 0, 0}, 0, 2, {OPT_Reg|OPS_64|OPA_Op0Add, OPT_Imm|OPS_64|OPA_Imm, 0} },
    { CPU_64, MOD_GasSufQ, 64, 0, 0, 1, {0xB8, 0xC7, 0}, 0, 2, {OPT_Reg|OPS_64|OPA_Op0Add, OPT_Imm|OPS_64|OPS_Relaxed|OPA_Imm|OPAP_SImm32Avail, 0} },
    { CPU_Any, MOD_GasSufB, 0, 0, 0, 1, {0xC6, 0, 0}, 0, 2, {OPT_RM|OPS_8|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_8|OPA_Imm, 0} },
    { CPU_Any, MOD_GasSufW, 16, 0, 0, 1, {0xC7, 0, 0}, 0, 2, {OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_16|OPA_Imm, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0xC7, 0, 0}, 0, 2, {OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_32|OPA_Imm, 0} },
    { CPU_64, MOD_GasSufQ, 64, 0, 0, 1, {0xC7, 0, 0}, 0, 2, {OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_32|OPA_Imm, 0} },
    { CPU_Any, MOD_GasSufB, 0, 0, 0, 1, {0xC6, 0, 0}, 0, 2, {OPT_RM|OPS_8|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_Any, MOD_GasSufW, 16, 0, 0, 1, {0xC7, 0, 0}, 0, 2, {OPT_RM|OPS_16|OPA_EA, OPT_Imm|OPS_16|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0xC7, 0, 0}, 0, 2, {OPT_RM|OPS_32|OPA_EA, OPT_Imm|OPS_32|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_64, MOD_GasSufQ, 64, 0, 0, 1, {0xC7, 0, 0}, 0, 2, {OPT_RM|OPS_64|OPA_EA, OPT_Imm|OPS_32|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_586|CPU_Not64|CPU_Priv, MOD_GasSufL, 0, 0, 0, 2, {0x0F, 0x22, 0}, 0, 2, {OPT_CR4|OPS_32|OPA_Spare, OPT_Reg|OPS_32|OPA_EA, 0} },
    { CPU_386|CPU_Not64|CPU_Priv, MOD_GasSufL, 0, 0, 0, 2, {0x0F, 0x22, 0}, 0, 2, {OPT_CRReg|OPS_32|OPA_Spare, OPT_Reg|OPS_32|OPA_EA, 0} },
    { CPU_64|CPU_Priv, MOD_GasSufQ, 0, 0, 0, 2, {0x0F, 0x22, 0}, 0, 2, {OPT_CRReg|OPS_32|OPA_Spare, OPT_Reg|OPS_64|OPA_EA, 0} },
    { CPU_586|CPU_Not64|CPU_Priv, MOD_GasSufL, 0, 0, 0, 2, {0x0F, 0x20, 0}, 0, 2, {OPT_Reg|OPS_32|OPA_EA, OPT_CR4|OPS_32|OPA_Spare, 0} },
    { CPU_386|CPU_Not64|CPU_Priv, MOD_GasSufL, 0, 0, 0, 2, {0x0F, 0x20, 0}, 0, 2, {OPT_Reg|OPS_32|OPA_EA, OPT_CRReg|OPS_32|OPA_Spare, 0} },
    { CPU_64|CPU_Priv, MOD_GasSufQ, 0, 0, 0, 2, {0x0F, 0x20, 0}, 0, 2, {OPT_Reg|OPS_64|OPA_EA, OPT_CRReg|OPS_32|OPA_Spare, 0} },
    { CPU_386|CPU_Not64|CPU_Priv, MOD_GasSufL, 0, 0, 0, 2, {0x0F, 0x23, 0}, 0, 2, {OPT_DRReg|OPS_32|OPA_Spare, OPT_Reg|OPS_32|OPA_EA, 0} },
    { CPU_64|CPU_Priv, MOD_GasSufQ, 0, 0, 0, 2, {0x0F, 0x23, 0}, 0, 2, {OPT_DRReg|OPS_32|OPA_Spare, OPT_Reg|OPS_64|OPA_EA, 0} },
    { CPU_386|CPU_Not64|CPU_Priv, MOD_GasSufL, 0, 0, 0, 2, {0x0F, 0x21, 0}, 0, 2, {OPT_Reg|OPS_32|OPA_EA, OPT_DRReg|OPS_32|OPA_Spare, 0} },
    { CPU_64|CPU_Priv, MOD_GasSufQ, 0, 0, 0, 2, {0x0F, 0x21, 0}, 0, 2, {OPT_Reg|OPS_64|OPA_EA, OPT_DRReg|OPS_32|OPA_Spare, 0} },
    { CPU_MMX, MOD_GasOnly|MOD_GasSufQ, 0, 0, 0, 2, {0x0F, 0x6F, 0}, 0, 2, {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_SIMDRM|OPS_64|OPS_Relaxed|OPA_EA, 0} },
    { CPU_64|CPU_MMX, MOD_GasOnly|MOD_GasSufQ, 64, 0, 0, 2, {0x0F, 0x6E, 0}, 0, 2, {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, 0} },
    { CPU_MMX, MOD_GasOnly|MOD_GasSufQ, 0, 0, 0, 2, {0x0F, 0x7F, 0}, 0, 2, {OPT_SIMDRM|OPS_64|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_64|OPA_Spare, 0} },
    { CPU_64|CPU_MMX, MOD_GasOnly|MOD_GasSufQ, 64, 0, 0, 2, {0x0F, 0x7E, 0}, 0, 2, {OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_64|OPA_Spare, 0} },
    { CPU_SSE2, MOD_GasOnly|MOD_GasSufQ, 0, 0, 0xF3, 2, {0x0F, 0x7E, 0}, 0, 2, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, 0} },
    { CPU_SSE2, MOD_GasOnly|MOD_GasSufQ, 0, 0, 0xF3, 2, {0x0F, 0x7E, 0}, 0, 2, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDRM|OPS_64|OPS_Relaxed|OPA_EA, 0} },
    { CPU_64|CPU_SSE2, MOD_GasOnly|MOD_GasSufQ, 64, 0, 0x66, 2, {0x0F, 0x6E, 0}, 0, 2, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, 0} },
    { CPU_SSE2, MOD_GasOnly|MOD_GasSufQ, 0, 0, 0x66, 2, {0x0F, 0xD6, 0}, 0, 2, {OPT_SIMDRM|OPS_64|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, 0} },
    { CPU_64|CPU_SSE2, MOD_GasOnly|MOD_GasSufQ, 64, 0, 0x66, 2, {0x0F, 0x7E, 0}, 0, 2, {OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, 0} }
};

static const x86_insn_info movabs_insn[] = {
    { CPU_64, MOD_GasSufB, 0, 0, 0, 1, {0xA0, 0, 0}, 0, 2, {OPT_Areg|OPS_8|OPA_None, OPT_MemOffs|OPS_8|OPS_Relaxed|OPEAS_64|OPA_EA, 0} },
    { CPU_64, MOD_GasSufW, 16, 0, 0, 1, {0xA1, 0, 0}, 0, 2, {OPT_Areg|OPS_16|OPA_None, OPT_MemOffs|OPS_16|OPS_Relaxed|OPEAS_64|OPA_EA, 0} },
    { CPU_64, MOD_GasSufL, 32, 0, 0, 1, {0xA1, 0, 0}, 0, 2, {OPT_Areg|OPS_32|OPA_None, OPT_MemOffs|OPS_32|OPS_Relaxed|OPEAS_64|OPA_EA, 0} },
    { CPU_64, MOD_GasSufQ, 64, 0, 0, 1, {0xA1, 0, 0}, 0, 2, {OPT_Areg|OPS_64|OPA_None, OPT_MemOffs|OPS_64|OPS_Relaxed|OPEAS_64|OPA_EA, 0} },
    { CPU_64, MOD_GasSufB, 0, 0, 0, 1, {0xA2, 0, 0}, 0, 2, {OPT_MemOffs|OPS_8|OPS_Relaxed|OPEAS_64|OPA_EA, OPT_Areg|OPS_8|OPA_None, 0} },
    { CPU_64, MOD_GasSufW, 16, 0, 0, 1, {0xA3, 0, 0}, 0, 2, {OPT_MemOffs|OPS_16|OPS_Relaxed|OPEAS_64|OPA_EA, OPT_Areg|OPS_16|OPA_None, 0} },
    { CPU_64, MOD_GasSufL, 32, 0, 0, 1, {0xA3, 0, 0}, 0, 2, {OPT_MemOffs|OPS_32|OPS_Relaxed|OPEAS_64|OPA_EA, OPT_Areg|OPS_32|OPA_None, 0} },
    { CPU_64, MOD_GasSufQ, 64, 0, 0, 1, {0xA3, 0, 0}, 0, 2, {OPT_MemOffs|OPS_64|OPS_Relaxed|OPEAS_64|OPA_EA, OPT_Areg|OPS_64|OPA_None, 0} },
    { CPU_64, MOD_GasSufQ, 64, 0, 0, 1, {0xB8, 0, 0}, 0, 2, {OPT_Reg|OPS_64|OPA_Op0Add, OPT_Imm|OPS_64|OPS_Relaxed|OPA_Imm, 0} }
};

static const x86_insn_info movszx_insn[] = {
    { CPU_386, MOD_Op1Add|MOD_GasSufB, 16, 0, 0, 2, {0x0F, 0x00, 0}, 0, 2, {OPT_Reg|OPS_16|OPA_Spare, OPT_RM|OPS_8|OPS_Relaxed|OPA_EA, 0} },
    { CPU_386, MOD_Op1Add|MOD_GasSufB, 32, 0, 0, 2, {0x0F, 0x00, 0}, 0, 2, {OPT_Reg|OPS_32|OPA_Spare, OPT_RM|OPS_8|OPA_EA, 0} },
    { CPU_64, MOD_Op1Add|MOD_GasSufB, 64, 0, 0, 2, {0x0F, 0x00, 0}, 0, 2, {OPT_Reg|OPS_64|OPA_Spare, OPT_RM|OPS_8|OPA_EA, 0} },
    { CPU_386, MOD_Op1Add|MOD_GasSufW, 32, 0, 0, 2, {0x0F, 0x01, 0}, 0, 2, {OPT_Reg|OPS_32|OPA_Spare, OPT_RM|OPS_16|OPA_EA, 0} },
    { CPU_64, MOD_Op1Add|MOD_GasSufW, 64, 0, 0, 2, {0x0F, 0x01, 0}, 0, 2, {OPT_Reg|OPS_64|OPA_Spare, OPT_RM|OPS_16|OPA_EA, 0} }
};

static const x86_insn_info movsxd_insn[] = {
    { CPU_64, MOD_GasSufL, 64, 0, 0, 1, {0x63, 0, 0}, 0, 2, {OPT_Reg|OPS_64|OPA_Spare, OPT_RM|OPS_32|OPA_EA, 0} }
};

static const x86_insn_info push_insn[] = {
    { CPU_Any, MOD_GasSufW, 16, 64, 0, 1, {0x50, 0, 0}, 0, 1, {OPT_Reg|OPS_16|OPA_Op0Add, 0, 0} },
    { CPU_386|CPU_Not64, MOD_GasSufL, 32, 0, 0, 1, {0x50, 0, 0}, 0, 1, {OPT_Reg|OPS_32|OPA_Op0Add, 0, 0} },
    { CPU_64, MOD_GasSufQ, 0, 64, 0, 1, {0x50, 0, 0}, 0, 1, {OPT_Reg|OPS_64|OPA_Op0Add, 0, 0} },
    { CPU_Any, MOD_GasSufW, 16, 64, 0, 1, {0xFF, 0, 0}, 6, 1, {OPT_RM|OPS_16|OPA_EA, 0, 0} },
    { CPU_386|CPU_Not64, MOD_GasSufL, 32, 0, 0, 1, {0xFF, 0, 0}, 6, 1, {OPT_RM|OPS_32|OPA_EA, 0, 0} },
    { CPU_64, MOD_GasSufQ, 0, 64, 0, 1, {0xFF, 0, 0}, 6, 1, {OPT_RM|OPS_64|OPA_EA, 0, 0} },
    { CPU_186, MOD_GasIllegal, 0, 64, 0, 1, {0x6A, 0, 0}, 0, 1, {OPT_Imm|OPS_8|OPA_SImm, 0, 0} },
    { CPU_186, MOD_GasOnly, 0, 64, 0, 1, {0x6A, 0, 0}, 0, 1, {OPT_Imm|OPS_8|OPS_Relaxed|OPA_SImm, 0, 0} },
    { CPU_186, MOD_GasOnly|MOD_GasSufW, 16, 64, 0, 1, {0x6A, 0x68, 0}, 0, 1, {OPT_Imm|OPS_16|OPS_Relaxed|OPA_Imm|OPAP_SImm8, 0, 0} },
    { CPU_386|CPU_Not64, MOD_GasOnly|MOD_GasSufL, 32, 0, 0, 1, {0x6A, 0x68, 0}, 0, 1, {OPT_Imm|OPS_32|OPS_Relaxed|OPA_Imm|OPAP_SImm8, 0, 0} },
    { CPU_64, MOD_GasSufQ, 64, 64, 0, 1, {0x6A, 0x68, 0}, 0, 1, {OPT_Imm|OPS_32|OPS_Relaxed|OPA_SImm|OPAP_SImm8, 0, 0} },
    { CPU_186|CPU_Not64, MOD_GasIllegal, 0, 0, 0, 1, {0x6A, 0x68, 0}, 0, 1, {OPT_Imm|OPS_BITS|OPS_Relaxed|OPA_Imm|OPAP_SImm8, 0, 0} },
    { CPU_186, MOD_GasIllegal, 16, 64, 0, 1, {0x68, 0, 0}, 0, 1, {OPT_Imm|OPS_16|OPA_Imm, 0, 0} },
    { CPU_386|CPU_Not64, MOD_GasIllegal, 32, 0, 0, 1, {0x68, 0, 0}, 0, 1, {OPT_Imm|OPS_32|OPA_Imm, 0, 0} },
    { CPU_64, MOD_GasIllegal, 64, 64, 0, 1, {0x68, 0, 0}, 0, 1, {OPT_Imm|OPS_32|OPA_SImm, 0, 0} },
    { CPU_Not64, 0, 0, 0, 0, 1, {0x0E, 0, 0}, 0, 1, {OPT_CS|OPS_Any|OPA_None, 0, 0} },
    { CPU_Not64, MOD_GasSufW, 16, 0, 0, 1, {0x0E, 0, 0}, 0, 1, {OPT_CS|OPS_16|OPA_None, 0, 0} },
    { CPU_386|CPU_Not64, MOD_GasSufL, 32, 0, 0, 1, {0x0E, 0, 0}, 0, 1, {OPT_CS|OPS_32|OPA_None, 0, 0} },
    { CPU_Not64, 0, 0, 0, 0, 1, {0x16, 0, 0}, 0, 1, {OPT_SS|OPS_Any|OPA_None, 0, 0} },
    { CPU_Not64, MOD_GasSufW, 16, 0, 0, 1, {0x16, 0, 0}, 0, 1, {OPT_SS|OPS_16|OPA_None, 0, 0} },
    { CPU_386|CPU_Not64, MOD_GasSufL, 32, 0, 0, 1, {0x16, 0, 0}, 0, 1, {OPT_SS|OPS_32|OPA_None, 0, 0} },
    { CPU_Not64, 0, 0, 0, 0, 1, {0x1E, 0, 0}, 0, 1, {OPT_DS|OPS_Any|OPA_None, 0, 0} },
    { CPU_Not64, MOD_GasSufW, 16, 0, 0, 1, {0x1E, 0, 0}, 0, 1, {OPT_DS|OPS_16|OPA_None, 0, 0} },
    { CPU_386|CPU_Not64, MOD_GasSufL, 32, 0, 0, 1, {0x1E, 0, 0}, 0, 1, {OPT_DS|OPS_32|OPA_None, 0, 0} },
    { CPU_Not64, 0, 0, 0, 0, 1, {0x06, 0, 0}, 0, 1, {OPT_ES|OPS_Any|OPA_None, 0, 0} },
    { CPU_Not64, MOD_GasSufW, 16, 0, 0, 1, {0x06, 0, 0}, 0, 1, {OPT_ES|OPS_16|OPA_None, 0, 0} },
    { CPU_386|CPU_Not64, MOD_GasSufL, 32, 0, 0, 1, {0x06, 0, 0}, 0, 1, {OPT_ES|OPS_32|OPA_None, 0, 0} },
    { CPU_386, 0, 0, 0, 0, 2, {0x0F, 0xA0, 0}, 0, 1, {OPT_FS|OPS_Any|OPA_None, 0, 0} },
    { CPU_386, MOD_GasSufW, 16, 0, 0, 2, {0x0F, 0xA0, 0}, 0, 1, {OPT_FS|OPS_16|OPA_None, 0, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 2, {0x0F, 0xA0, 0}, 0, 1, {OPT_FS|OPS_32|OPA_None, 0, 0} },
    { CPU_386, 0, 0, 0, 0, 2, {0x0F, 0xA8, 0}, 0, 1, {OPT_GS|OPS_Any|OPA_None, 0, 0} },
    { CPU_386, MOD_GasSufW, 16, 0, 0, 2, {0x0F, 0xA8, 0}, 0, 1, {OPT_GS|OPS_16|OPA_None, 0, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 2, {0x0F, 0xA8, 0}, 0, 1, {OPT_GS|OPS_32|OPA_None, 0, 0} }
};

static const x86_insn_info pop_insn[] = {
    { CPU_Any, MOD_GasSufW, 16, 64, 0, 1, {0x58, 0, 0}, 0, 1, {OPT_Reg|OPS_16|OPA_Op0Add, 0, 0} },
    { CPU_386|CPU_Not64, MOD_GasSufL, 32, 0, 0, 1, {0x58, 0, 0}, 0, 1, {OPT_Reg|OPS_32|OPA_Op0Add, 0, 0} },
    { CPU_64, MOD_GasSufQ, 0, 64, 0, 1, {0x58, 0, 0}, 0, 1, {OPT_Reg|OPS_64|OPA_Op0Add, 0, 0} },
    { CPU_Any, MOD_GasSufW, 16, 64, 0, 1, {0x8F, 0, 0}, 0, 1, {OPT_RM|OPS_16|OPA_EA, 0, 0} },
    { CPU_386|CPU_Not64, MOD_GasSufL, 32, 0, 0, 1, {0x8F, 0, 0}, 0, 1, {OPT_RM|OPS_32|OPA_EA, 0, 0} },
    { CPU_64, MOD_GasSufQ, 0, 64, 0, 1, {0x8F, 0, 0}, 0, 1, {OPT_RM|OPS_64|OPA_EA, 0, 0} },
    { CPU_Not64, 0, 0, 0, 0, 1, {0x17, 0, 0}, 0, 1, {OPT_SS|OPS_Any|OPA_None, 0, 0} },
    { CPU_Not64, 0, 16, 0, 0, 1, {0x17, 0, 0}, 0, 1, {OPT_SS|OPS_16|OPA_None, 0, 0} },
    { CPU_386|CPU_Not64, 0, 32, 0, 0, 1, {0x17, 0, 0}, 0, 1, {OPT_SS|OPS_32|OPA_None, 0, 0} },
    { CPU_Not64, 0, 0, 0, 0, 1, {0x1F, 0, 0}, 0, 1, {OPT_DS|OPS_Any|OPA_None, 0, 0} },
    { CPU_Not64, 0, 16, 0, 0, 1, {0x1F, 0, 0}, 0, 1, {OPT_DS|OPS_16|OPA_None, 0, 0} },
    { CPU_386|CPU_Not64, 0, 32, 0, 0, 1, {0x1F, 0, 0}, 0, 1, {OPT_DS|OPS_32|OPA_None, 0, 0} },
    { CPU_Not64, 0, 0, 0, 0, 1, {0x07, 0, 0}, 0, 1, {OPT_ES|OPS_Any|OPA_None, 0, 0} },
    { CPU_Not64, 0, 16, 0, 0, 1, {0x07, 0, 0}, 0, 1, {OPT_ES|OPS_16|OPA_None, 0, 0} },
    { CPU_386|CPU_Not64, 0, 32, 0, 0, 1, {0x07, 0, 0}, 0, 1, {OPT_ES|OPS_32|OPA_None, 0, 0} },
    { CPU_386, 0, 0, 0, 0, 2, {0x0F, 0xA1, 0}, 0, 1, {OPT_FS|OPS_Any|OPA_None, 0, 0} },
    { CPU_386, 0, 16, 0, 0, 2, {0x0F, 0xA1, 0}, 0, 1, {OPT_FS|OPS_16|OPA_None, 0, 0} },
    { CPU_386, 0, 32, 0, 0, 2, {0x0F, 0xA1, 0}, 0, 1, {OPT_FS|OPS_32|OPA_None, 0, 0} },
    { CPU_386, 0, 0, 0, 0, 2, {0x0F, 0xA9, 0}, 0, 1, {OPT_GS|OPS_Any|OPA_None, 0, 0} },
    { CPU_386, 0, 16, 0, 0, 2, {0x0F, 0xA9, 0}, 0, 1, {OPT_GS|OPS_16|OPA_None, 0, 0} },
    { CPU_386, 0, 32, 0, 0, 2, {0x0F, 0xA9, 0}, 0, 1, {OPT_GS|OPS_32|OPA_None, 0, 0} }
};

static const x86_insn_info xchg_insn[] = {
    { CPU_Any, MOD_GasSufB, 0, 0, 0, 1, {0x86, 0, 0}, 0, 2, {OPT_RM|OPS_8|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_8|OPA_Spare, 0} },
    { CPU_Any, MOD_GasSufB, 0, 0, 0, 1, {0x86, 0, 0}, 0, 2, {OPT_Reg|OPS_8|OPA_Spare, OPT_RM|OPS_8|OPS_Relaxed|OPA_EA, 0} },
    { CPU_Any, MOD_GasSufW, 16, 0, 0, 1, {0x90, 0, 0}, 0, 2, {OPT_Areg|OPS_16|OPA_None, OPT_Reg|OPS_16|OPA_Op0Add, 0} },
    { CPU_Any, MOD_GasSufW, 16, 0, 0, 1, {0x90, 0, 0}, 0, 2, {OPT_Reg|OPS_16|OPA_Op0Add, OPT_Areg|OPS_16|OPA_None, 0} },
    { CPU_Any, MOD_GasSufW, 16, 0, 0, 1, {0x87, 0, 0}, 0, 2, {OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_16|OPA_Spare, 0} },
    { CPU_Any, MOD_GasSufW, 16, 0, 0, 1, {0x87, 0, 0}, 0, 2, {OPT_Reg|OPS_16|OPA_Spare, OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, 0} },
    { CPU_64, MOD_GasSufL, 32, 0, 0, 1, {0x87, 0, 0}, 0, 2, {OPT_Areg|OPS_32|OPA_EA, OPT_Areg|OPS_32|OPA_Spare, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0x90, 0, 0}, 0, 2, {OPT_Areg|OPS_32|OPA_None, OPT_Reg|OPS_32|OPA_Op0Add, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0x90, 0, 0}, 0, 2, {OPT_Reg|OPS_32|OPA_Op0Add, OPT_Areg|OPS_32|OPA_None, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0x87, 0, 0}, 0, 2, {OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_32|OPA_Spare, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0x87, 0, 0}, 0, 2, {OPT_Reg|OPS_32|OPA_Spare, OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, 0} },
    { CPU_64, MOD_GasSufQ, 0, 0, 0, 1, {0x90, 0, 0}, 0, 2, {OPT_Areg|OPS_64|OPA_None, OPT_Areg|OPS_64|OPA_Op0Add, 0} },
    { CPU_64, MOD_GasSufQ, 64, 0, 0, 1, {0x90, 0, 0}, 0, 2, {OPT_Areg|OPS_64|OPA_None, OPT_Reg|OPS_64|OPA_Op0Add, 0} },
    { CPU_64, MOD_GasSufQ, 64, 0, 0, 1, {0x90, 0, 0}, 0, 2, {OPT_Reg|OPS_64|OPA_Op0Add, OPT_Areg|OPS_64|OPA_None, 0} },
    { CPU_64, MOD_GasSufQ, 64, 0, 0, 1, {0x87, 0, 0}, 0, 2, {OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_64|OPA_Spare, 0} },
    { CPU_64, MOD_GasSufQ, 64, 0, 0, 1, {0x87, 0, 0}, 0, 2, {OPT_Reg|OPS_64|OPA_Spare, OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, 0} }
};

static const x86_insn_info in_insn[] = {
    { CPU_Any, MOD_GasSufB, 0, 0, 0, 1, {0xE4, 0, 0}, 0, 2, {OPT_Areg|OPS_8|OPA_None, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_Any, MOD_GasSufW, 16, 0, 0, 1, {0xE5, 0, 0}, 0, 2, {OPT_Areg|OPS_16|OPA_None, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0xE5, 0, 0}, 0, 2, {OPT_Areg|OPS_32|OPA_None, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_Any, MOD_GasSufB, 0, 0, 0, 1, {0xEC, 0, 0}, 0, 2, {OPT_Areg|OPS_8|OPA_None, OPT_Dreg|OPS_16|OPA_None, 0} },
    { CPU_Any, MOD_GasSufW, 16, 0, 0, 1, {0xED, 0, 0}, 0, 2, {OPT_Areg|OPS_16|OPA_None, OPT_Dreg|OPS_16|OPA_None, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0xED, 0, 0}, 0, 2, {OPT_Areg|OPS_32|OPA_None, OPT_Dreg|OPS_16|OPA_None, 0} },
    { CPU_Any, MOD_GasOnly|MOD_GasSufB, 0, 0, 0, 1, {0xE4, 0, 0}, 0, 1, {OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0, 0} },
    { CPU_Any, MOD_GasOnly|MOD_GasSufW, 16, 0, 0, 1, {0xE5, 0, 0}, 0, 1, {OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0, 0} },
    { CPU_386, MOD_GasOnly|MOD_GasSufL, 32, 0, 0, 1, {0xE5, 0, 0}, 0, 1, {OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0, 0} },
    { CPU_Any, MOD_GasOnly|MOD_GasSufB, 0, 0, 0, 1, {0xEC, 0, 0}, 0, 1, {OPT_Dreg|OPS_16|OPA_None, 0, 0} },
    { CPU_Any, MOD_GasOnly|MOD_GasSufW, 16, 0, 0, 1, {0xED, 0, 0}, 0, 1, {OPT_Dreg|OPS_16|OPA_None, 0, 0} },
    { CPU_386, MOD_GasOnly|MOD_GasSufL, 32, 0, 0, 1, {0xED, 0, 0}, 0, 1, {OPT_Dreg|OPS_16|OPA_None, 0, 0} }
};

static const x86_insn_info out_insn[] = {
    { CPU_Any, MOD_GasSufB, 0, 0, 0, 1, {0xE6, 0, 0}, 0, 2, {OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, OPT_Areg|OPS_8|OPA_None, 0} },
    { CPU_Any, MOD_GasSufW, 16, 0, 0, 1, {0xE7, 0, 0}, 0, 2, {OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, OPT_Areg|OPS_16|OPA_None, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0xE7, 0, 0}, 0, 2, {OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, OPT_Areg|OPS_32|OPA_None, 0} },
    { CPU_Any, MOD_GasSufB, 0, 0, 0, 1, {0xEE, 0, 0}, 0, 2, {OPT_Dreg|OPS_16|OPA_None, OPT_Areg|OPS_8|OPA_None, 0} },
    { CPU_Any, MOD_GasSufW, 16, 0, 0, 1, {0xEF, 0, 0}, 0, 2, {OPT_Dreg|OPS_16|OPA_None, OPT_Areg|OPS_16|OPA_None, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0xEF, 0, 0}, 0, 2, {OPT_Dreg|OPS_16|OPA_None, OPT_Areg|OPS_32|OPA_None, 0} },
    { CPU_Any, MOD_GasOnly|MOD_GasSufB, 0, 0, 0, 1, {0xE6, 0, 0}, 0, 1, {OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0, 0} },
    { CPU_Any, MOD_GasOnly|MOD_GasSufW, 16, 0, 0, 1, {0xE7, 0, 0}, 0, 1, {OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0, 0} },
    { CPU_386, MOD_GasOnly|MOD_GasSufL, 32, 0, 0, 1, {0xE7, 0, 0}, 0, 1, {OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0, 0} },
    { CPU_Any, MOD_GasOnly|MOD_GasSufB, 0, 0, 0, 1, {0xEE, 0, 0}, 0, 1, {OPT_Dreg|OPS_16|OPA_None, 0, 0} },
    { CPU_Any, MOD_GasOnly|MOD_GasSufW, 16, 0, 0, 1, {0xEF, 0, 0}, 0, 1, {OPT_Dreg|OPS_16|OPA_None, 0, 0} },
    { CPU_386, MOD_GasOnly|MOD_GasSufL, 32, 0, 0, 1, {0xEF, 0, 0}, 0, 1, {OPT_Dreg|OPS_16|OPA_None, 0, 0} }
};

static const x86_insn_info lea_insn[] = {
    { CPU_Any, MOD_GasSufW, 16, 0, 0, 1, {0x8D, 0, 0}, 0, 2, {OPT_Reg|OPS_16|OPA_Spare, OPT_Mem|OPS_16|OPS_Relaxed|OPA_EA, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0x8D, 0, 0}, 0, 2, {OPT_Reg|OPS_32|OPA_Spare, OPT_Mem|OPS_32|OPS_Relaxed|OPA_EA, 0} },
    { CPU_64, MOD_GasSufQ, 64, 0, 0, 1, {0x8D, 0, 0}, 0, 2, {OPT_Reg|OPS_64|OPA_Spare, OPT_Mem|OPS_64|OPS_Relaxed|OPA_EA, 0} }
};

static const x86_insn_info ldes_insn[] = {
    { CPU_Not64, MOD_Op0Add|MOD_GasSufW, 16, 0, 0, 1, {0x00, 0, 0}, 0, 2, {OPT_Reg|OPS_16|OPA_Spare, OPT_Mem|OPS_Any|OPA_EA, 0} },
    { CPU_386|CPU_Not64, MOD_Op0Add|MOD_GasSufL, 32, 0, 0, 1, {0x00, 0, 0}, 0, 2, {OPT_Reg|OPS_32|OPA_Spare, OPT_Mem|OPS_Any|OPA_EA, 0} }
};

static const x86_insn_info lfgss_insn[] = {
    { CPU_386, MOD_Op1Add|MOD_GasSufW, 16, 0, 0, 2, {0x0F, 0x00, 0}, 0, 2, {OPT_Reg|OPS_16|OPA_Spare, OPT_Mem|OPS_Any|OPA_EA, 0} },
    { CPU_386, MOD_Op1Add|MOD_GasSufL, 32, 0, 0, 2, {0x0F, 0x00, 0}, 0, 2, {OPT_Reg|OPS_32|OPA_Spare, OPT_Mem|OPS_Any|OPA_EA, 0} }
};

static const x86_insn_info arith_insn[] = {
    { CPU_Any, MOD_Op0Add|MOD_GasSufB, 0, 0, 0, 1, {0x04, 0, 0}, 0, 2, {OPT_Areg|OPS_8|OPA_None, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_Any, MOD_Op2Add|MOD_Op1AddSp|MOD_GasSufW, 16, 0, 0, 2, {0x83, 0xC0, 0x05}, 0, 2, {OPT_Areg|OPS_16|OPA_None, OPT_Imm|OPS_16|OPS_Relaxed|OPA_Imm|OPAP_SImm8, 0} },
    { CPU_386, MOD_Op2Add|MOD_Op1AddSp|MOD_GasSufL, 32, 0, 0, 2, {0x83, 0xC0, 0x05}, 0, 2, {OPT_Areg|OPS_32|OPA_None, OPT_Imm|OPS_32|OPS_Relaxed|OPA_Imm|OPAP_SImm8, 0} },
    { CPU_64, MOD_Op2Add|MOD_Op1AddSp|MOD_GasSufQ, 64, 0, 0, 2, {0x83, 0xC0, 0x05}, 0, 2, {OPT_Areg|OPS_64|OPA_None, OPT_Imm|OPS_32|OPS_Relaxed|OPA_Imm|OPAP_SImm8, 0} },
    { CPU_Any, MOD_Gap0|MOD_SpAdd|MOD_GasSufB, 0, 0, 0, 1, {0x80, 0, 0}, 0, 2, {OPT_RM|OPS_8|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_Any, MOD_Gap0|MOD_SpAdd|MOD_GasSufB, 0, 0, 0, 1, {0x80, 0, 0}, 0, 2, {OPT_RM|OPS_8|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_8|OPA_Imm, 0} },
    { CPU_Any, MOD_Gap0|MOD_SpAdd|MOD_GasSufW, 16, 0, 0, 1, {0x83, 0, 0}, 0, 2, {OPT_RM|OPS_16|OPA_EA, OPT_Imm|OPS_8|OPA_SImm, 0} },
    { CPU_Any, MOD_Gap0|MOD_SpAdd|MOD_GasIllegal, 16, 0, 0, 1, {0x83, 0x81, 0}, 0, 2, {OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_16|OPA_Imm|OPAP_SImm8, 0} },
    { CPU_Any, MOD_Gap0|MOD_SpAdd|MOD_GasSufW, 16, 0, 0, 1, {0x83, 0x81, 0}, 0, 2, {OPT_RM|OPS_16|OPA_EA, OPT_Imm|OPS_16|OPS_Relaxed|OPA_Imm|OPAP_SImm8, 0} },
    { CPU_386, MOD_Gap0|MOD_SpAdd|MOD_GasSufL, 32, 0, 0, 1, {0x83, 0, 0}, 0, 2, {OPT_RM|OPS_32|OPA_EA, OPT_Imm|OPS_8|OPA_SImm, 0} },
    { CPU_386|CPU_Not64, MOD_Gap0|MOD_SpAdd|MOD_GasIllegal, 32, 0, 0, 1, {0x83, 0x81, 0}, 0, 2, {OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_32|OPA_Imm|OPAP_SImm8, 0} },
    { CPU_386, MOD_Gap0|MOD_SpAdd|MOD_GasSufL, 32, 0, 0, 1, {0x83, 0x81, 0}, 0, 2, {OPT_RM|OPS_32|OPA_EA, OPT_Imm|OPS_32|OPS_Relaxed|OPA_Imm|OPAP_SImm8, 0} },
    { CPU_64, MOD_Gap0|MOD_SpAdd|MOD_GasSufQ, 64, 0, 0, 1, {0x83, 0, 0}, 0, 2, {OPT_RM|OPS_64|OPA_EA, OPT_Imm|OPS_8|OPA_SImm, 0} },
    { CPU_64, MOD_Gap0|MOD_SpAdd|MOD_GasSufQ, 64, 0, 0, 1, {0x83, 0x81, 0}, 0, 2, {OPT_RM|OPS_64|OPA_EA, OPT_Imm|OPS_32|OPS_Relaxed|OPA_Imm|OPAP_SImm8, 0} },
    { CPU_Any, MOD_Op0Add|MOD_GasSufB, 0, 0, 0, 1, {0x00, 0, 0}, 0, 2, {OPT_RM|OPS_8|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_8|OPA_Spare, 0} },
    { CPU_Any, MOD_Op0Add|MOD_GasSufW, 16, 0, 0, 1, {0x01, 0, 0}, 0, 2, {OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_16|OPA_Spare, 0} },
    { CPU_386, MOD_Op0Add|MOD_GasSufL, 32, 0, 0, 1, {0x01, 0, 0}, 0, 2, {OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_32|OPA_Spare, 0} },
    { CPU_64, MOD_Op0Add|MOD_GasSufQ, 64, 0, 0, 1, {0x01, 0, 0}, 0, 2, {OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_64|OPA_Spare, 0} },
    { CPU_Any, MOD_Op0Add|MOD_GasSufB, 0, 0, 0, 1, {0x02, 0, 0}, 0, 2, {OPT_Reg|OPS_8|OPA_Spare, OPT_RM|OPS_8|OPS_Relaxed|OPA_EA, 0} },
    { CPU_Any, MOD_Op0Add|MOD_GasSufW, 16, 0, 0, 1, {0x03, 0, 0}, 0, 2, {OPT_Reg|OPS_16|OPA_Spare, OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, 0} },
    { CPU_386, MOD_Op0Add|MOD_GasSufL, 32, 0, 0, 1, {0x03, 0, 0}, 0, 2, {OPT_Reg|OPS_32|OPA_Spare, OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, 0} },
    { CPU_64, MOD_Op0Add|MOD_GasSufQ, 64, 0, 0, 1, {0x03, 0, 0}, 0, 2, {OPT_Reg|OPS_64|OPA_Spare, OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, 0} }
};

static const x86_insn_info incdec_insn[] = {
    { CPU_Any, MOD_Gap0|MOD_SpAdd|MOD_GasSufB, 0, 0, 0, 1, {0xFE, 0, 0}, 0, 1, {OPT_RM|OPS_8|OPA_EA, 0, 0} },
    { CPU_Not64, MOD_Op0Add|MOD_GasSufW, 16, 0, 0, 1, {0x00, 0, 0}, 0, 1, {OPT_Reg|OPS_16|OPA_Op0Add, 0, 0} },
    { CPU_Any, MOD_Gap0|MOD_SpAdd|MOD_GasSufW, 16, 0, 0, 1, {0xFF, 0, 0}, 0, 1, {OPT_RM|OPS_16|OPA_EA, 0, 0} },
    { CPU_386|CPU_Not64, MOD_Op0Add|MOD_GasSufL, 32, 0, 0, 1, {0x00, 0, 0}, 0, 1, {OPT_Reg|OPS_32|OPA_Op0Add, 0, 0} },
    { CPU_386, MOD_Gap0|MOD_SpAdd|MOD_GasSufL, 32, 0, 0, 1, {0xFF, 0, 0}, 0, 1, {OPT_RM|OPS_32|OPA_EA, 0, 0} },
    { CPU_64, MOD_Gap0|MOD_SpAdd|MOD_GasSufQ, 64, 0, 0, 1, {0xFF, 0, 0}, 0, 1, {OPT_RM|OPS_64|OPA_EA, 0, 0} }
};

static const x86_insn_info f6_insn[] = {
    { CPU_Any, MOD_SpAdd|MOD_GasSufB, 0, 0, 0, 1, {0xF6, 0, 0}, 0, 1, {OPT_RM|OPS_8|OPA_EA, 0, 0} },
    { CPU_Any, MOD_SpAdd|MOD_GasSufW, 16, 0, 0, 1, {0xF7, 0, 0}, 0, 1, {OPT_RM|OPS_16|OPA_EA, 0, 0} },
    { CPU_386, MOD_SpAdd|MOD_GasSufL, 32, 0, 0, 1, {0xF7, 0, 0}, 0, 1, {OPT_RM|OPS_32|OPA_EA, 0, 0} },
    { CPU_64, MOD_SpAdd|MOD_GasSufQ, 64, 0, 0, 1, {0xF7, 0, 0}, 0, 1, {OPT_RM|OPS_64|OPA_EA, 0, 0} }
};

static const x86_insn_info div_insn[] = {
    { CPU_Any, MOD_SpAdd|MOD_GasSufB, 0, 0, 0, 1, {0xF6, 0, 0}, 0, 1, {OPT_RM|OPS_8|OPA_EA, 0, 0} },
    { CPU_Any, MOD_SpAdd|MOD_GasSufW, 16, 0, 0, 1, {0xF7, 0, 0}, 0, 1, {OPT_RM|OPS_16|OPA_EA, 0, 0} },
    { CPU_386, MOD_SpAdd|MOD_GasSufL, 32, 0, 0, 1, {0xF7, 0, 0}, 0, 1, {OPT_RM|OPS_32|OPA_EA, 0, 0} },
    { CPU_64, MOD_SpAdd|MOD_GasSufQ, 64, 0, 0, 1, {0xF7, 0, 0}, 0, 1, {OPT_RM|OPS_64|OPA_EA, 0, 0} },
    { CPU_Any, MOD_SpAdd|MOD_GasSufB, 0, 0, 0, 1, {0xF6, 0, 0}, 0, 2, {OPT_Areg|OPS_8|OPA_None, OPT_RM|OPS_8|OPA_EA, 0} },
    { CPU_Any, MOD_SpAdd|MOD_GasSufW, 16, 0, 0, 1, {0xF7, 0, 0}, 0, 2, {OPT_Areg|OPS_16|OPA_None, OPT_RM|OPS_16|OPA_EA, 0} },
    { CPU_386, MOD_SpAdd|MOD_GasSufL, 32, 0, 0, 1, {0xF7, 0, 0}, 0, 2, {OPT_Areg|OPS_32|OPA_None, OPT_RM|OPS_32|OPA_EA, 0} },
    { CPU_64, MOD_SpAdd|MOD_GasSufQ, 64, 0, 0, 1, {0xF7, 0, 0}, 0, 2, {OPT_Areg|OPS_64|OPA_None, OPT_RM|OPS_64|OPA_EA, 0} }
};

static const x86_insn_info test_insn[] = {
    { CPU_Any, MOD_GasSufB, 0, 0, 0, 1, {0xA8, 0, 0}, 0, 2, {OPT_Areg|OPS_8|OPA_None, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_Any, MOD_GasSufW, 16, 0, 0, 1, {0xA9, 0, 0}, 0, 2, {OPT_Areg|OPS_16|OPA_None, OPT_Imm|OPS_16|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0xA9, 0, 0}, 0, 2, {OPT_Areg|OPS_32|OPA_None, OPT_Imm|OPS_32|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_64, MOD_GasSufQ, 64, 0, 0, 1, {0xA9, 0, 0}, 0, 2, {OPT_Areg|OPS_64|OPA_None, OPT_Imm|OPS_32|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_Any, MOD_GasSufB, 0, 0, 0, 1, {0xF6, 0, 0}, 0, 2, {OPT_RM|OPS_8|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_Any, MOD_GasSufB, 0, 0, 0, 1, {0xF6, 0, 0}, 0, 2, {OPT_RM|OPS_8|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_8|OPA_Imm, 0} },
    { CPU_Any, MOD_GasSufW, 16, 0, 0, 1, {0xF7, 0, 0}, 0, 2, {OPT_RM|OPS_16|OPA_EA, OPT_Imm|OPS_16|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_Any, MOD_GasSufW, 16, 0, 0, 1, {0xF7, 0, 0}, 0, 2, {OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_16|OPA_Imm, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0xF7, 0, 0}, 0, 2, {OPT_RM|OPS_32|OPA_EA, OPT_Imm|OPS_32|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0xF7, 0, 0}, 0, 2, {OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_32|OPA_Imm, 0} },
    { CPU_64, MOD_GasSufQ, 64, 0, 0, 1, {0xF7, 0, 0}, 0, 2, {OPT_RM|OPS_64|OPA_EA, OPT_Imm|OPS_32|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_64, MOD_GasSufQ, 64, 0, 0, 1, {0xF7, 0, 0}, 0, 2, {OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_32|OPA_Imm, 0} },
    { CPU_Any, MOD_GasSufB, 0, 0, 0, 1, {0x84, 0, 0}, 0, 2, {OPT_RM|OPS_8|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_8|OPA_Spare, 0} },
    { CPU_Any, MOD_GasSufW, 16, 0, 0, 1, {0x85, 0, 0}, 0, 2, {OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_16|OPA_Spare, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0x85, 0, 0}, 0, 2, {OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_32|OPA_Spare, 0} },
    { CPU_64, MOD_GasSufQ, 64, 0, 0, 1, {0x85, 0, 0}, 0, 2, {OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_64|OPA_Spare, 0} },
    { CPU_Any, MOD_GasSufB, 0, 0, 0, 1, {0x84, 0, 0}, 0, 2, {OPT_Reg|OPS_8|OPA_Spare, OPT_RM|OPS_8|OPS_Relaxed|OPA_EA, 0} },
    { CPU_Any, MOD_GasSufW, 16, 0, 0, 1, {0x85, 0, 0}, 0, 2, {OPT_Reg|OPS_16|OPA_Spare, OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0x85, 0, 0}, 0, 2, {OPT_Reg|OPS_32|OPA_Spare, OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, 0} },
    { CPU_64, MOD_GasSufQ, 64, 0, 0, 1, {0x85, 0, 0}, 0, 2, {OPT_Reg|OPS_64|OPA_Spare, OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, 0} }
};

static const x86_insn_info aadm_insn[] = {
    { CPU_Any, MOD_Op0Add, 0, 0, 0, 2, {0xD4, 0x0A, 0}, 0, 0, {0, 0, 0} },
    { CPU_Any, MOD_Op0Add, 0, 0, 0, 1, {0xD4, 0, 0}, 0, 1, {OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0, 0} }
};

static const x86_insn_info imul_insn[] = {
    { CPU_Any, MOD_GasSufB, 0, 0, 0, 1, {0xF6, 0, 0}, 5, 1, {OPT_RM|OPS_8|OPA_EA, 0, 0} },
    { CPU_Any, MOD_GasSufW, 16, 0, 0, 1, {0xF7, 0, 0}, 5, 1, {OPT_RM|OPS_16|OPA_EA, 0, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0xF7, 0, 0}, 5, 1, {OPT_RM|OPS_32|OPA_EA, 0, 0} },
    { CPU_64, MOD_GasSufQ, 64, 0, 0, 1, {0xF7, 0, 0}, 5, 1, {OPT_RM|OPS_64|OPA_EA, 0, 0} },
    { CPU_386, MOD_GasSufW, 16, 0, 0, 2, {0x0F, 0xAF, 0}, 0, 2, {OPT_Reg|OPS_16|OPA_Spare, OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 2, {0x0F, 0xAF, 0}, 0, 2, {OPT_Reg|OPS_32|OPA_Spare, OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, 0} },
    { CPU_386|CPU_64, MOD_GasSufQ, 64, 0, 0, 2, {0x0F, 0xAF, 0}, 0, 2, {OPT_Reg|OPS_64|OPA_Spare, OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, 0} },
    { CPU_186, MOD_GasSufW, 16, 0, 0, 1, {0x6B, 0, 0}, 0, 3, {OPT_Reg|OPS_16|OPA_Spare, OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_8|OPA_SImm} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0x6B, 0, 0}, 0, 3, {OPT_Reg|OPS_32|OPA_Spare, OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_8|OPA_SImm} },
    { CPU_186|CPU_64, MOD_GasSufQ, 64, 0, 0, 1, {0x6B, 0, 0}, 0, 3, {OPT_Reg|OPS_64|OPA_Spare, OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_8|OPA_SImm} },
    { CPU_186, MOD_GasSufW, 16, 0, 0, 1, {0x6B, 0, 0}, 0, 2, {OPT_Reg|OPS_16|OPA_SpareEA, OPT_Imm|OPS_8|OPA_SImm, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0x6B, 0, 0}, 0, 2, {OPT_Reg|OPS_32|OPA_SpareEA, OPT_Imm|OPS_8|OPA_SImm, 0} },
    { CPU_186|CPU_64, MOD_GasSufQ, 64, 0, 0, 1, {0x6B, 0, 0}, 0, 2, {OPT_Reg|OPS_64|OPA_SpareEA, OPT_Imm|OPS_8|OPA_SImm, 0} },
    { CPU_186, MOD_GasSufW, 16, 0, 0, 1, {0x6B, 0x69, 0}, 0, 3, {OPT_Reg|OPS_16|OPA_Spare, OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_16|OPS_Relaxed|OPA_SImm|OPAP_SImm8} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0x6B, 0x69, 0}, 0, 3, {OPT_Reg|OPS_32|OPA_Spare, OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_32|OPS_Relaxed|OPA_SImm|OPAP_SImm8} },
    { CPU_186|CPU_64, MOD_GasSufQ, 64, 0, 0, 1, {0x6B, 0x69, 0}, 0, 3, {OPT_Reg|OPS_64|OPA_Spare, OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_32|OPS_Relaxed|OPA_SImm|OPAP_SImm8} },
    { CPU_186, MOD_GasSufW, 16, 0, 0, 1, {0x6B, 0x69, 0}, 0, 2, {OPT_Reg|OPS_16|OPA_SpareEA, OPT_Imm|OPS_16|OPS_Relaxed|OPA_SImm|OPAP_SImm8, 0} },
    { CPU_386, MOD_GasSufL, 32, 0, 0, 1, {0x6B, 0x69, 0}, 0, 2, {OPT_Reg|OPS_32|OPA_SpareEA, OPT_Imm|OPS_32|OPS_Relaxed|OPA_SImm|OPAP_SImm8, 0} },
    { CPU_186|CPU_64, MOD_GasSufQ, 64, 0, 0, 1, {0x6B, 0x69, 0}, 0, 2, {OPT_Reg|OPS_64|OPA_SpareEA, OPT_Imm|OPS_32|OPS_Relaxed|OPA_SImm|OPAP_SImm8, 0} }
};

static const x86_insn_info shift_insn[] = {
    { CPU_Any, MOD_SpAdd|MOD_GasSufB, 0, 0, 0, 1, {0xD2, 0, 0}, 0, 2, {OPT_RM|OPS_8|OPA_EA, OPT_Creg|OPS_8|OPA_None, 0} },
    { CPU_Any, MOD_SpAdd|MOD_GasSufB, 0, 0, 0, 1, {0xD0, 0, 0}, 0, 2, {OPT_RM|OPS_8|OPA_EA, OPT_Imm1|OPS_8|OPS_Relaxed|OPA_None, 0} },
    { CPU_186, MOD_SpAdd|MOD_GasSufB, 0, 0, 0, 1, {0xC0, 0, 0}, 0, 2, {OPT_RM|OPS_8|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_Any, MOD_SpAdd|MOD_GasSufW, 16, 0, 0, 1, {0xD3, 0, 0}, 0, 2, {OPT_RM|OPS_16|OPA_EA, OPT_Creg|OPS_8|OPA_None, 0} },
    { CPU_Any, MOD_SpAdd|MOD_GasSufW, 16, 0, 0, 1, {0xD1, 0, 0}, 0, 2, {OPT_RM|OPS_16|OPA_EA, OPT_Imm1|OPS_8|OPS_Relaxed|OPA_None, 0} },
    { CPU_186, MOD_SpAdd|MOD_GasSufW, 16, 0, 0, 1, {0xC1, 0, 0}, 0, 2, {OPT_RM|OPS_16|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_386, MOD_SpAdd|MOD_GasSufL, 32, 0, 0, 1, {0xD3, 0, 0}, 0, 2, {OPT_RM|OPS_32|OPA_EA, OPT_Creg|OPS_8|OPA_None, 0} },
    { CPU_386, MOD_SpAdd|MOD_GasSufL, 32, 0, 0, 1, {0xD1, 0, 0}, 0, 2, {OPT_RM|OPS_32|OPA_EA, OPT_Imm1|OPS_8|OPS_Relaxed|OPA_None, 0} },
    { CPU_386, MOD_SpAdd|MOD_GasSufL, 32, 0, 0, 1, {0xC1, 0, 0}, 0, 2, {OPT_RM|OPS_32|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_64, MOD_SpAdd|MOD_GasSufQ, 64, 0, 0, 1, {0xD3, 0, 0}, 0, 2, {OPT_RM|OPS_64|OPA_EA, OPT_Creg|OPS_8|OPA_None, 0} },
    { CPU_64, MOD_SpAdd|MOD_GasSufQ, 64, 0, 0, 1, {0xD1, 0, 0}, 0, 2, {OPT_RM|OPS_64|OPA_EA, OPT_Imm1|OPS_8|OPS_Relaxed|OPA_None, 0} },
    { CPU_186|CPU_64, MOD_SpAdd|MOD_GasSufQ, 64, 0, 0, 1, {0xC1, 0, 0}, 0, 2, {OPT_RM|OPS_64|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_Any, MOD_SpAdd|MOD_GasOnly|MOD_GasSufB, 0, 0, 0, 1, {0xD0, 0, 0}, 0, 1, {OPT_RM|OPS_8|OPA_EA, 0, 0} },
    { CPU_Any, MOD_SpAdd|MOD_GasOnly|MOD_GasSufW, 16, 0, 0, 1, {0xD1, 0, 0}, 0, 1, {OPT_RM|OPS_16|OPA_EA, 0, 0} },
    { CPU_386, MOD_SpAdd|MOD_GasOnly|MOD_GasSufL, 32, 0, 0, 1, {0xD1, 0, 0}, 0, 1, {OPT_RM|OPS_32|OPA_EA, 0, 0} },
    { CPU_64, MOD_SpAdd|MOD_GasOnly|MOD_GasSufQ, 64, 0, 0, 1, {0xD1, 0, 0}, 0, 1, {OPT_RM|OPS_64|OPA_EA, 0, 0} }
};

static const x86_insn_info shlrd_insn[] = {
    { CPU_386, MOD_Op1Add|MOD_GasSufW, 16, 0, 0, 2, {0x0F, 0x00, 0}, 0, 3, {OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_16|OPA_Spare, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} },
    { CPU_386, MOD_Op1Add|MOD_GasSufW, 16, 0, 0, 2, {0x0F, 0x01, 0}, 0, 3, {OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_16|OPA_Spare, OPT_Creg|OPS_8|OPA_None} },
    { CPU_386, MOD_Op1Add|MOD_GasSufL, 32, 0, 0, 2, {0x0F, 0x00, 0}, 0, 3, {OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_32|OPA_Spare, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} },
    { CPU_386, MOD_Op1Add|MOD_GasSufL, 32, 0, 0, 2, {0x0F, 0x01, 0}, 0, 3, {OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_32|OPA_Spare, OPT_Creg|OPS_8|OPA_None} },
    { CPU_386|CPU_64, MOD_Op1Add|MOD_GasSufQ, 64, 0, 0, 2, {0x0F, 0x00, 0}, 0, 3, {OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_64|OPA_Spare, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} },
    { CPU_386|CPU_64, MOD_Op1Add|MOD_GasSufQ, 64, 0, 0, 2, {0x0F, 0x01, 0}, 0, 3, {OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_64|OPA_Spare, OPT_Creg|OPS_8|OPA_None} },
    { CPU_386, MOD_Op1Add|MOD_GasOnly|MOD_GasSufW, 16, 0, 0, 2, {0x0F, 0x01, 0}, 0, 2, {OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_16|OPA_Spare, 0} },
    { CPU_386, MOD_Op1Add|MOD_GasOnly|MOD_GasSufL, 32, 0, 0, 2, {0x0F, 0x01, 0}, 0, 2, {OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_32|OPA_Spare, 0} },
    { CPU_386|CPU_64, MOD_Op1Add|MOD_GasOnly|MOD_GasSufQ, 64, 0, 0, 2, {0x0F, 0x01, 0}, 0, 2, {OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_64|OPA_Spare, 0} }
};

static const x86_insn_info call_insn[] = {
    { CPU_Any, 0, 0, 0, 0, 0, {0, 0, 0}, 0, 1, {OPT_ImmNotSegOff|OPS_Any|OPA_JmpRel, 0, 0} },
    { CPU_Any, 0, 16, 0, 0, 0, {0, 0, 0}, 0, 1, {OPT_ImmNotSegOff|OPS_16|OPA_JmpRel, 0, 0} },
    { CPU_386|CPU_Not64, 0, 32, 0, 0, 0, {0, 0, 0}, 0, 1, {OPT_ImmNotSegOff|OPS_32|OPA_JmpRel, 0, 0} },
    { CPU_64, 0, 64, 0, 0, 0, {0, 0, 0}, 0, 1, {OPT_ImmNotSegOff|OPS_32|OPA_JmpRel, 0, 0} },
    { CPU_Any, 0, 16, 64, 0, 1, {0xE8, 0, 0}, 0, 1, {OPT_Imm|OPS_16|OPTM_Near|OPA_JmpRel, 0, 0} },
    { CPU_386|CPU_Not64, 0, 32, 0, 0, 1, {0xE8, 0, 0}, 0, 1, {OPT_Imm|OPS_32|OPTM_Near|OPA_JmpRel, 0, 0} },
    { CPU_64, 0, 64, 64, 0, 1, {0xE8, 0, 0}, 0, 1, {OPT_Imm|OPS_32|OPTM_Near|OPA_JmpRel, 0, 0} },
    { CPU_Any, 0, 0, 64, 0, 1, {0xE8, 0, 0}, 0, 1, {OPT_Imm|OPS_Any|OPTM_Near|OPA_JmpRel, 0, 0} },
    { CPU_Any, 0, 16, 0, 0, 1, {0xFF, 0, 0}, 2, 1, {OPT_RM|OPS_16|OPA_EA, 0, 0} },
    { CPU_386|CPU_Not64, 0, 32, 0, 0, 1, {0xFF, 0, 0}, 2, 1, {OPT_RM|OPS_32|OPA_EA, 0, 0} },
    { CPU_64, 0, 64, 64, 0, 1, {0xFF, 0, 0}, 2, 1, {OPT_RM|OPS_64|OPA_EA, 0, 0} },
    { CPU_Any, 0, 0, 64, 0, 1, {0xFF, 0, 0}, 2, 1, {OPT_Mem|OPS_Any|OPA_EA, 0, 0} },
    { CPU_Any, 0, 16, 64, 0, 1, {0xFF, 0, 0}, 2, 1, {OPT_RM|OPS_16|OPTM_Near|OPA_EA, 0, 0} },
    { CPU_386|CPU_Not64, 0, 32, 0, 0, 1, {0xFF, 0, 0}, 2, 1, {OPT_RM|OPS_32|OPTM_Near|OPA_EA, 0, 0} },
    { CPU_64, 0, 64, 64, 0, 1, {0xFF, 0, 0}, 2, 1, {OPT_RM|OPS_64|OPTM_Near|OPA_EA, 0, 0} },
    { CPU_Any, 0, 0, 64, 0, 1, {0xFF, 0, 0}, 2, 1, {OPT_Mem|OPS_Any|OPTM_Near|OPA_EA, 0, 0} },
    { CPU_Any, 0, 16, 0, 0, 1, {0xFF, 0, 0}, 3, 1, {OPT_Mem|OPS_16|OPTM_Far|OPA_EA, 0, 0} },
    { CPU_386, 0, 32, 0, 0, 1, {0xFF, 0, 0}, 3, 1, {OPT_Mem|OPS_32|OPTM_Far|OPA_EA, 0, 0} },
    { CPU_64, 0, 64, 0, 0, 1, {0xFF, 0, 0}, 3, 1, {OPT_Mem|OPS_64|OPTM_Far|OPA_EA, 0, 0} },
    { CPU_Any, 0, 0, 0, 0, 1, {0xFF, 0, 0}, 3, 1, {OPT_Mem|OPS_Any|OPTM_Far|OPA_EA, 0, 0} },
    { CPU_Not64, 0, 16, 0, 0, 1, {0x9A, 0, 0}, 3, 1, {OPT_Imm|OPS_16|OPTM_Far|OPA_JmpFar, 0, 0} },
    { CPU_386|CPU_Not64, 0, 32, 0, 0, 1, {0x9A, 0, 0}, 3, 1, {OPT_Imm|OPS_32|OPTM_Far|OPA_JmpFar, 0, 0} },
    { CPU_Not64, 0, 0, 0, 0, 1, {0x9A, 0, 0}, 3, 1, {OPT_Imm|OPS_Any|OPTM_Far|OPA_JmpFar, 0, 0} },
    { CPU_Not64, 0, 16, 0, 0, 1, {0x9A, 0, 0}, 3, 1, {OPT_Imm|OPS_16|OPA_JmpFar, 0, 0} },
    { CPU_386|CPU_Not64, 0, 32, 0, 0, 1, {0x9A, 0, 0}, 3, 1, {OPT_Imm|OPS_32|OPA_JmpFar, 0, 0} },
    { CPU_Not64, 0, 0, 0, 0, 1, {0x9A, 0, 0}, 3, 1, {OPT_Imm|OPS_Any|OPA_JmpFar, 0, 0} }
};

static const x86_insn_info jmp_insn[] = {
    { CPU_Any, 0, 0, 0, 0, 0, {0, 0, 0}, 0, 1, {OPT_ImmNotSegOff|OPS_Any|OPA_JmpRel, 0, 0} },
    { CPU_Any, 0, 16, 0, 0, 0, {0, 0, 0}, 0, 1, {OPT_ImmNotSegOff|OPS_16|OPA_JmpRel, 0, 0} },
    { CPU_386|CPU_Not64, 0, 32, 0, 0, 1, {0x00, 0, 0}, 0, 1, {OPT_ImmNotSegOff|OPS_32|OPA_JmpRel, 0, 0} },
    { CPU_64, 0, 64, 0, 0, 1, {0x00, 0, 0}, 0, 1, {OPT_ImmNotSegOff|OPS_32|OPA_JmpRel, 0, 0} },
    { CPU_Any, 0, 0, 64, 0, 1, {0xEB, 0, 0}, 0, 1, {OPT_Imm|OPS_Any|OPTM_Short|OPA_JmpRel, 0, 0} },
    { CPU_Any, 0, 16, 64, 0, 1, {0xE9, 0, 0}, 0, 1, {OPT_Imm|OPS_16|OPTM_Near|OPA_JmpRel, 0, 0} },
    { CPU_386|CPU_Not64, 0, 32, 0, 0, 1, {0xE9, 0, 0}, 0, 1, {OPT_Imm|OPS_32|OPTM_Near|OPA_JmpRel, 0, 0} },
    { CPU_64, 0, 64, 64, 0, 1, {0xE9, 0, 0}, 0, 1, {OPT_Imm|OPS_32|OPTM_Near|OPA_JmpRel, 0, 0} },
    { CPU_Any, 0, 0, 64, 0, 1, {0xE9, 0, 0}, 0, 1, {OPT_Imm|OPS_Any|OPTM_Near|OPA_JmpRel, 0, 0} },
    { CPU_Any, 0, 16, 64, 0, 1, {0xFF, 0, 0}, 4, 1, {OPT_RM|OPS_16|OPA_EA, 0, 0} },
    { CPU_386|CPU_Not64, 0, 32, 0, 0, 1, {0xFF, 0, 0}, 4, 1, {OPT_RM|OPS_32|OPA_EA, 0, 0} },
    { CPU_64, 0, 64, 64, 0, 1, {0xFF, 0, 0}, 4, 1, {OPT_RM|OPS_64|OPA_EA, 0, 0} },
    { CPU_Any, 0, 0, 64, 0, 1, {0xFF, 0, 0}, 4, 1, {OPT_Mem|OPS_Any|OPA_EA, 0, 0} },
    { CPU_Any, 0, 16, 64, 0, 1, {0xFF, 0, 0}, 4, 1, {OPT_RM|OPS_16|OPTM_Near|OPA_EA, 0, 0} },
    { CPU_386|CPU_Not64, 0, 32, 0, 0, 1, {0xFF, 0, 0}, 4, 1, {OPT_RM|OPS_32|OPTM_Near|OPA_EA, 0, 0} },
    { CPU_64, 0, 64, 64, 0, 1, {0xFF, 0, 0}, 4, 1, {OPT_RM|OPS_64|OPTM_Near|OPA_EA, 0, 0} },
    { CPU_Any, 0, 0, 64, 0, 1, {0xFF, 0, 0}, 4, 1, {OPT_Mem|OPS_Any|OPTM_Near|OPA_EA, 0, 0} },
    { CPU_Any, 0, 16, 0, 0, 1, {0xFF, 0, 0}, 5, 1, {OPT_Mem|OPS_16|OPTM_Far|OPA_EA, 0, 0} },
    { CPU_386, 0, 32, 0, 0, 1, {0xFF, 0, 0}, 5, 1, {OPT_Mem|OPS_32|OPTM_Far|OPA_EA, 0, 0} },
    { CPU_64, 0, 64, 0, 0, 1, {0xFF, 0, 0}, 5, 1, {OPT_Mem|OPS_64|OPTM_Far|OPA_EA, 0, 0} },
    { CPU_Any, 0, 0, 0, 0, 1, {0xFF, 0, 0}, 5, 1, {OPT_Mem|OPS_Any|OPTM_Far|OPA_EA, 0, 0} },
    { CPU_Not64, 0, 16, 0, 0, 1, {0xEA, 0, 0}, 3, 1, {OPT_Imm|OPS_16|OPTM_Far|OPA_JmpFar, 0, 0} },
    { CPU_386|CPU_Not64, 0, 32, 0, 0, 1, {0xEA, 0, 0}, 3, 1, {OPT_Imm|OPS_32|OPTM_Far|OPA_JmpFar, 0, 0} },
    { CPU_Not64, 0, 0, 0, 0, 1, {0xEA, 0, 0}, 3, 1, {OPT_Imm|OPS_Any|OPTM_Far|OPA_JmpFar, 0, 0} },
    { CPU_Not64, 0, 16, 0, 0, 1, {0xEA, 0, 0}, 3, 1, {OPT_Imm|OPS_16|OPA_JmpFar, 0, 0} },
    { CPU_386|CPU_Not64, 0, 32, 0, 0, 1, {0xEA, 0, 0}, 3, 1, {OPT_Imm|OPS_32|OPA_JmpFar, 0, 0} },
    { CPU_Not64, 0, 0, 0, 0, 1, {0xEA, 0, 0}, 3, 1, {OPT_Imm|OPS_Any|OPA_JmpFar, 0, 0} }
};

static const x86_insn_info retnf_insn[] = {
    { CPU_Not64, MOD_Op0Add, 0, 0, 0, 1, {0x01, 0, 0}, 0, 0, {0, 0, 0} },
    { CPU_Not64, MOD_Op0Add, 0, 0, 0, 1, {0x00, 0, 0}, 0, 1, {OPT_Imm|OPS_16|OPS_Relaxed|OPA_Imm, 0, 0} },
    { CPU_64, MOD_Op0Add|MOD_OpSizeR, 0, 0, 0, 1, {0x01, 0, 0}, 0, 0, {0, 0, 0} },
    { CPU_64, MOD_Op0Add|MOD_OpSizeR, 0, 0, 0, 1, {0x00, 0, 0}, 0, 1, {OPT_Imm|OPS_16|OPS_Relaxed|OPA_Imm, 0, 0} },
    { CPU_Any, MOD_Op0Add|MOD_OpSizeR|MOD_GasSufL|MOD_GasSufQ|MOD_GasSufW, 0, 0, 0, 1, {0x01, 0, 0}, 0, 0, {0, 0, 0} },
    { CPU_Any, MOD_Op0Add|MOD_OpSizeR|MOD_GasSufL|MOD_GasSufQ|MOD_GasSufW, 0, 0, 0, 1, {0x00, 0, 0}, 0, 1, {OPT_Imm|OPS_16|OPS_Relaxed|OPA_Imm, 0, 0} }
};

static const x86_insn_info enter_insn[] = {
    { CPU_186|CPU_Not64, MOD_GasNoRev|MOD_GasSufL, 0, 0, 0, 1, {0xC8, 0, 0}, 0, 2, {OPT_Imm|OPS_16|OPS_Relaxed|OPA_EA|OPAP_A16, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_186|CPU_64, MOD_GasNoRev|MOD_GasSufQ, 64, 64, 0, 1, {0xC8, 0, 0}, 0, 2, {OPT_Imm|OPS_16|OPS_Relaxed|OPA_EA|OPAP_A16, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_186, MOD_GasOnly|MOD_GasNoRev|MOD_GasSufW, 16, 0, 0, 1, {0xC8, 0, 0}, 0, 2, {OPT_Imm|OPS_16|OPS_Relaxed|OPA_EA|OPAP_A16, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} }
};

static const x86_insn_info jcc_insn[] = {
    { CPU_Any, 0, 0, 0, 0, 0, {0, 0, 0}, 0, 1, {OPT_Imm|OPS_Any|OPA_JmpRel, 0, 0} },
    { CPU_Any, 0, 16, 0, 0, 0, {0, 0, 0}, 0, 1, {OPT_Imm|OPS_16|OPA_JmpRel, 0, 0} },
    { CPU_386|CPU_Not64, 0, 32, 0, 0, 0, {0, 0, 0}, 0, 1, {OPT_Imm|OPS_32|OPA_JmpRel, 0, 0} },
    { CPU_64, 0, 64, 0, 0, 0, {0, 0, 0}, 0, 1, {OPT_Imm|OPS_32|OPA_JmpRel, 0, 0} },
    { CPU_Any, MOD_Op0Add, 0, 64, 0, 1, {0x70, 0, 0}, 0, 1, {OPT_Imm|OPS_Any|OPTM_Short|OPA_JmpRel, 0, 0} },
    { CPU_186, MOD_Op1Add, 16, 64, 0, 2, {0x0F, 0x80, 0}, 0, 1, {OPT_Imm|OPS_16|OPTM_Near|OPA_JmpRel, 0, 0} },
    { CPU_386|CPU_Not64, MOD_Op1Add, 32, 0, 0, 2, {0x0F, 0x80, 0}, 0, 1, {OPT_Imm|OPS_32|OPTM_Near|OPA_JmpRel, 0, 0} },
    { CPU_64, MOD_Op1Add, 64, 64, 0, 2, {0x0F, 0x80, 0}, 0, 1, {OPT_Imm|OPS_32|OPTM_Near|OPA_JmpRel, 0, 0} },
    { CPU_186, MOD_Op1Add, 0, 64, 0, 2, {0x0F, 0x80, 0}, 0, 1, {OPT_Imm|OPS_Any|OPTM_Near|OPA_JmpRel, 0, 0} }
};

static const x86_insn_info jcxz_insn[] = {
    { CPU_Any, MOD_AdSizeR, 0, 0, 0, 0, {0, 0, 0}, 0, 1, {OPT_Imm|OPS_Any|OPA_JmpRel, 0, 0} },
    { CPU_Any, MOD_AdSizeR, 0, 64, 0, 1, {0xE3, 0, 0}, 0, 1, {OPT_Imm|OPS_Any|OPTM_Short|OPA_JmpRel, 0, 0} }
};

static const x86_insn_info loop_insn[] = {
    { CPU_Any, 0, 0, 0, 0, 0, {0, 0, 0}, 0, 1, {OPT_Imm|OPS_Any|OPA_JmpRel, 0, 0} },
    { CPU_Not64, 0, 0, 0, 0, 0, {0, 0, 0}, 0, 2, {OPT_Imm|OPS_Any|OPA_JmpRel, OPT_Creg|OPS_16|OPA_AdSizeR, 0} },
    { CPU_386, 0, 0, 64, 0, 0, {0, 0, 0}, 0, 2, {OPT_Imm|OPS_Any|OPA_JmpRel, OPT_Creg|OPS_32|OPA_AdSizeR, 0} },
    { CPU_64, 0, 0, 64, 0, 0, {0, 0, 0}, 0, 2, {OPT_Imm|OPS_Any|OPA_JmpRel, OPT_Creg|OPS_64|OPA_AdSizeR, 0} },
    { CPU_Not64, MOD_Op0Add, 0, 0, 0, 1, {0xE0, 0, 0}, 0, 1, {OPT_Imm|OPS_Any|OPTM_Short|OPA_JmpRel, 0, 0} },
    { CPU_Any, MOD_Op0Add, 0, 64, 0, 1, {0xE0, 0, 0}, 0, 2, {OPT_Imm|OPS_Any|OPTM_Short|OPA_JmpRel, OPT_Creg|OPS_16|OPA_AdSizeR, 0} },
    { CPU_386, MOD_Op0Add, 0, 64, 0, 1, {0xE0, 0, 0}, 0, 2, {OPT_Imm|OPS_Any|OPTM_Short|OPA_JmpRel, OPT_Creg|OPS_32|OPA_AdSizeR, 0} },
    { CPU_64, MOD_Op0Add, 0, 64, 0, 1, {0xE0, 0, 0}, 0, 2, {OPT_Imm|OPS_Any|OPTM_Short|OPA_JmpRel, OPT_Creg|OPS_64|OPA_AdSizeR, 0} }
};

static const x86_insn_info setcc_insn[] = {
    { CPU_386, MOD_Op1Add|MOD_GasSufB, 0, 0, 0, 2, {0x0F, 0x90, 0}, 2, 1, {OPT_RM|OPS_8|OPS_Relaxed|OPA_EA, 0, 0} }
};

static const x86_insn_info cmpsd_insn[] = {
    { CPU_386, MOD_GasIllegal, 32, 0, 0, 1, {0xA7, 0, 0}, 0, 0, {0, 0, 0} },
    { CPU_SSE2, 0, 0, 0, 0xF2, 2, {0x0F, 0xC2, 0}, 0, 3, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDRM|OPS_128|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} }
};

static const x86_insn_info movsd_insn[] = {
    { CPU_386, MOD_GasIllegal, 32, 0, 0, 1, {0xA5, 0, 0}, 0, 0, {0, 0, 0} },
    { CPU_SSE2, 0, 0, 0, 0xF2, 2, {0x0F, 0x10, 0}, 0, 2, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, 0} },
    { CPU_SSE2, 0, 0, 0, 0xF2, 2, {0x0F, 0x10, 0}, 0, 2, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_Mem|OPS_64|OPS_Relaxed|OPA_EA, 0} },
    { CPU_SSE2, 0, 0, 0, 0xF2, 2, {0x0F, 0x11, 0}, 0, 2, {OPT_Mem|OPS_64|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, 0} }
};

static const x86_insn_info bittest_insn[] = {
    { CPU_386, MOD_Op1Add|MOD_GasSufW, 16, 0, 0, 2, {0x0F, 0x00, 0}, 0, 2, {OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_16|OPA_Spare, 0} },
    { CPU_386, MOD_Op1Add|MOD_GasSufL, 32, 0, 0, 2, {0x0F, 0x00, 0}, 0, 2, {OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_32|OPA_Spare, 0} },
    { CPU_386|CPU_64, MOD_Op1Add|MOD_GasSufQ, 64, 0, 0, 2, {0x0F, 0x00, 0}, 0, 2, {OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_64|OPA_Spare, 0} },
    { CPU_386, MOD_Gap0|MOD_SpAdd|MOD_GasSufW, 16, 0, 0, 2, {0x0F, 0xBA, 0}, 0, 2, {OPT_RM|OPS_16|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_386, MOD_Gap0|MOD_SpAdd|MOD_GasSufL, 32, 0, 0, 2, {0x0F, 0xBA, 0}, 0, 2, {OPT_RM|OPS_32|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_386|CPU_64, MOD_Gap0|MOD_SpAdd|MOD_GasSufQ, 64, 0, 0, 2, {0x0F, 0xBA, 0}, 0, 2, {OPT_RM|OPS_64|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} }
};

static const x86_insn_info bsfr_insn[] = {
    { CPU_386, MOD_Op1Add|MOD_GasSufW, 16, 0, 0, 2, {0x0F, 0x00, 0}, 0, 2, {OPT_Reg|OPS_16|OPA_Spare, OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, 0} },
    { CPU_386, MOD_Op1Add|MOD_GasSufL, 32, 0, 0, 2, {0x0F, 0x00, 0}, 0, 2, {OPT_Reg|OPS_32|OPA_Spare, OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, 0} },
    { CPU_386|CPU_64, MOD_Op1Add|MOD_GasSufQ, 64, 0, 0, 2, {0x0F, 0x00, 0}, 0, 2, {OPT_Reg|OPS_64|OPA_Spare, OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, 0} }
};

static const x86_insn_info int_insn[] = {
    { CPU_Any, 0, 0, 0, 0, 1, {0xCD, 0, 0}, 0, 1, {OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0, 0} }
};

static const x86_insn_info bound_insn[] = {
    { CPU_186|CPU_Not64, MOD_GasSufW, 16, 0, 0, 1, {0x62, 0, 0}, 0, 2, {OPT_Reg|OPS_16|OPA_Spare, OPT_Mem|OPS_16|OPS_Relaxed|OPA_EA, 0} },
    { CPU_386|CPU_Not64, MOD_GasSufL, 32, 0, 0, 1, {0x62, 0, 0}, 0, 2, {OPT_Reg|OPS_32|OPA_Spare, OPT_Mem|OPS_32|OPS_Relaxed|OPA_EA, 0} }
};

static const x86_insn_info arpl_insn[] = {
    { CPU_286|CPU_Not64|CPU_Prot, MOD_GasSufW, 0, 0, 0, 1, {0x63, 0, 0}, 0, 2, {OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_16|OPA_Spare, 0} }
};

static const x86_insn_info str_insn[] = {
    { CPU_286|CPU_Prot, MOD_GasSufW, 16, 0, 0, 2, {0x0F, 0x00, 0}, 1, 1, {OPT_Reg|OPS_16|OPA_EA, 0, 0} },
    { CPU_386|CPU_Prot, MOD_GasSufL, 32, 0, 0, 2, {0x0F, 0x00, 0}, 1, 1, {OPT_Reg|OPS_32|OPA_EA, 0, 0} },
    { CPU_286|CPU_64|CPU_Prot, MOD_GasSufQ, 64, 0, 0, 2, {0x0F, 0x00, 0}, 1, 1, {OPT_Reg|OPS_64|OPA_EA, 0, 0} },
    { CPU_286|CPU_Prot, MOD_GasSufL|MOD_GasSufW, 0, 0, 0, 2, {0x0F, 0x00, 0}, 1, 1, {OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, 0, 0} }
};

static const x86_insn_info prot286_insn[] = {
    { CPU_286, MOD_Op1Add|MOD_SpAdd|MOD_GasSufW, 0, 0, 0, 2, {0x0F, 0x00, 0}, 0, 1, {OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, 0, 0} }
};

static const x86_insn_info sldtmsw_insn[] = {
    { CPU_286, MOD_Op1Add|MOD_SpAdd|MOD_GasSufW, 0, 0, 0, 2, {0x0F, 0x00, 0}, 0, 1, {OPT_Mem|OPS_16|OPS_Relaxed|OPA_EA, 0, 0} },
    { CPU_386, MOD_Op1Add|MOD_SpAdd|MOD_GasSufL, 0, 0, 0, 2, {0x0F, 0x00, 0}, 0, 1, {OPT_Mem|OPS_32|OPS_Relaxed|OPA_EA, 0, 0} },
    { CPU_286|CPU_64, MOD_Op1Add|MOD_SpAdd|MOD_GasSufQ, 0, 0, 0, 2, {0x0F, 0x00, 0}, 0, 1, {OPT_Mem|OPS_64|OPS_Relaxed|OPA_EA, 0, 0} },
    { CPU_286, MOD_Op1Add|MOD_SpAdd|MOD_GasSufW, 16, 0, 0, 2, {0x0F, 0x00, 0}, 0, 1, {OPT_Reg|OPS_16|OPA_EA, 0, 0} },
    { CPU_386, MOD_Op1Add|MOD_SpAdd|MOD_GasSufL, 32, 0, 0, 2, {0x0F, 0x00, 0}, 0, 1, {OPT_Reg|OPS_32|OPA_EA, 0, 0} },
    { CPU_286|CPU_64, MOD_Op1Add|MOD_SpAdd|MOD_GasSufQ, 64, 0, 0, 2, {0x0F, 0x00, 0}, 0, 1, {OPT_Reg|OPS_64|OPA_EA, 0, 0} }
};

static const x86_insn_info fld_insn[] = {
    { CPU_FPU, MOD_GasSufS, 0, 0, 0, 1, {0xD9, 0, 0}, 0, 1, {OPT_Mem|OPS_32|OPA_EA, 0, 0} },
    { CPU_FPU, MOD_GasSufL, 0, 0, 0, 1, {0xDD, 0, 0}, 0, 1, {OPT_Mem|OPS_64|OPA_EA, 0, 0} },
    { CPU_FPU, 0, 0, 0, 0, 1, {0xDB, 0, 0}, 5, 1, {OPT_Mem|OPS_80|OPA_EA, 0, 0} },
    { CPU_FPU, 0, 0, 0, 0, 2, {0xD9, 0xC0, 0}, 0, 1, {OPT_Reg|OPS_80|OPA_Op1Add, 0, 0} }
};

static const x86_insn_info fstp_insn[] = {
    { CPU_FPU, MOD_GasSufS, 0, 0, 0, 1, {0xD9, 0, 0}, 3, 1, {OPT_Mem|OPS_32|OPA_EA, 0, 0} },
    { CPU_FPU, MOD_GasSufL, 0, 0, 0, 1, {0xDD, 0, 0}, 3, 1, {OPT_Mem|OPS_64|OPA_EA, 0, 0} },
    { CPU_FPU, 0, 0, 0, 0, 1, {0xDB, 0, 0}, 7, 1, {OPT_Mem|OPS_80|OPA_EA, 0, 0} },
    { CPU_FPU, 0, 0, 0, 0, 2, {0xDD, 0xD8, 0}, 0, 1, {OPT_Reg|OPS_80|OPA_Op1Add, 0, 0} }
};

static const x86_insn_info fldstpt_insn[] = {
    { CPU_FPU, MOD_SpAdd, 0, 0, 0, 1, {0xDB, 0, 0}, 0, 1, {OPT_Mem|OPS_80|OPA_EA, 0, 0} }
};

static const x86_insn_info fildstp_insn[] = {
    { CPU_FPU, MOD_SpAdd|MOD_GasSufS, 0, 0, 0, 1, {0xDF, 0, 0}, 0, 1, {OPT_Mem|OPS_16|OPA_EA, 0, 0} },
    { CPU_FPU, MOD_SpAdd|MOD_GasSufL, 0, 0, 0, 1, {0xDB, 0, 0}, 0, 1, {OPT_Mem|OPS_32|OPA_EA, 0, 0} },
    { CPU_FPU, MOD_Gap0|MOD_Op0Add|MOD_SpAdd|MOD_GasSufQ, 0, 0, 0, 1, {0xDD, 0, 0}, 0, 1, {OPT_Mem|OPS_64|OPA_EA, 0, 0} }
};

static const x86_insn_info fbldstp_insn[] = {
    { CPU_FPU, MOD_SpAdd, 0, 0, 0, 1, {0xDF, 0, 0}, 0, 1, {OPT_Mem|OPS_80|OPS_Relaxed|OPA_EA, 0, 0} }
};

static const x86_insn_info fst_insn[] = {
    { CPU_FPU, MOD_GasSufS, 0, 0, 0, 1, {0xD9, 0, 0}, 2, 1, {OPT_Mem|OPS_32|OPA_EA, 0, 0} },
    { CPU_FPU, MOD_GasSufL, 0, 0, 0, 1, {0xDD, 0, 0}, 2, 1, {OPT_Mem|OPS_64|OPA_EA, 0, 0} },
    { CPU_FPU, 0, 0, 0, 0, 2, {0xDD, 0xD0, 0}, 0, 1, {OPT_Reg|OPS_80|OPA_Op1Add, 0, 0} }
};

static const x86_insn_info fxch_insn[] = {
    { CPU_FPU, 0, 0, 0, 0, 2, {0xD9, 0xC8, 0}, 0, 1, {OPT_Reg|OPS_80|OPA_Op1Add, 0, 0} },
    { CPU_FPU, 0, 0, 0, 0, 2, {0xD9, 0xC8, 0}, 0, 2, {OPT_ST0|OPS_80|OPA_None, OPT_Reg|OPS_80|OPA_Op1Add, 0} },
    { CPU_FPU, 0, 0, 0, 0, 2, {0xD9, 0xC8, 0}, 0, 2, {OPT_Reg|OPS_80|OPA_Op1Add, OPT_ST0|OPS_80|OPA_None, 0} },
    { CPU_FPU, 0, 0, 0, 0, 2, {0xD9, 0xC9, 0}, 0, 0, {0, 0, 0} }
};

static const x86_insn_info fcom_insn[] = {
    { CPU_FPU, MOD_Gap0|MOD_SpAdd|MOD_GasSufS, 0, 0, 0, 1, {0xD8, 0, 0}, 0, 1, {OPT_Mem|OPS_32|OPA_EA, 0, 0} },
    { CPU_FPU, MOD_Gap0|MOD_SpAdd|MOD_GasSufL, 0, 0, 0, 1, {0xDC, 0, 0}, 0, 1, {OPT_Mem|OPS_64|OPA_EA, 0, 0} },
    { CPU_FPU, MOD_Op1Add, 0, 0, 0, 2, {0xD8, 0x00, 0}, 0, 1, {OPT_Reg|OPS_80|OPA_Op1Add, 0, 0} },
    { CPU_FPU, MOD_Op1Add|MOD_GasOnly, 0, 0, 0, 2, {0xD8, 0x01, 0}, 0, 0, {0, 0, 0} },
    { CPU_FPU, MOD_Op1Add|MOD_GasIllegal, 0, 0, 0, 2, {0xD8, 0x00, 0}, 0, 2, {OPT_ST0|OPS_80|OPA_None, OPT_Reg|OPS_80|OPA_Op1Add, 0} }
};

static const x86_insn_info fcom2_insn[] = {
    { CPU_286|CPU_FPU, MOD_Op1Add|MOD_Op0Add, 0, 0, 0, 2, {0x00, 0x00, 0}, 0, 1, {OPT_Reg|OPS_80|OPA_Op1Add, 0, 0} },
    { CPU_286|CPU_FPU, MOD_Op1Add|MOD_Op0Add, 0, 0, 0, 2, {0x00, 0x00, 0}, 0, 2, {OPT_ST0|OPS_80|OPA_None, OPT_Reg|OPS_80|OPA_Op1Add, 0} }
};

static const x86_insn_info farith_insn[] = {
    { CPU_FPU, MOD_Gap0|MOD_Gap1|MOD_SpAdd|MOD_GasSufS, 0, 0, 0, 1, {0xD8, 0, 0}, 0, 1, {OPT_Mem|OPS_32|OPA_EA, 0, 0} },
    { CPU_FPU, MOD_Gap0|MOD_Gap1|MOD_SpAdd|MOD_GasSufL, 0, 0, 0, 1, {0xDC, 0, 0}, 0, 1, {OPT_Mem|OPS_64|OPA_EA, 0, 0} },
    { CPU_FPU, MOD_Gap0|MOD_Op1Add, 0, 0, 0, 2, {0xD8, 0x00, 0}, 0, 1, {OPT_Reg|OPS_80|OPA_Op1Add, 0, 0} },
    { CPU_FPU, MOD_Gap0|MOD_Op1Add, 0, 0, 0, 2, {0xD8, 0x00, 0}, 0, 2, {OPT_ST0|OPS_80|OPA_None, OPT_Reg|OPS_80|OPA_Op1Add, 0} },
    { CPU_FPU, MOD_Op1Add, 0, 0, 0, 2, {0xDC, 0x00, 0}, 0, 1, {OPT_Reg|OPS_80|OPTM_To|OPA_Op1Add, 0, 0} },
    { CPU_FPU, MOD_Op1Add|MOD_GasIllegal, 0, 0, 0, 2, {0xDC, 0x00, 0}, 0, 2, {OPT_Reg|OPS_80|OPA_Op1Add, OPT_ST0|OPS_80|OPA_None, 0} },
    { CPU_FPU, MOD_Gap0|MOD_Op1Add|MOD_GasOnly, 0, 0, 0, 2, {0xDC, 0x00, 0}, 0, 2, {OPT_Reg|OPS_80|OPA_Op1Add, OPT_ST0|OPS_80|OPA_None, 0} }
};

static const x86_insn_info farithp_insn[] = {
    { CPU_FPU, MOD_Op1Add, 0, 0, 0, 2, {0xDE, 0x01, 0}, 0, 0, {0, 0, 0} },
    { CPU_FPU, MOD_Op1Add, 0, 0, 0, 2, {0xDE, 0x00, 0}, 0, 1, {OPT_Reg|OPS_80|OPA_Op1Add, 0, 0} },
    { CPU_FPU, MOD_Op1Add, 0, 0, 0, 2, {0xDE, 0x00, 0}, 0, 2, {OPT_Reg|OPS_80|OPA_Op1Add, OPT_ST0|OPS_80|OPA_None, 0} }
};

static const x86_insn_info fiarith_insn[] = {
    { CPU_FPU, MOD_Op0Add|MOD_SpAdd|MOD_GasSufS, 0, 0, 0, 1, {0x04, 0, 0}, 0, 1, {OPT_Mem|OPS_16|OPA_EA, 0, 0} },
    { CPU_FPU, MOD_Op0Add|MOD_SpAdd|MOD_GasSufL, 0, 0, 0, 1, {0x00, 0, 0}, 0, 1, {OPT_Mem|OPS_32|OPA_EA, 0, 0} }
};

static const x86_insn_info fldnstcw_insn[] = {
    { CPU_FPU, MOD_SpAdd|MOD_GasSufW, 0, 0, 0, 1, {0xD9, 0, 0}, 0, 1, {OPT_Mem|OPS_16|OPS_Relaxed|OPA_EA, 0, 0} }
};

static const x86_insn_info fstcw_insn[] = {
    { CPU_FPU, MOD_GasSufW, 0, 0, 0, 2, {0x9B, 0xD9, 0}, 7, 1, {OPT_Mem|OPS_16|OPS_Relaxed|OPA_EA, 0, 0} }
};

static const x86_insn_info fnstsw_insn[] = {
    { CPU_FPU, MOD_GasSufW, 0, 0, 0, 1, {0xDD, 0, 0}, 7, 1, {OPT_Mem|OPS_16|OPS_Relaxed|OPA_EA, 0, 0} },
    { CPU_FPU, MOD_GasSufW, 0, 0, 0, 2, {0xDF, 0xE0, 0}, 0, 1, {OPT_Areg|OPS_16|OPA_None, 0, 0} }
};

static const x86_insn_info fstsw_insn[] = {
    { CPU_FPU, MOD_GasSufW, 0, 0, 0, 2, {0x9B, 0xDD, 0}, 7, 1, {OPT_Mem|OPS_16|OPS_Relaxed|OPA_EA, 0, 0} },
    { CPU_FPU, MOD_GasSufW, 0, 0, 0, 3, {0x9B, 0xDF, 0xE0}, 0, 1, {OPT_Areg|OPS_16|OPA_None, 0, 0} }
};

static const x86_insn_info ffree_insn[] = {
    { CPU_FPU, MOD_Op0Add, 0, 0, 0, 2, {0x00, 0xC0, 0}, 0, 1, {OPT_Reg|OPS_80|OPA_Op1Add, 0, 0} }
};

static const x86_insn_info bswap_insn[] = {
    { CPU_486, MOD_GasSufL, 32, 0, 0, 2, {0x0F, 0xC8, 0}, 0, 1, {OPT_Reg|OPS_32|OPA_Op1Add, 0, 0} },
    { CPU_64, MOD_GasSufQ, 64, 0, 0, 2, {0x0F, 0xC8, 0}, 0, 1, {OPT_Reg|OPS_64|OPA_Op1Add, 0, 0} }
};

static const x86_insn_info cmpxchgxadd_insn[] = {
    { CPU_486, MOD_Op1Add|MOD_GasSufB, 0, 0, 0, 2, {0x0F, 0x00, 0}, 0, 2, {OPT_RM|OPS_8|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_8|OPA_Spare, 0} },
    { CPU_486, MOD_Op1Add|MOD_GasSufW, 16, 0, 0, 2, {0x0F, 0x01, 0}, 0, 2, {OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_16|OPA_Spare, 0} },
    { CPU_486, MOD_Op1Add|MOD_GasSufL, 32, 0, 0, 2, {0x0F, 0x01, 0}, 0, 2, {OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_32|OPA_Spare, 0} },
    { CPU_486|CPU_64, MOD_Op1Add|MOD_GasSufQ, 64, 0, 0, 2, {0x0F, 0x01, 0}, 0, 2, {OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_64|OPA_Spare, 0} }
};

static const x86_insn_info cmpxchg8b_insn[] = {
    { CPU_586, MOD_GasSufQ, 0, 0, 0, 2, {0x0F, 0xC7, 0}, 1, 1, {OPT_Mem|OPS_64|OPS_Relaxed|OPA_EA, 0, 0} }
};

static const x86_insn_info cmovcc_insn[] = {
    { CPU_686, MOD_Op1Add|MOD_GasSufW, 16, 0, 0, 2, {0x0F, 0x40, 0}, 0, 2, {OPT_Reg|OPS_16|OPA_Spare, OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, 0} },
    { CPU_686, MOD_Op1Add|MOD_GasSufL, 32, 0, 0, 2, {0x0F, 0x40, 0}, 0, 2, {OPT_Reg|OPS_32|OPA_Spare, OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, 0} },
    { CPU_64|CPU_686, MOD_Op1Add|MOD_GasSufQ, 64, 0, 0, 2, {0x0F, 0x40, 0}, 0, 2, {OPT_Reg|OPS_64|OPA_Spare, OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, 0} }
};

static const x86_insn_info fcmovcc_insn[] = {
    { CPU_686|CPU_FPU, MOD_Op1Add|MOD_Op0Add, 0, 0, 0, 2, {0x00, 0x00, 0}, 0, 2, {OPT_ST0|OPS_80|OPA_None, OPT_Reg|OPS_80|OPA_Op1Add, 0} }
};

static const x86_insn_info movnti_insn[] = {
    { CPU_P4, MOD_GasSufL, 0, 0, 0, 2, {0x0F, 0xC3, 0}, 0, 2, {OPT_Mem|OPS_32|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_32|OPA_Spare, 0} },
    { CPU_64|CPU_P4, MOD_GasSufQ, 64, 0, 0, 2, {0x0F, 0xC3, 0}, 0, 2, {OPT_Mem|OPS_64|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_64|OPA_Spare, 0} }
};

static const x86_insn_info clflush_insn[] = {
    { CPU_P3, 0, 0, 0, 0, 2, {0x0F, 0xAE, 0}, 7, 1, {OPT_Mem|OPS_8|OPS_Relaxed|OPA_EA, 0, 0} }
};

static const x86_insn_info movd_insn[] = {
    { CPU_386|CPU_MMX, 0, 0, 0, 0, 2, {0x0F, 0x6E, 0}, 0, 2, {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, 0} },
    { CPU_64|CPU_MMX, 0, 64, 0, 0, 2, {0x0F, 0x6E, 0}, 0, 2, {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, 0} },
    { CPU_386|CPU_MMX, 0, 0, 0, 0, 2, {0x0F, 0x7E, 0}, 0, 2, {OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_64|OPA_Spare, 0} },
    { CPU_64|CPU_MMX, 0, 64, 0, 0, 2, {0x0F, 0x7E, 0}, 0, 2, {OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_64|OPA_Spare, 0} },
    { CPU_386|CPU_SSE2, 0, 0, 0, 0x66, 2, {0x0F, 0x6E, 0}, 0, 2, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, 0} },
    { CPU_64|CPU_SSE2, 0, 64, 0, 0x66, 2, {0x0F, 0x6E, 0}, 0, 2, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, 0} },
    { CPU_386|CPU_SSE2, 0, 0, 0, 0x66, 2, {0x0F, 0x7E, 0}, 0, 2, {OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, 0} },
    { CPU_64|CPU_SSE2, 0, 64, 0, 0x66, 2, {0x0F, 0x7E, 0}, 0, 2, {OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, 0} }
};

static const x86_insn_info movq_insn[] = {
    { CPU_MMX, MOD_GasIllegal, 0, 0, 0, 2, {0x0F, 0x6F, 0}, 0, 2, {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_SIMDRM|OPS_64|OPS_Relaxed|OPA_EA, 0} },
    { CPU_64|CPU_MMX, MOD_GasIllegal, 64, 0, 0, 2, {0x0F, 0x6E, 0}, 0, 2, {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, 0} },
    { CPU_MMX, MOD_GasIllegal, 0, 0, 0, 2, {0x0F, 0x7F, 0}, 0, 2, {OPT_SIMDRM|OPS_64|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_64|OPA_Spare, 0} },
    { CPU_64|CPU_MMX, MOD_GasIllegal, 64, 0, 0, 2, {0x0F, 0x7E, 0}, 0, 2, {OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_64|OPA_Spare, 0} },
    { CPU_SSE2, MOD_GasIllegal, 0, 0, 0xF3, 2, {0x0F, 0x7E, 0}, 0, 2, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, 0} },
    { CPU_SSE2, MOD_GasIllegal, 0, 0, 0xF3, 2, {0x0F, 0x7E, 0}, 0, 2, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDRM|OPS_64|OPS_Relaxed|OPA_EA, 0} },
    { CPU_64|CPU_SSE2, MOD_GasIllegal, 64, 0, 0x66, 2, {0x0F, 0x6E, 0}, 0, 2, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, 0} },
    { CPU_SSE2, MOD_GasIllegal, 0, 0, 0x66, 2, {0x0F, 0xD6, 0}, 0, 2, {OPT_SIMDRM|OPS_64|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, 0} },
    { CPU_64|CPU_SSE2, MOD_GasIllegal, 64, 0, 0x66, 2, {0x0F, 0x7E, 0}, 0, 2, {OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, 0} }
};

static const x86_insn_info mmxsse2_insn[] = {
    { CPU_MMX, MOD_Op1Add, 0, 0, 0, 2, {0x0F, 0x00, 0}, 0, 2, {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_SIMDRM|OPS_64|OPS_Relaxed|OPA_EA, 0} },
    { CPU_SSE2, MOD_Op1Add, 0, 0, 0x66, 2, {0x0F, 0x00, 0}, 0, 2, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDRM|OPS_128|OPS_Relaxed|OPA_EA, 0} }
};

static const x86_insn_info pshift_insn[] = {
    { CPU_MMX, MOD_Op1Add, 0, 0, 0, 2, {0x0F, 0x00, 0}, 0, 2, {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_SIMDRM|OPS_64|OPS_Relaxed|OPA_EA, 0} },
    { CPU_MMX, MOD_Gap0|MOD_Op1Add|MOD_SpAdd, 0, 0, 0, 2, {0x0F, 0x00, 0}, 0, 2, {OPT_SIMDReg|OPS_64|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} },
    { CPU_SSE2, MOD_Op1Add, 0, 0, 0x66, 2, {0x0F, 0x00, 0}, 0, 2, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDRM|OPS_128|OPS_Relaxed|OPA_EA, 0} },
    { CPU_SSE2, MOD_Gap0|MOD_Op1Add|MOD_SpAdd, 0, 0, 0x66, 2, {0x0F, 0x00, 0}, 0, 2, {OPT_SIMDReg|OPS_128|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} }
};

static const x86_insn_info sseps_insn[] = {
    { CPU_SSE, MOD_Op1Add, 0, 0, 0, 2, {0x0F, 0x00, 0}, 0, 2, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDRM|OPS_128|OPS_Relaxed|OPA_EA, 0} }
};

static const x86_insn_info cvt_rx_xmm32_insn[] = {
    { CPU_386|CPU_SSE, MOD_Op1Add|MOD_PreAdd|MOD_GasSufL, 0, 0, 0x00, 2, {0x0F, 0x00, 0}, 0, 2, {OPT_Reg|OPS_32|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, 0} },
    { CPU_386|CPU_SSE, MOD_Op1Add|MOD_PreAdd|MOD_GasSufL, 0, 0, 0x00, 2, {0x0F, 0x00, 0}, 0, 2, {OPT_Reg|OPS_32|OPA_Spare, OPT_Mem|OPS_32|OPS_Relaxed|OPA_EA, 0} },
    { CPU_64|CPU_SSE, MOD_Op1Add|MOD_PreAdd|MOD_GasSufQ, 64, 0, 0x00, 2, {0x0F, 0x00, 0}, 0, 2, {OPT_Reg|OPS_64|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, 0} },
    { CPU_64|CPU_SSE, MOD_Op1Add|MOD_PreAdd|MOD_GasSufQ, 64, 0, 0x00, 2, {0x0F, 0x00, 0}, 0, 2, {OPT_Reg|OPS_64|OPA_Spare, OPT_Mem|OPS_32|OPS_Relaxed|OPA_EA, 0} }
};

static const x86_insn_info cvt_mm_xmm64_insn[] = {
    { CPU_SSE, MOD_Op1Add, 0, 0, 0, 2, {0x0F, 0x00, 0}, 0, 2, {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, 0} },
    { CPU_SSE, MOD_Op1Add, 0, 0, 0, 2, {0x0F, 0x00, 0}, 0, 2, {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_Mem|OPS_64|OPS_Relaxed|OPA_EA, 0} }
};

static const x86_insn_info cvt_xmm_mm_ps_insn[] = {
    { CPU_SSE, MOD_Op1Add, 0, 0, 0, 2, {0x0F, 0x00, 0}, 0, 2, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDRM|OPS_64|OPS_Relaxed|OPA_EA, 0} }
};

static const x86_insn_info cvt_xmm_rmx_insn[] = {
    { CPU_386|CPU_SSE, MOD_Op1Add|MOD_PreAdd|MOD_GasSufL, 0, 0, 0x00, 2, {0x0F, 0x00, 0}, 0, 2, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, 0} },
    { CPU_64|CPU_SSE, MOD_Op1Add|MOD_PreAdd|MOD_GasSufQ, 64, 0, 0x00, 2, {0x0F, 0x00, 0}, 0, 2, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, 0} }
};

static const x86_insn_info ssess_insn[] = {
    { CPU_SSE, MOD_Op1Add|MOD_PreAdd, 0, 0, 0x00, 2, {0x0F, 0x00, 0}, 0, 2, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDRM|OPS_128|OPS_Relaxed|OPA_EA, 0} }
};

static const x86_insn_info ssecmpps_insn[] = {
    { CPU_SSE, MOD_Imm8, 0, 0, 0, 2, {0x0F, 0xC2, 0}, 0, 2, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDRM|OPS_128|OPS_Relaxed|OPA_EA, 0} }
};

static const x86_insn_info ssecmpss_insn[] = {
    { CPU_SSE, MOD_PreAdd|MOD_Imm8, 0, 0, 0x00, 2, {0x0F, 0xC2, 0}, 0, 2, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDRM|OPS_128|OPS_Relaxed|OPA_EA, 0} }
};

static const x86_insn_info ssepsimm_insn[] = {
    { CPU_SSE, MOD_Op1Add, 0, 0, 0, 2, {0x0F, 0x00, 0}, 0, 3, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDRM|OPS_128|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} }
};

static const x86_insn_info ssessimm_insn[] = {
    { CPU_SSE, MOD_Op1Add|MOD_PreAdd, 0, 0, 0x00, 2, {0x0F, 0x00, 0}, 0, 3, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDRM|OPS_128|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} }
};

static const x86_insn_info ldstmxcsr_insn[] = {
    { CPU_SSE, MOD_SpAdd, 0, 0, 0, 2, {0x0F, 0xAE, 0}, 0, 1, {OPT_Mem|OPS_32|OPS_Relaxed|OPA_EA, 0, 0} }
};

static const x86_insn_info maskmovq_insn[] = {
    { CPU_MMX|CPU_P3, 0, 0, 0, 0, 2, {0x0F, 0xF7, 0}, 0, 2, {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_SIMDReg|OPS_64|OPA_EA, 0} }
};

static const x86_insn_info movaups_insn[] = {
    { CPU_SSE, MOD_Op1Add, 0, 0, 0, 2, {0x0F, 0x00, 0}, 0, 2, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDRM|OPS_128|OPS_Relaxed|OPA_EA, 0} },
    { CPU_SSE, MOD_Op1Add, 0, 0, 0, 2, {0x0F, 0x01, 0}, 0, 2, {OPT_SIMDRM|OPS_128|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, 0} }
};

static const x86_insn_info movhllhps_insn[] = {
    { CPU_SSE, MOD_Op1Add, 0, 0, 0, 2, {0x0F, 0x00, 0}, 0, 2, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, 0} }
};

static const x86_insn_info movhlps_insn[] = {
    { CPU_SSE, MOD_Op1Add, 0, 0, 0, 2, {0x0F, 0x00, 0}, 0, 2, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_Mem|OPS_64|OPS_Relaxed|OPA_EA, 0} },
    { CPU_SSE, MOD_Op1Add, 0, 0, 0, 2, {0x0F, 0x01, 0}, 0, 2, {OPT_Mem|OPS_64|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, 0} }
};

static const x86_insn_info movmskps_insn[] = {
    { CPU_386|CPU_SSE, MOD_GasSufL, 0, 0, 0, 2, {0x0F, 0x50, 0}, 0, 2, {OPT_Reg|OPS_32|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, 0} },
    { CPU_64|CPU_SSE, MOD_GasSufQ, 64, 0, 0, 2, {0x0F, 0x50, 0}, 0, 2, {OPT_Reg|OPS_64|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, 0} }
};

static const x86_insn_info movntps_insn[] = {
    { CPU_SSE, 0, 0, 0, 0, 2, {0x0F, 0x2B, 0}, 0, 2, {OPT_Mem|OPS_128|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, 0} }
};

static const x86_insn_info movntq_insn[] = {
    { CPU_SSE, 0, 0, 0, 0, 2, {0x0F, 0xE7, 0}, 0, 2, {OPT_Mem|OPS_64|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_64|OPA_Spare, 0} }
};

static const x86_insn_info movss_insn[] = {
    { CPU_SSE, 0, 0, 0, 0xF3, 2, {0x0F, 0x10, 0}, 0, 2, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, 0} },
    { CPU_SSE, 0, 0, 0, 0xF3, 2, {0x0F, 0x10, 0}, 0, 2, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_Mem|OPS_32|OPS_Relaxed|OPA_EA, 0} },
    { CPU_SSE, 0, 0, 0, 0xF3, 2, {0x0F, 0x11, 0}, 0, 2, {OPT_Mem|OPS_32|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, 0} }
};

static const x86_insn_info pextrw_insn[] = {
    { CPU_MMX|CPU_P3, MOD_GasSufL, 0, 0, 0, 2, {0x0F, 0xC5, 0}, 0, 3, {OPT_Reg|OPS_32|OPA_Spare, OPT_SIMDReg|OPS_64|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} },
    { CPU_386|CPU_SSE2, MOD_GasSufL, 0, 0, 0x66, 2, {0x0F, 0xC5, 0}, 0, 3, {OPT_Reg|OPS_32|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} },
    { CPU_64|CPU_MMX|CPU_P3, MOD_GasSufQ, 64, 0, 0, 2, {0x0F, 0xC5, 0}, 0, 3, {OPT_Reg|OPS_64|OPA_Spare, OPT_SIMDReg|OPS_64|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} },
    { CPU_64|CPU_SSE2, MOD_GasSufQ, 64, 0, 0x66, 2, {0x0F, 0xC5, 0}, 0, 3, {OPT_Reg|OPS_64|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} },
    { CPU_SSE41, 0, 0, 0, 0x66, 3, {0x0F, 0x3A, 0x15}, 0, 3, {OPT_Mem|OPS_16|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} },
    { CPU_386|CPU_SSE41, 0, 32, 0, 0x66, 3, {0x0F, 0x3A, 0x15}, 0, 3, {OPT_Reg|OPS_32|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} },
    { CPU_64|CPU_SSE41, 0, 64, 0, 0x66, 3, {0x0F, 0x3A, 0x15}, 0, 3, {OPT_Reg|OPS_64|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} }
};

static const x86_insn_info pinsrw_insn[] = {
    { CPU_MMX|CPU_P3, MOD_GasSufL, 0, 0, 0, 2, {0x0F, 0xC4, 0}, 0, 3, {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_Reg|OPS_32|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} },
    { CPU_64|CPU_MMX|CPU_P3, MOD_GasSufQ, 64, 0, 0, 2, {0x0F, 0xC4, 0}, 0, 3, {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_Reg|OPS_64|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} },
    { CPU_MMX|CPU_P3, MOD_GasSufL, 0, 0, 0, 2, {0x0F, 0xC4, 0}, 0, 3, {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_Mem|OPS_16|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} },
    { CPU_386|CPU_SSE2, MOD_GasSufL, 0, 0, 0x66, 2, {0x0F, 0xC4, 0}, 0, 3, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_Reg|OPS_32|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} },
    { CPU_64|CPU_SSE2, MOD_GasSufQ, 64, 0, 0x66, 2, {0x0F, 0xC4, 0}, 0, 3, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_Reg|OPS_64|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} },
    { CPU_SSE2, MOD_GasSufL, 0, 0, 0x66, 2, {0x0F, 0xC4, 0}, 0, 3, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_Mem|OPS_16|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} }
};

static const x86_insn_info pmovmskb_insn[] = {
    { CPU_MMX|CPU_P3, MOD_GasSufL, 0, 0, 0, 2, {0x0F, 0xD7, 0}, 0, 2, {OPT_Reg|OPS_32|OPA_Spare, OPT_SIMDReg|OPS_64|OPA_EA, 0} },
    { CPU_386|CPU_SSE2, MOD_GasSufL, 0, 0, 0x66, 2, {0x0F, 0xD7, 0}, 0, 2, {OPT_Reg|OPS_32|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, 0} },
    { CPU_64|CPU_MMX|CPU_P3, MOD_GasSufQ, 64, 0, 0, 2, {0x0F, 0xD7, 0}, 0, 2, {OPT_Reg|OPS_64|OPA_Spare, OPT_SIMDReg|OPS_64|OPA_EA, 0} },
    { CPU_64|CPU_SSE2, MOD_GasSufQ, 64, 0, 0x66, 2, {0x0F, 0xD7, 0}, 0, 2, {OPT_Reg|OPS_64|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, 0} }
};

static const x86_insn_info pshufw_insn[] = {
    { CPU_MMX|CPU_P3, 0, 0, 0, 0, 2, {0x0F, 0x70, 0}, 0, 3, {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_SIMDRM|OPS_64|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} }
};

static const x86_insn_info cvt_xmm_xmm64_ss_insn[] = {
    { CPU_SSE2, MOD_Op1Add|MOD_PreAdd, 0, 0, 0x00, 2, {0x0F, 0x00, 0}, 0, 2, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, 0} },
    { CPU_SSE2, MOD_Op1Add|MOD_PreAdd, 0, 0, 0x00, 2, {0x0F, 0x00, 0}, 0, 2, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_Mem|OPS_64|OPS_Relaxed|OPA_EA, 0} }
};

static const x86_insn_info cvt_xmm_xmm64_ps_insn[] = {
    { CPU_SSE2, MOD_Op1Add, 0, 0, 0, 2, {0x0F, 0x00, 0}, 0, 2, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, 0} },
    { CPU_SSE2, MOD_Op1Add, 0, 0, 0, 2, {0x0F, 0x00, 0}, 0, 2, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_Mem|OPS_64|OPS_Relaxed|OPA_EA, 0} }
};

static const x86_insn_info cvt_rx_xmm64_insn[] = {
    { CPU_386|CPU_SSE2, MOD_Op1Add|MOD_PreAdd|MOD_GasSufL, 0, 0, 0x00, 2, {0x0F, 0x00, 0}, 0, 2, {OPT_Reg|OPS_32|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, 0} },
    { CPU_386|CPU_SSE2, MOD_Op1Add|MOD_PreAdd|MOD_GasSufL, 0, 0, 0x00, 2, {0x0F, 0x00, 0}, 0, 2, {OPT_Reg|OPS_32|OPA_Spare, OPT_Mem|OPS_64|OPS_Relaxed|OPA_EA, 0} },
    { CPU_64|CPU_SSE2, MOD_Op1Add|MOD_PreAdd|MOD_GasSufQ, 64, 0, 0x00, 2, {0x0F, 0x00, 0}, 0, 2, {OPT_Reg|OPS_64|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, 0} },
    { CPU_64|CPU_SSE2, MOD_Op1Add|MOD_PreAdd|MOD_GasSufQ, 64, 0, 0x00, 2, {0x0F, 0x00, 0}, 0, 2, {OPT_Reg|OPS_64|OPA_Spare, OPT_Mem|OPS_64|OPS_Relaxed|OPA_EA, 0} }
};

static const x86_insn_info cvt_mm_xmm_insn[] = {
    { CPU_SSE2, MOD_Op1Add|MOD_PreAdd, 0, 0, 0x00, 2, {0x0F, 0x00, 0}, 0, 2, {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_SIMDRM|OPS_128|OPS_Relaxed|OPA_EA, 0} }
};

static const x86_insn_info cvt_xmm_mm_ss_insn[] = {
    { CPU_SSE, MOD_Op1Add|MOD_PreAdd, 0, 0, 0x00, 2, {0x0F, 0x00, 0}, 0, 2, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDRM|OPS_64|OPS_Relaxed|OPA_EA, 0} }
};

static const x86_insn_info movaupd_insn[] = {
    { CPU_SSE2, MOD_Op1Add, 0, 0, 0x66, 2, {0x0F, 0x00, 0}, 0, 2, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDRM|OPS_128|OPS_Relaxed|OPA_EA, 0} },
    { CPU_SSE2, MOD_Op1Add, 0, 0, 0x66, 2, {0x0F, 0x01, 0}, 0, 2, {OPT_SIMDRM|OPS_128|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, 0} }
};

static const x86_insn_info movhlpd_insn[] = {
    { CPU_SSE2, MOD_Op1Add, 0, 0, 0x66, 2, {0x0F, 0x00, 0}, 0, 2, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_Mem|OPS_64|OPS_Relaxed|OPA_EA, 0} },
    { CPU_SSE2, MOD_Op1Add, 0, 0, 0x66, 2, {0x0F, 0x01, 0}, 0, 2, {OPT_Mem|OPS_64|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, 0} }
};

static const x86_insn_info movmskpd_insn[] = {
    { CPU_386|CPU_SSE2, MOD_GasSufL, 0, 0, 0x66, 2, {0x0F, 0x50, 0}, 0, 2, {OPT_Reg|OPS_32|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, 0} },
    { CPU_64|CPU_SSE2, MOD_GasSufQ, 0, 0, 0x66, 2, {0x0F, 0x50, 0}, 0, 2, {OPT_Reg|OPS_64|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, 0} }
};

static const x86_insn_info movntpddq_insn[] = {
    { CPU_SSE2, MOD_Op1Add, 0, 0, 0x66, 2, {0x0F, 0x00, 0}, 0, 2, {OPT_Mem|OPS_128|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, 0} }
};

static const x86_insn_info vmxmemrd_insn[] = {
    { CPU_Not64|CPU_P4, MOD_GasSufL, 32, 0, 0, 2, {0x0F, 0x78, 0}, 0, 2, {OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_32|OPA_Spare, 0} },
    { CPU_64|CPU_P4, MOD_GasSufQ, 64, 64, 0, 2, {0x0F, 0x78, 0}, 0, 2, {OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_64|OPA_Spare, 0} }
};

static const x86_insn_info vmxmemwr_insn[] = {
    { CPU_Not64|CPU_P4, MOD_GasSufL, 32, 0, 0, 2, {0x0F, 0x79, 0}, 0, 2, {OPT_Reg|OPS_32|OPA_Spare, OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, 0} },
    { CPU_64|CPU_P4, MOD_GasSufQ, 64, 64, 0, 2, {0x0F, 0x79, 0}, 0, 2, {OPT_Reg|OPS_64|OPA_Spare, OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, 0} }
};

static const x86_insn_info vmxtwobytemem_insn[] = {
    { CPU_P4, MOD_SpAdd, 0, 0, 0, 2, {0x0F, 0xC7, 0}, 0, 1, {OPT_Mem|OPS_64|OPS_Relaxed|OPA_EA, 0, 0} }
};

static const x86_insn_info vmxthreebytemem_insn[] = {
    { CPU_P4, MOD_PreAdd, 0, 0, 0x00, 2, {0x0F, 0xC7, 0}, 6, 1, {OPT_Mem|OPS_64|OPS_Relaxed|OPA_EA, 0, 0} }
};

static const x86_insn_info cvt_xmm_xmm32_insn[] = {
    { CPU_SSE2, MOD_Op1Add|MOD_PreAdd, 0, 0, 0x00, 2, {0x0F, 0x00, 0}, 0, 2, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, 0} },
    { CPU_SSE2, MOD_Op1Add|MOD_PreAdd, 0, 0, 0x00, 2, {0x0F, 0x00, 0}, 0, 2, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_Mem|OPS_32|OPS_Relaxed|OPA_EA, 0} }
};

static const x86_insn_info maskmovdqu_insn[] = {
    { CPU_SSE2, 0, 0, 0, 0x66, 2, {0x0F, 0xF7, 0}, 0, 2, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, 0} }
};

static const x86_insn_info movdqau_insn[] = {
    { CPU_SSE2, MOD_PreAdd, 0, 0, 0x00, 2, {0x0F, 0x6F, 0}, 0, 2, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDRM|OPS_128|OPS_Relaxed|OPA_EA, 0} },
    { CPU_SSE2, MOD_PreAdd, 0, 0, 0x00, 2, {0x0F, 0x7F, 0}, 0, 2, {OPT_SIMDRM|OPS_128|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, 0} }
};

static const x86_insn_info movdq2q_insn[] = {
    { CPU_SSE2, 0, 0, 0, 0xF2, 2, {0x0F, 0xD6, 0}, 0, 2, {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, 0} }
};

static const x86_insn_info movq2dq_insn[] = {
    { CPU_SSE2, 0, 0, 0, 0xF3, 2, {0x0F, 0xD6, 0}, 0, 2, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDReg|OPS_64|OPA_EA, 0} }
};

static const x86_insn_info pslrldq_insn[] = {
    { CPU_SSE2, MOD_SpAdd, 0, 0, 0x66, 2, {0x0F, 0x73, 0}, 0, 2, {OPT_SIMDReg|OPS_128|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm, 0} }
};

static const x86_insn_info lddqu_insn[] = {
    { CPU_SSE3, 0, 0, 0, 0xF2, 2, {0x0F, 0xF0, 0}, 0, 2, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_Mem|OPS_Any|OPA_EA, 0} }
};

static const x86_insn_info ssse3_insn[] = {
    { CPU_SSSE3, MOD_Op2Add, 0, 0, 0, 3, {0x0F, 0x38, 0x00}, 0, 2, {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_SIMDRM|OPS_64|OPS_Relaxed|OPA_EA, 0} },
    { CPU_SSSE3, MOD_Op2Add, 0, 0, 0x66, 3, {0x0F, 0x38, 0x00}, 0, 2, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDRM|OPS_128|OPS_Relaxed|OPA_EA, 0} }
};

static const x86_insn_info ssse3imm_insn[] = {
    { CPU_SSSE3, MOD_Op2Add, 0, 0, 0, 3, {0x0F, 0x3A, 0x00}, 0, 3, {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_SIMDRM|OPS_64|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} },
    { CPU_SSSE3, MOD_Op2Add, 0, 0, 0x66, 3, {0x0F, 0x3A, 0x00}, 0, 3, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDRM|OPS_128|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} }
};

static const x86_insn_info sse4_insn[] = {
    { CPU_SSE41, MOD_Op2Add, 0, 0, 0x66, 3, {0x0F, 0x38, 0x00}, 0, 2, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDRM|OPS_128|OPS_Relaxed|OPA_EA, 0} }
};

static const x86_insn_info sse4imm_insn[] = {
    { CPU_SSE41, MOD_Op2Add, 0, 0, 0x66, 3, {0x0F, 0x3A, 0x00}, 0, 3, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDRM|OPS_128|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} }
};

static const x86_insn_info sse4xmm0_insn[] = {
    { CPU_SSE41, MOD_Op2Add, 0, 0, 0x66, 3, {0x0F, 0x38, 0x00}, 0, 2, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDRM|OPS_128|OPS_Relaxed|OPA_EA, 0} },
    { CPU_SSE41, MOD_Op2Add, 0, 0, 0x66, 3, {0x0F, 0x38, 0x00}, 0, 3, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDRM|OPS_128|OPS_Relaxed|OPA_EA, OPT_XMM0|OPS_128|OPA_None} }
};

static const x86_insn_info crc32_insn[] = {
    { CPU_386|CPU_SSE42, MOD_GasSufB, 0, 0, 0xF2, 3, {0x0F, 0x38, 0xF0}, 0, 2, {OPT_Reg|OPS_32|OPA_Spare, OPT_RM|OPS_8|OPA_EA, 0} },
    { CPU_386|CPU_SSE42, MOD_GasSufW, 16, 0, 0xF2, 3, {0x0F, 0x38, 0xF1}, 0, 2, {OPT_Reg|OPS_32|OPA_Spare, OPT_RM|OPS_16|OPA_EA, 0} },
    { CPU_386|CPU_SSE42, MOD_GasSufL, 32, 0, 0xF2, 3, {0x0F, 0x38, 0xF1}, 0, 2, {OPT_Reg|OPS_32|OPA_Spare, OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, 0} },
    { CPU_64|CPU_SSE42, MOD_GasSufB, 64, 0, 0xF2, 3, {0x0F, 0x38, 0xF0}, 0, 2, {OPT_Reg|OPS_64|OPA_Spare, OPT_RM|OPS_8|OPA_EA, 0} },
    { CPU_64|CPU_SSE42, MOD_GasSufQ, 64, 0, 0xF2, 3, {0x0F, 0x38, 0xF1}, 0, 2, {OPT_Reg|OPS_64|OPA_Spare, OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, 0} }
};

static const x86_insn_info extractps_insn[] = {
    { CPU_386|CPU_SSE41, 0, 32, 0, 0x66, 3, {0x0F, 0x3A, 0x17}, 0, 3, {OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} },
    { CPU_64|CPU_SSE41, 0, 64, 0, 0x66, 3, {0x0F, 0x3A, 0x17}, 0, 3, {OPT_Reg|OPS_64|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} }
};

static const x86_insn_info insertps_insn[] = {
    { CPU_SSE41, 0, 0, 0, 0x66, 3, {0x0F, 0x3A, 0x21}, 0, 3, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_Mem|OPS_32|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} },
    { CPU_SSE41, 0, 0, 0, 0x66, 3, {0x0F, 0x3A, 0x21}, 0, 3, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} }
};

static const x86_insn_info movntdqa_insn[] = {
    { CPU_SSE41, 0, 0, 0, 0x66, 3, {0x0F, 0x38, 0x2A}, 0, 2, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_Mem|OPS_128|OPS_Relaxed|OPA_EA, 0} }
};

static const x86_insn_info sse4pcmpstr_insn[] = {
    { CPU_SSE42, MOD_Op2Add, 0, 0, 0x66, 3, {0x0F, 0x3A, 0x00}, 0, 3, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDRM|OPS_128|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} }
};

static const x86_insn_info pextrb_insn[] = {
    { CPU_SSE41, 0, 0, 0, 0x66, 3, {0x0F, 0x3A, 0x14}, 0, 3, {OPT_Mem|OPS_8|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} },
    { CPU_386|CPU_SSE41, 0, 32, 0, 0x66, 3, {0x0F, 0x3A, 0x14}, 0, 3, {OPT_Reg|OPS_32|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} },
    { CPU_64|CPU_SSE41, 0, 64, 0, 0x66, 3, {0x0F, 0x3A, 0x14}, 0, 3, {OPT_Reg|OPS_64|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} }
};

static const x86_insn_info pextrd_insn[] = {
    { CPU_386|CPU_SSE41, 0, 32, 0, 0x66, 3, {0x0F, 0x3A, 0x16}, 0, 3, {OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} }
};

static const x86_insn_info pextrq_insn[] = {
    { CPU_64|CPU_SSE41, 0, 64, 0, 0x66, 3, {0x0F, 0x3A, 0x16}, 0, 3, {OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} }
};

static const x86_insn_info pinsrb_insn[] = {
    { CPU_SSE41, 0, 0, 0, 0x66, 3, {0x0F, 0x3A, 0x20}, 0, 3, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_Mem|OPS_8|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} },
    { CPU_386|CPU_SSE41, 0, 32, 0, 0x66, 3, {0x0F, 0x3A, 0x20}, 0, 3, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_Reg|OPS_32|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} }
};

static const x86_insn_info pinsrd_insn[] = {
    { CPU_386|CPU_SSE41, 0, 32, 0, 0x66, 3, {0x0F, 0x3A, 0x22}, 0, 3, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} }
};

static const x86_insn_info pinsrq_insn[] = {
    { CPU_64|CPU_SSE41, 0, 64, 0, 0x66, 3, {0x0F, 0x3A, 0x22}, 0, 3, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} }
};

static const x86_insn_info sse4m64_insn[] = {
    { CPU_SSE41, MOD_Op2Add, 0, 0, 0x66, 3, {0x0F, 0x38, 0x00}, 0, 2, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_Mem|OPS_64|OPS_Relaxed|OPA_EA, 0} },
    { CPU_SSE41, MOD_Op2Add, 0, 0, 0x66, 3, {0x0F, 0x38, 0x00}, 0, 2, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, 0} }
};

static const x86_insn_info sse4m32_insn[] = {
    { CPU_SSE41, MOD_Op2Add, 0, 0, 0x66, 3, {0x0F, 0x38, 0x00}, 0, 2, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_Mem|OPS_32|OPS_Relaxed|OPA_EA, 0} },
    { CPU_SSE41, MOD_Op2Add, 0, 0, 0x66, 3, {0x0F, 0x38, 0x00}, 0, 2, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, 0} }
};

static const x86_insn_info sse4m16_insn[] = {
    { CPU_SSE41, MOD_Op2Add, 0, 0, 0x66, 3, {0x0F, 0x38, 0x00}, 0, 2, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_Mem|OPS_16|OPS_Relaxed|OPA_EA, 0} },
    { CPU_SSE41, MOD_Op2Add, 0, 0, 0x66, 3, {0x0F, 0x38, 0x00}, 0, 2, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, 0} }
};

static const x86_insn_info cnt_insn[] = {
    { CPU_Any, MOD_Op1Add|MOD_GasSufW, 16, 0, 0xF3, 2, {0x0F, 0x00, 0}, 0, 2, {OPT_Reg|OPS_16|OPA_Spare, OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, 0} },
    { CPU_386, MOD_Op1Add|MOD_GasSufL, 32, 0, 0xF3, 2, {0x0F, 0x00, 0}, 0, 2, {OPT_Reg|OPS_32|OPA_Spare, OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, 0} },
    { CPU_64, MOD_Op1Add|MOD_GasSufQ, 64, 0, 0xF3, 2, {0x0F, 0x00, 0}, 0, 2, {OPT_Reg|OPS_64|OPA_Spare, OPT_RM|OPS_64|OPS_Relaxed|OPA_EA, 0} }
};

static const x86_insn_info extrq_insn[] = {
    { CPU_SSE41, 0, 0, 0, 0x66, 2, {0x0F, 0x78, 0}, 0, 3, {OPT_SIMDReg|OPS_128|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_Imm} },
    { CPU_SSE41, 0, 0, 0, 0x66, 2, {0x0F, 0x79, 0}, 0, 2, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, 0} }
};

static const x86_insn_info insertq_insn[] = {
    { CPU_SSE41, 0, 0, 0, 0xF2, 2, {0x0F, 0x78, 0}, 0, 4, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, OPT_Imm|OPS_8|OPS_Relaxed|OPA_EA} },
    { CPU_SSE41, 0, 0, 0, 0xF2, 2, {0x0F, 0x79, 0}, 0, 2, {OPT_SIMDReg|OPS_128|OPA_Spare, OPT_SIMDReg|OPS_128|OPA_EA, 0} }
};

static const x86_insn_info movntsd_insn[] = {
    { CPU_SSE41, 0, 0, 0, 0xF2, 2, {0x0F, 0x2B, 0}, 0, 2, {OPT_Mem|OPS_64|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, 0} }
};

static const x86_insn_info movntss_insn[] = {
    { CPU_SSE41, 0, 0, 0, 0xF3, 2, {0x0F, 0x2B, 0}, 0, 2, {OPT_Mem|OPS_32|OPS_Relaxed|OPA_EA, OPT_SIMDReg|OPS_128|OPA_Spare, 0} }
};

static const x86_insn_info now3d_insn[] = {
    { CPU_3DNow, MOD_Imm8, 0, 0, 0, 2, {0x0F, 0x0F, 0}, 0, 2, {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_SIMDRM|OPS_64|OPS_Relaxed|OPA_EA, 0} }
};

static const x86_insn_info cmpxchg16b_insn[] = {
    { CPU_64, 0, 64, 0, 0, 2, {0x0F, 0xC7, 0}, 1, 1, {OPT_Mem|OPS_128|OPS_Relaxed|OPA_EA, 0, 0} }
};

static const x86_insn_info invlpga_insn[] = {
    { CPU_SVM, 0, 0, 0, 0, 3, {0x0F, 0x01, 0xDF}, 0, 0, {0, 0, 0} },
    { CPU_386|CPU_SVM, 0, 0, 0, 0, 3, {0x0F, 0x01, 0xDF}, 0, 2, {OPT_MemrAX|OPS_Any|OPA_AdSizeEA, OPT_Creg|OPS_32|OPA_None, 0} }
};

static const x86_insn_info skinit_insn[] = {
    { CPU_SVM, 0, 0, 0, 0, 3, {0x0F, 0x01, 0xDE}, 0, 0, {0, 0, 0} },
    { CPU_SVM, 0, 0, 0, 0, 3, {0x0F, 0x01, 0xDE}, 0, 1, {OPT_MemEAX|OPS_Any|OPA_None, 0, 0} }
};

static const x86_insn_info svm_rax_insn[] = {
    { CPU_SVM, MOD_Op2Add, 0, 0, 0, 3, {0x0F, 0x01, 0x00}, 0, 0, {0, 0, 0} },
    { CPU_SVM, MOD_Op2Add, 0, 0, 0, 3, {0x0F, 0x01, 0x00}, 0, 1, {OPT_MemrAX|OPS_Any|OPA_AdSizeEA, 0, 0} }
};

static const x86_insn_info padlock_insn[] = {
    { CPU_PadLock, MOD_Op1Add|MOD_PreAdd|MOD_Imm8, 0, 0, 0x00, 2, {0x0F, 0x00, 0}, 0, 0, {0, 0, 0} }
};

static const x86_insn_info cyrixmmx_insn[] = {
    { CPU_Cyrix|CPU_MMX, MOD_Op1Add, 0, 0, 0, 2, {0x0F, 0x00, 0}, 0, 2, {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_SIMDRM|OPS_64|OPS_Relaxed|OPA_EA, 0} }
};

static const x86_insn_info pmachriw_insn[] = {
    { CPU_Cyrix|CPU_MMX, 0, 0, 0, 0, 2, {0x0F, 0x5E, 0}, 0, 2, {OPT_SIMDReg|OPS_64|OPA_Spare, OPT_Mem|OPS_64|OPS_Relaxed|OPA_EA, 0} }
};

static const x86_insn_info rdwrshr_insn[] = {
    { CPU_686|CPU_Cyrix|CPU_SMM, MOD_Op1Add, 0, 0, 0, 2, {0x0F, 0x36, 0}, 0, 1, {OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, 0, 0} }
};

static const x86_insn_info rsdc_insn[] = {
    { CPU_486|CPU_Cyrix|CPU_SMM, 0, 0, 0, 0, 2, {0x0F, 0x79, 0}, 0, 2, {OPT_SegReg|OPS_16|OPS_Relaxed|OPA_Spare, OPT_Mem|OPS_80|OPS_Relaxed|OPA_EA, 0} }
};

static const x86_insn_info cyrixsmm_insn[] = {
    { CPU_486|CPU_Cyrix|CPU_SMM, MOD_Op1Add, 0, 0, 0, 2, {0x0F, 0x00, 0}, 0, 1, {OPT_Mem|OPS_80|OPS_Relaxed|OPA_EA, 0, 0} }
};

static const x86_insn_info svdc_insn[] = {
    { CPU_486|CPU_Cyrix|CPU_SMM, 0, 0, 0, 0, 2, {0x0F, 0x78, 0}, 0, 2, {OPT_Mem|OPS_80|OPS_Relaxed|OPA_EA, OPT_SegReg|OPS_16|OPS_Relaxed|OPA_Spare, 0} }
};

static const x86_insn_info ibts_insn[] = {
    { CPU_386|CPU_Obs|CPU_Undoc, 0, 16, 0, 0, 2, {0x0F, 0xA7, 0}, 0, 2, {OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_16|OPA_Spare, 0} },
    { CPU_386|CPU_Obs|CPU_Undoc, 0, 32, 0, 0, 2, {0x0F, 0xA7, 0}, 0, 2, {OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_32|OPA_Spare, 0} }
};

static const x86_insn_info umov_insn[] = {
    { CPU_386|CPU_Undoc, 0, 0, 0, 0, 2, {0x0F, 0x10, 0}, 0, 2, {OPT_RM|OPS_8|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_8|OPA_Spare, 0} },
    { CPU_386|CPU_Undoc, 0, 16, 0, 0, 2, {0x0F, 0x11, 0}, 0, 2, {OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_16|OPA_Spare, 0} },
    { CPU_386|CPU_Undoc, 0, 32, 0, 0, 2, {0x0F, 0x11, 0}, 0, 2, {OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, OPT_Reg|OPS_32|OPA_Spare, 0} },
    { CPU_386|CPU_Undoc, 0, 0, 0, 0, 2, {0x0F, 0x12, 0}, 0, 2, {OPT_Reg|OPS_8|OPA_Spare, OPT_RM|OPS_8|OPS_Relaxed|OPA_EA, 0} },
    { CPU_386|CPU_Undoc, 0, 16, 0, 0, 2, {0x0F, 0x13, 0}, 0, 2, {OPT_Reg|OPS_16|OPA_Spare, OPT_RM|OPS_16|OPS_Relaxed|OPA_EA, 0} },
    { CPU_386|CPU_Undoc, 0, 32, 0, 0, 2, {0x0F, 0x13, 0}, 0, 2, {OPT_Reg|OPS_32|OPA_Spare, OPT_RM|OPS_32|OPS_Relaxed|OPA_EA, 0} }
};

static const x86_insn_info xbts_insn[] = {
    { CPU_386|CPU_Obs|CPU_Undoc, 0, 16, 0, 0, 2, {0x0F, 0xA6, 0}, 0, 2, {OPT_Reg|OPS_16|OPA_Spare, OPT_Mem|OPS_16|OPS_Relaxed|OPA_EA, 0} },
    { CPU_386|CPU_Obs|CPU_Undoc, 0, 32, 0, 0, 2, {0x0F, 0xA6, 0}, 0, 2, {OPT_Reg|OPS_32|OPA_Spare, OPT_Mem|OPS_32|OPS_Relaxed|OPA_EA, 0} }
};

