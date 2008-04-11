[bits 64]
vpermiltd2pd xmm1, xmm2, xmm3, xmm4	; 0
;vpermiltd2pd xmm1, xmm2, xmm3, xmm4	; 1
vpermilmo2pd xmm1, xmm2, xmm3, xmm4	; 2
vpermilmz2pd xmm1, xmm2, xmm3, xmm4	; 3

vpermiltd2ps xmm1, xmm2, xmm3, xmm4	; 0
;vpermiltd2ps xmm1, xmm2, xmm3, xmm4	; 1
vpermilmo2ps xmm1, xmm2, xmm3, xmm4	; 2
vpermilmz2ps xmm1, xmm2, xmm3, xmm4	; 3
