.global outbyte

.text
.even


	
.equ SYSCALL_NUM_PUTSTRING, 2
	
outbyte:
	link    %A6,#0
	movem.l	%D0-%D3,-(%sp)
	move.l  12(%A6), %D1

	lea     8(%A6), %A0
	addq.l  #3, %A0
	move.l  %A0, %D2
	
loop:	
	move.l #SYSCALL_NUM_PUTSTRING, %D0
	move.l #1, %D3
	trap #0

	cmp #0,%D0
	beq loop

	movem.l	(%sp)+,%D0-%D3
	unlk    %A6
	rts



