.global outbyte

.text
.even


	
.equ SYSCALL_NUM_PUTSTRING, 2
	
outbyte:
	movem.l	%D0-%D3,-(%sp)
loop:	
	move.l #SYSCALL_NUM_PUTSTRING, %D0
	move.l #0, %D1 /* ch = 0 */
	move.l %sp,%D2 /* p = #BUF */
	add.l  #23,%D2
	move.l #1, %D3
	trap #0

	cmp #0,%D0
	beq loop


	movem.l	(%sp)+,%D0-%D3
	rts



