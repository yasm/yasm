[bits 64]

pclmulqdq xmm1, xmm2, 5
pclmulqdq xmm1, [rax], byte 5
pclmulqdq xmm1, dqword [rax], 5

pclmullqlqdq xmm1, xmm2
pclmullqlqdq xmm1, [rax]
pclmullqlqdq xmm1, dqword [rax]

pclmulhqlqdq xmm1, xmm2
pclmulhqlqdq xmm1, [rax]
pclmulhqlqdq xmm1, dqword [rax]

pclmullqhqdq xmm1, xmm2
pclmullqhqdq xmm1, [rax]
pclmullqhqdq xmm1, dqword [rax]

pclmulhqhqdq xmm1, xmm2
pclmulhqhqdq xmm1, [rax]
pclmulhqhqdq xmm1, dqword [rax]

