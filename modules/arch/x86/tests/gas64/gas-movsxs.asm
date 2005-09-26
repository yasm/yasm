movsbl %al, %eax
movsbw %al, %ax
movswl %ax, %eax
movsbq %al, %rax
movswq %ax, %rax
movslq %eax, %rax
# Intel formats - untested for now
#movsxw %ax, %eax
#movsxb %al, %ax
#movsxb %al, %rax
#movsxw %ax, %rax
#movsxl %eax, %rax

movzbl %al, %eax
movzbw %al, %ax
movzwl %ax, %eax
movzbq %al, %rax
movzwq %ax, %rax
