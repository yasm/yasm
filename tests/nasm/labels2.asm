;[EXTERN banana]
[GLOBAL banana]
;[COMMON banana 4]
apple:
mov ax, 5
 .orange:
mov dx, banana
 .orange.white:
mov al, 1
banana.echo:
mov dx, 5+2
 .insane:
banana:

blah..host..me:

test.:
 .me:

;banana.echo:
jmp apple
jmp apple.orange
jmp apple.orange.white
jmp banana
jmp banana.echo
;jmp banana.insane
jmp banana.echo.insane
jmp blah..host..me
jmp test..me
;banana equ 5
;banana equ 7
