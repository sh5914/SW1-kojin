.global inbyte
	

.text
.even


.equ SYSCALL_NUM_GETSTRING, 1
	
inbyte:
	link   %A6,#-4
	movem.l	%D1-%D3/%A0,-(%sp)
	move.l  8(%a6),%D1
	
loop:	
	move.l #SYSCALL_NUM_GETSTRING, %D0
	lea    -1(%A6),%A0
	move.l %A0, %D2
	move.l #1, %D3 /* size = 256 */
	trap #0
	

	cmp #0,%D0
	beq loop

	move.l #0,%D0
	move.b -1(%A6),%D0
	

	movem.l	(%sp)+,%D1-%D3/%A0
	unlk   %A6
	rts





