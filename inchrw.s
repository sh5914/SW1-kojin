.global inbyte
.global inkey
	

.text
.even


.equ SYSCALL_NUM_GETSTRING, 1
	
inbyte:
	movem.l	%D1-%D3/%A0,-(%sp)
loop:	
	move.l #SYSCALL_NUM_GETSTRING, %D0
	move.l #0, %D1 /* ch = 0 */
	move.l #BUF, %D2 /* p = #BUF */
	move.l #1, %D3 /* size = 256 */
	trap #0
	

	cmp #0,%D0
	beq loop

	lea BUF, %A0
	move.b (%A0),%D0
	

	movem.l	(%sp)+,%D1-%D3/%A0
	rts

.section .bss
BUF:
	.ds.b 1 /* BUF[256] */
.even


/*inkey:*/
	/*move.l #SYSCALL_NUM_GETSTRING, %D0*/
	/*move.l #0, %D1 /* ch = 0 */
	/*move.l #BUF, %D2 /* p = #BUF */
	/*move.l #1, %D3 /* size = 256 */
	/*trap #0*/

	/*tst.l %D0*/
	/*beq    no_input*/

	/*moveq  #0,%D0*/
	/*move.l #BUF,%D0*/
	/*bra    inkey_end*/


/*no_input:*/
	/*move.l  #-1,%D0*/

/*inkey_end:*/
	/*rts*/


