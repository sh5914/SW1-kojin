.global inbyte
.global inkey
	

.text
.even


.equ SYSCALL_NUM_GETSTRING, 1
	
inbyte:
	movem.l	%D0-%D3/%A1,-(%sp)
loop:	
	move.l #SYSCALL_NUM_GETSTRING, %D0
	move.l #0, %D1 /* ch = 0 */
	move.b -(%sp), %D2 /* p = #BUF */
	move.l #1, %D3 /* size = 256 */
	trap #0

	move.b (%sp)+, %D0

	cmp #0,%D0
	beq loop


	movem.l	(%sp)+,%D0-%D3/%A1
	rts

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






