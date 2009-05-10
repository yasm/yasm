; BITS=16 to minimize output length
[bits 16]
vcvtph2ps xmm1, xmm4, 5			; 8F E8 78 A0 314 05
vcvtph2ps xmm2, [0], byte 5		; 8F E8 78 A0 026 00 00 05
vcvtph2ps xmm3, qword [0], 5		; 8F E8 78 A0 036 00 00 05
vcvtph2ps ymm1, xmm4, 5			; 8F E8 7C A0 314 05
vcvtph2ps ymm2, [0], byte 5		; 8F E8 7C A0 026 00 00 05
vcvtph2ps ymm3, dqword [0], 5		; 8F E8 7C A0 036 00 00 05

vcvtps2ph xmm1, xmm4, 5			; 8F E8 78 A1 341 05
vcvtps2ph [0], xmm2, byte 5		; 8F E8 78 A1 026 00 00 05
vcvtps2ph qword [0], xmm3, 5		; 8F E8 78 A1 036 00 00 05
vcvtps2ph xmm1, ymm4, 5			; 8F E8 7C A1 341 05
vcvtps2ph [0], ymm2, byte 5		; 8F E8 7C A1 026 00 00 05
vcvtps2ph dqword [0], ymm3, 5		; 8F E8 7C A1 036 00 00 05

