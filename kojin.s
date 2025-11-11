*****************
** 各種レジスタ定義
*****************


** レジスタ群の先頭 ***************
	.equ REGBASE, 0xFFF000	/* DMAP を使用． */
	.equ IOBASE, 0x00d00000


** 割り込み関係のレジスタ***************
	.equ IVR, REGBASE+0x300	/* 割り込みベクタレジスタ */
	.equ IMR, REGBASE+0x304	/* 割り込みマスクレジスタ */
	.equ ISR, REGBASE+0x30c	/* 割り込みステータスレジスタ */
	.equ IPR, REGBASE+0x310	/* 割り込みペンディングレジスタ */

** タイマ関係のレジスタ ***************
	.equ TCTL1, REGBASE+0x600	/* タイマ１コントロールレジスタ */
	.equ TPRER1, REGBASE+0x602	/* タイマ１プリスケーラレジスタ */
	.equ TCMP1, REGBASE+0x604	/* タイマ１コンペアレジスタ */
	.equ TCN1, REGBASE+0x608	/* タイマ１カウンタレジスタ */
	.equ TSTAT1, REGBASE+0x60a	/* タイマ１ステータスレジスタ */


/* キューのオフセットと必要領域計算 */
.section .bss

	.equ	B_SIZE,	256	
	.equ	TOP,	0
	.equ	BOTTOM,	B_SIZE
	.equ	IN,	BOTTOM + 4
	.equ	OUT,	IN + 4
	.equ	S,	OUT + 4
	.equ	SIZE,	S + 2

/* キュー用のメモリ領域確保 */
QUEUES:		.ds.b	SIZE * 4


** UART1（送受信）関係のレジスタ 
	.equ USTCNT1, REGBASE+0x900	/* UART1 ステータス/コントロールレジスタ*/
	.equ UBAUD1, REGBASE+0x902	/* UART1 ボーコントロールレジスタ */
	.equ URX1, REGBASE+0x904	/* UART1 受信レジスタ */
	.equ UTX1, REGBASE+0x906	/* UART1 送信レジスタ */

***************
** LED
**ボード搭載の LED 用レジスタ,使用法については付録 A.4.3.1
***************
	.equ LED7, IOBASE+0x000002f
	.equ LED6, IOBASE+0x000002d 
	.equ LED5, IOBASE+0x000002b
	.equ LED4, IOBASE+0x0000029
	.equ LED3, IOBASE+0x000003f
	.equ LED2, IOBASE+0x000003d
	.equ LED1, IOBASE+0x000003b
	.equ LED0, IOBASE+0x0000039

********************
** スタック領域の確保 
********************
	.section .bss
	.even
SYS_STK:
	.ds.b 0x4000	/* システムスタック領域 */
	.even 
	SYS_STK_TOP: /*| システムスタック領域の最後尾 */
	
TOTAL_SECONDS:
    .ds.l 1     /* 総合経過時間 (秒) (注: TOTAL_SECOND ではなく SECONDS)*/
TIME_BUF:
    .ds.b 8     /* "00:00" 用バッファ*/

CURRENT_PROMPT_PTR:
    .ds.l 1     /* 現在のお題のアドレス (注: CURRENT_DATA_PTR ではなく)*/
CURRENT_CHAR_INDEX:
    .ds.l 1     /* 次に打つべき文字のインデックス*/

WORD_TIMER_FLAG:
    .ds.b 1     /* お題ごとのタイマーフラグ (0: 測定中, 1: 時間切れ)*/



********************
** PUT/GETSTRING用変数の確保 
********************

sz:		.ds.l 1
i:		.ds.l 1

* SET_TIMER のコールバックルーチンポインタ
task_p:
    .ds.l   1       /* 4バイトの領域を確保 */




***************************************************************
** 初期化 
** 内部デバイスレジスタには特定の値が設定されている． 
** その理由を知るには，付録 B にある各レジスタの仕様を参照すること． 
***************************************************************

	.section .text
	.even
boot: /* スーパーバイザ & 各種設定を行っている最中の割込禁止 */
	move.w #0x2700,%SR
	lea.l SYS_STK_TOP, %SP /* | Set SSP */
****************
** 割り込みコントローラの初期化
****************
	move.b #0x40, IVR	/* | ユーザ割り込みベクタ番号を| 0x40+level に設定． */
	move.l #0x00ffffff,IMR	/* | 全割り込みマスク */

****************
** 送受信 (UART1) 関係の初期化 (割り込みレベルは 4 に固定されている) 
****************
	move.w #0x0000, USTCNT1	/* | リセット */
	move.w #0xe100, USTCNT1	/* | 送受信可能, パリティなし, 1 stop, 8 bit, */
				/* | 送受割り込み禁止 */
	move.w #0x0038, UBAUD1	/* | baud rate = 230400 bps */


/* キューの初期化 */
QueueInitialize:
	movem.l	%d0-%d1/%a0-%a2,-(%sp)
	lea.l	QUEUES,%a0
	moveq	#4,%d0
	
InitLoop:
	movea.l	%a0,%a1
	lea.l	TOP(%a1),%a2
	move.l	%a2,IN(%a1)
	move.l	%a2,OUT(%a1)
	move.w	#0,S(%a1)
	adda.l	#SIZE,%a0
	subq.l	#1,%d0
	bne	InitLoop
	movem.l	(%sp)+,%d0-%d1/%a0-%a2
	


****************
** タイマ関係の初期化 (割り込みレベルは 6 に固定されている) 
*****************
	move.w #0x0004, TCTL1	/* restart, 割り込み不可 */
                        **システムクロックの 1/16 を単位として計時，
                        **タイマ使用停止

						
	* TRAP #0 ハンドラをベクタテーブルに登録
	move.l	#TRAP0_HANDLER, 0x0080
	* TRAP 0 (ベクタ番号 32) のアドレスにハンドラを設定 
	/* UART1 割り込み(レベル4, ベクタ68) ハンドラを設定 */
    /* ( IVR 0x40 + Level 4 = 0x44 (68), Address 68 * 4 = 0x110 ) */
    move.l  #send_or_receive, 0x110
    
    /* タイマ1 割り込み(レベル6, ベクタ70) ハンドラを設定 */
    /* ( IVR 0x40 + Level 6 = 0x46 (70), Address 70 * 4 = 0x118 ) */
    move.l  #timer1_interrupt, 0x118
    
    /* UART(レベル4)の割り込みマスクをIMRで解除 */
    /* IMRのビット2,1を0にする */
    andi.l  #0xfffffff9, IMR
	
	bra INIT
******************
** 初期化（追加部分）
******************

.section .text
.even
INIT:
	* 走行レベル0で開始（スーパバイザモード）
	move.w	#0x2000, %SR

    * UART 割り込み許可（送受信割り込みON）
	move.w  #0xE10C, USTCNT1



***************
** システムコール番号
***************
.equ SYSCALL_NUM_GETSTRING, 1
.equ SYSCALL_NUM_PUTSTRING, 2
.equ SYSCALL_NUM_RESET_TIMER, 3
.equ SYSCALL_NUM_SET_TIMER, 4
****************************************************************
*** プログラム領域
****************************************************************
.section .text
.even
MAIN:
** 走行モードとレベルの設定 (「ユーザモード」への移行処理)
	move.w #0x0000, %SR /* USER MODE, LEVEL 0 */
	lea.l USR_STK_TOP,%SP /* user stack の設定 */


MAIN_START:
	lea.l   TOTAL_SECONDS, %a0
    move.l  #0, (%a0)           


    bsr     PUTSTRING_CALL_LIB 
    .asciz "Typing Practice Start!\r\n"
.even
    bsr     PUTSTRING_CALL_LIB 
    .asciz "Total Time: 00:00\r\n"
.even
    
    move.l  #SYSCALL_NUM_SET_TIMER, %d0
    move.w  #10000, %d1        
    move.l  #TIMER_1SEC_CALLBACK, %d2
    trap    #0

    bsr     DISPLAY_NEXT_PROMPT

MAIN_LOOP:
    lea.l   WORD_TIMER_FLAG, %a0
    cmp.b   #1, (%a0)
    beq     TIME_UP_FAILURE     /*時間切れなら失敗処理へ*/

    /*6. 1文字入力を待つ*/
    move.l  #SYSCALL_NUM_GETSTRING, %d0
    move.l  #0, %d1
    move.l  #BUF, %d2           /*1文字用のバッファ [cite: 2648-2650]*/
    move.l  #1, %d3             /*size = 1*/
    trap    #0
    cmp.l   #1, %d0             /*1文字入力されたか？*/
    bne     MAIN_LOOP           /*されてなければ無視 (ループ)*/

    /* 7. 入力された文字 (UserInput) を %d1 に取得*/
    move.b  BUF, %d1

    /* 8. お題から「期待される文字」 (ExpectedChar) を %d0 に取得*/
    lea.l   CURRENT_PROMPT_PTR, %a0
    move.l  (%a0), %a0          /* %a0 = お題のアドレス (例: "hello")*/
    lea.l   CURRENT_CHAR_INDEX, %a1
    move.l  (%a1), %d2          /* %d2 = インデックス (例: 0)*/
    add.l   %d2, %a0            /* %a0 = お題のアドレス + インデックス*/
    move.b  (%a0), %d0          /* %d0 = 期待される文字 (例: "hello"[0] -> 'h')*/

    /* 9. 答え合わせ*/
    cmp.b   %d1, %d0
    bne     GAME_LOOP           /* ★不正解: 何もせず、次の入力を待つ*/

/* --- (正解時の処理) ---*/
    /* 10. 正解の文字を画面に表示*/
    bsr     ECHO_CHAR_BUF       /* (PUTSTRINGでBUFを1文字表示する)*/

    /* 11. インデックスを1つ進める*/
    lea.l   CURRENT_CHAR_INDEX, %a1
    addq.l  #1, (%a1)
    move.l  (%a1), %d2          /* %d2 = 新しいインデックス (例: 1)*/

    /* 12. 単語を最後まで入力したかチェック*/
    lea.l   CURRENT_PROMPT_PTR, %a0
    move.l  (%a0), %a0          /* %a0 = お題のアドレス*/
    add.l   %d2, %a0            /* %a0 = お題のアドレス + 新インデックス*/
    move.b  (%a0), %d0          /* %d0 = 次の期待文字*/
    
    cmp.b   #0, %d0             /* 次の文字がヌル文字(0x00)か？*/
    bne     GAME_LOOP           /* 違えば: 単語はまだ続く (ループ)*/

/* --- (単語完成時の処理) ---*/
WORD_COMPLETE:
    /* 13. お題ごとのタイマーを止める (必須)*/
    move.l  #SYSCALL_NUM_RESET_TIMER, %d0
    trap    #0
    bra     NEXT_PROMPT_SETUP

/* --- (時間切れ時の処理) ---*/
TIME_UP_FAILURE:
    /* 14. 失敗メッセージを表示*/
    bsr     PUTSTRING_CALL_LIB
    .asciz "\r\nTime's Up! Next word...\r\n"
.even

NEXT_PROMPT_SETUP:
    /* 15. 次のお題を準備*/
    bsr     DISPLAY_NEXT_PROMPT
    bra     GAME_LOOP


.section .text
.even
********************************
** 受信割り込みか送信割り込みかを判定
********************************
send_or_receive:
    movem.l	%d0-%d7/%a0-%a7,-(%sp) /* 安全のため全レジスタを退避 */

* --- 1. 受信処理を優先的にチェック ---
check_receive:
    move.w	URX1, %d3       /* URX1レジスタ(受信)を読み込み */
    move.b	%d3, %d2        /* %d2 にデータ部(bit 7-0)を先に保存 */
    andi.w	#0x2000, %d3    /* DATA READYフラグ(bit 13)をチェック */
    beq		check_send      /* フラグが0 (受信データなし) なら、送信チェックへ進む */

* --- 受信データがあった場合の処理 ---
receive_init:
    move.l	#0, %d1         /* ch=0 (UART1) */
    jsr		INTERGET        /* %d2のデータを使ってINTERGETを実行し、キューに入れる */
                            /* INTERGETは %d2 のデータを使用します */
    
* --- 2. 送信処理をチェック ---
check_send:
    move.w	UTX1, %d3       /* UTX1レジスタ(送信)を読み込み */
    andi.w 	#0x8000, %d3    /* FIFO EMPTYフラグ(bit 15)をチェック */ 
    beq		SoR_end         /* フラグが0 (送信不可) なら、ハンドラを終了 */

* --- 送信可能だった場合の処理 ---
send_init:
    move.l	#0, %d1         /* ch=0 (UART1) */
    jsr		INTERPUT        /* INTERPUTを実行し、キューからデータを取り出し送信 */

* --- 終了処理 ---
SoR_end:
    movem.l	(%sp)+, %d0-%d7/%a0-%a7 /* 退避したレジスタを復帰 */
    rte


***************************************************************
** TRAP #0 システムコールハンドラ 
** (Step 8: OSサービスの呼び出しに対応)
***************************************************************
TRAP0_HANDLER:
	
	* サービス番号（%D0）に応じて分岐する
	cmpi.l	#1, %d0					/* %D0 が 1 (GETSTRING) かを比較する。*/
	beq		SYSCALL_GETSTRING
	
	cmpi.l	#2, %d0					/* %D0 が 2 (PUTSTRING) かを比較する。*/
	beq		SYSCALL_PUTSTRING

	cmpi.l	#3, %d0					/* %D0 が 3 (RESET_TIMER) かを比較する。*/
	beq		SYSCALL_RESET_TIMER
	
	cmpi.l	#4, %d0					/* %D0 が 4 (SET_TIMER) かを比較する。*/
	beq		SYSCALL_SET_TIMER

	bra		TRAP0_EXIT				/* どのサービス番号にも一致しない場合、終了へ進む。*/

SYSCALL_GETSTRING:
	* GETSTRING(ch=0, p=%D2, size=%D3)
	move.l	#GETSTRING, %d0
	jsr		GETSTRING				/* GETSTRING を呼び出す。*/
	bra		TRAP0_EXIT
	
SYSCALL_PUTSTRING:
	* PUTSTRING(ch=0, p=%D2, size=%D3)
	move.l	#PUTSTRING, %d0
	jsr		PUTSTRING				/* PUTSTRING を呼び出す。*/
	bra		TRAP0_EXIT

SYSCALL_RESET_TIMER:
	* RESET_TIMER()
	move.l	#RESET_TIMER, %d0
	jsr		RESET_TIMER				/* RESET_TIMER を呼び出す。*/
	bra		TRAP0_EXIT
	
SYSCALL_SET_TIMER:
	* SET_TIMER(t=%D1, p=%D2)
	move.l	#SET_TIMER, %d0
	jsr		SET_TIMER				/* SET_TIMER を呼び出す。*/
	bra		TRAP0_EXIT

TRAP0_EXIT:
	rte								/* 例外から復帰する。*/


**-----------------------------------------------------------------------
** INQ：番号noのキューにデータを入れる
** 入力：	キュー番号 no -> %d0.l
**			書き込む8bitデータ data -> %d1.b108
** 戻り値：	失敗0/成功1 -> %d0.l
**----------------------------------------------------------------------
INQ:
	move.w	%SR,-(%sp)					/* (1) 現走行レベルの退避 */
	move.w	#0x2700,%SR					/* (2) 割り込み禁止(= 走行レベルを 7 に) */
	movem.l	%d2-%d3/%a1-%a3,-(%sp)		/* レジスタの退避 */
	lea.l	QUEUES,%a1					/* 指定された番号のキューのアドレスを計算 */
	mulu.w	#SIZE,%d0
	adda.l	%d0,%a1
	jsr	PUT_BUF							/* (3) ～ (6) */
	movem.l	(%sp)+,%d2-%d3/%a1-%a3		/* レジスタの回復 */
	move.w	(%sp)+,%SR					/* (7) 旧走行レベルの回復 */
	rts

PUT_BUF:
	move.l	#0,%d2
	move.w	S(%a1),%d3
	cmp.w	#B_SIZE,%d3					/* (3)  s == 256 ならば %d0 を 0(失敗)に設定し，(7) へ */
	beq	PUT_BUF_Finish
	movea.l	IN(%a1),%a2
	move.b	%d1,(%a2)					/* (4) m[in] = data */
	adda.l	#1,%a2						/* in++ ( (5) の else ) */
	lea.l	BOTTOM(%a1),%a3
	cmpa.l	%a3,%a2						/* (5) if (in == bottom) in=top */
	bcs	PUT_BUF_STEP1
	lea.l	TOP(%a1),%a2

PUT_BUF_STEP1:
	move.l	%a2,IN(%a1)
	add.w	#1,%d3						/* (6) s++,                  */
	move.w	%d3,S(%a1)
	move.l	#1,%d2						/*     %D0 を 1（成功）に設定 */

PUT_BUF_Finish:
	move.l	%d2,%d0
	rts

**-----------------------------------------------------------------------
** OUTQ：番号noのキューからデータを一つ取り出す
** 入力：	キュー番号 no -> %d0.l
** 戻り値：	失敗0/成功1 -> %d0.l
** 			取り出した8bitデータ data -> %d1.b
**----------------------------------------------------------------------
OUTQ:
	move.w	%SR,-(%sp)					/* (1) 現走行レベルの退避 */
	move.w	#0x2700,%SR					/* (2) 割り込み禁止(= 走行レベルを 7 に) */
	movem.l	%d2-%d3/%a1-%a3,-(%sp)		/* レジスタの退避 */
	lea.l	QUEUES,%a1					/* 指定された番号のキューのアドレスを計算 */
	mulu.w	#SIZE,%d0
	adda.l	%d0,%a1
	jsr	GET_BUF							/* (3) ～ (6) */
	movem.l	(%sp)+,%d2-%d3/%a1-%a3		/* レジスタの回復 */
	move.w	(%sp)+,%SR					/* (7) 旧走行レベルの回復 */
	rts

GET_BUF:
	move.l	#0,%d2
	move.w	S(%a1),%d3
	cmp.w	#0x00,%d3					/* (3)  s == 0 ならば %d0 を 0(失敗)に設定し，(7) へ */
	beq	GET_BUF_Finish
	movea.l	OUT(%a1),%a2
	move.b	(%a2),%d1					/* (4) data = m[out] */
	adda.l	#1,%a2						/* out++ ( (5) の else ) */
	lea.l	BOTTOM(%a1),%a3
	cmpa.l	%a3,%a2						/* (5) if (out == bottom) iout=top */
	bcs	GET_BUF_STEP1
	lea.l	TOP(%a1),%a2

GET_BUF_STEP1:
	move.l	%a2,OUT(%a1)
	sub.w	#1,%d3						/* (6) s--                   */
	move.w	%d3,S(%a1)
	move.l	#1,%d2						/*     %d0 を 1（成功）に設定 */

GET_BUF_Finish:	
	move.l	%d2,%d0
	rts


*****************************************
**入力
  **チャネル ch→%d1
**戻り値
  **なし 
*****************************************


INTERPUT:
	movem.l	 %a0-%a2/%d0,-(%sp)	/*レジスタ退避*/
	
	/*(1) 割り込み禁止（走行レベルを7に）*/
	ori.w  #0x0700,%SR
	
	/*(2) chが0でないならば，何もせずに復帰*/
	cmp.l #0,%d1
	bne  END_of_INTERPUT
	
	/*(3) OUTQ(1,data) を実行する*/
	move.l  #1,%d0
	jsr  OUTQ
	
	/*(4) OUTQの戻り値が0(失敗)ならば，送信割り込みをマスク(USTCNT1 を操作)して復帰*/
	cmp.l #0,%d0			
	beq MASK
	
	/*(5) dataを送信レジスタUTX1に代入して送信*/
	move.l  %d1,%d0		
	andi.l  #0x000000ff, %d0	
	add.l   #0x0800,%d0		/*ヘッダの付与*/
	move.w  %d0,UTX1	
	bra    END_of_INTERPUT
	

MASK:
	move.w #0xe108,USTCNT1
	/*move.l #0x00fffffb, IMR
	move.w #0x2000, %SR*/
	
	

END_of_INTERPUT:	
	movem.l	(%sp)+, %a0-%a2/%d0 /*レジスタ退復帰*/
	rts

*****************************************
**入力
  **チャネル ch→%d1
  **データ読み込み先の先頭アドレス p→%d2
  **送信するデータ数 size→%d3
**戻り値
  **実際に送信したデータ数 sz→ %d0 
*****************************************
	

PUTSTRING:
	movem.l	 %a0-%a3,-(%sp)	/*レジスタ退避*/
	
	/*(1) chが0でないならば，何もせずに復帰*/
	cmp.l    #0,%d1			
	bne      END_of_PUTSTRING	/*chが0でないならば何もせずに復帰*/
	
	/*(2) sz ← 0 , i ← p*/
	lea.l    sz,%a0
	lea.l    i,%a1
    move.l   #0,(%a0) /*szを初期化*/
	move.l   %d2,(%a1)		/*iに文字列ポインタを保存*/
	
	/*(3) size = 0 ならば(10)へ*/
	cmp.l    #0,%d3
 	beq      sz_SET			/*(10)へ*/
	
	/*(4) sz = size ならば (9) へ*/
loop_PUTSTRING:	
	cmp.l    (%a0),%d3
	beq      UNMASK			/*sizeが0ならばアンマスクへ*/
	
	/*(5) INQ(1,i)を実行し，送信キューへi番地のデータを書き込む*/
	move.l   #1,%d0		/*noにチャンネルをセット*/
	movea.l  (%a1),%a3
	move.b   (%a3),%d1		/*dataにp[i]をセット*/
	jsr 	 INQ

	/*(6) INQの復帰値が0(失敗/queue full)なら(9)へ*/
	cmp      #0,%d0
	beq      UNMASK

	/*(7) sz++, i++*/
	addq.l   #1,(%a0)			/*sz++*/
	addq.l   #1,(%a1)			/*i++*/

	/*(8) (4) へ*/
	bra      loop_PUTSTRING

/*(9) USTCNT1 を操作して送信割り込み許可 (アンマスク)*/
UNMASK:
	move.w #0xe10c,USTCNT1

/*(10) %D0 ←− sz*/
sz_SET:
	move.l   (%a0),%d0		/*szをセット*/

END_of_PUTSTRING:	
	movem.l	(%sp)+, %a0-%a3
	rts
	
*----------------------------------------------------------------------
* GETSTRING と INTERGET 
*----------------------------------------------------------------------
GETSTRING:
	movem.l	%d1-%d4/%a0,-(%sp)		/* レジスタの退避 */
	cmp.l	#0,%d1					/* (1) ch ≠ 0 なら何も実行せず復帰 */
	bne	GETSTRING_END

GETSTRING1:
	move.l	#0,%d4					/* (2) sz <- 0 */
	move.l	%d2,%a0					/*     i <- p */

GETSTRING2:
	cmp.l	%d3,%d4					/* (3) sz = size なら (9) へ */
	beq	GETSTRING3
	move.l	#0,%d0					/* (4) OUTQ(0,data)により受信キューから8bitデータ読み込み */
	jsr	OUTQ
	cmp.l	#0,%d0					/* (5) OUTQの復帰値(%d0の値)が 0(失敗) なら (9) へ */
	beq	GETSTRING3
	move.b	%d1,(%a0)				/* (6) i番地にdataをコピー */
	addq.l	#1,%d4					/* (7) sz++ */
	adda.l	#1,%a0					/*     i++ */
	bra	GETSTRING2					/* (8) (3) へ */

GETSTRING3:
	move.l	%d4,%d0					/* (9) %d0 <- sz */
	bra	GETSTRING_Finish
	
GETSTRING_END:
	move.l	#0,%d0

GETSTRING_Finish:
	movem.l	(%sp)+,%d1-%d4/%a0		/* レジスタの回復 */
	rts

INTERGET:
	movem.l	%d0-%d1,-(%sp)			/* レジスタの退避 */
	cmp.l	#0,%d1					/* (1) ch ≠ 0 ならば何も実行せず復帰 */
	bne	INTERGET_Finish
	move.l	#0,%d0					/* (2) INQ(0,data) */
	move.l	%d2,%d1
	jsr	INQ

INTERGET_Finish:
	movem.l	(%sp)+,%d0-%d1			/* レジスタの回復 */
	rts


*----------------------------------------------------------------------
* RESET_TIMER ルーチン (タイマ停止 & 割り込み禁止)
*
* 役割: タイマーを安全に停止し、割り込みを禁止する。
*----------------------------------------------------------------------
RESET_TIMER:
    move.l  #TCTL1, %A0        /* A0 に TCTL1 のアドレスをロード */
    andi.w  #0xFFFE, (%A0)     /* タイマー停止 (TCTL1 Bit 0 'TEN' = 0) */
    
    move.l  #IMR, %A0          /* A0 に IMR のアドレスをロード */
    * IMR Bit 1 (Timer1) を 1 (マスク/禁止) に設定する
    ori.l   #0x00000002, (%A0)
    
    move.w  #0x0000, TSTAT1   /* 保留中の割り込みフラグをクリア */
    rts

*----------------------------------------------------------------------
* SET_TIMER ルーチン (タイマ設定 & 割り込み許可)
*
* 役割: 指定時間後に指定ルーチンを呼び出すよう設定する。
* 引数:
* %D1.W : タイムアウト時間 t (0.1msec単位)
* %D2.L : コールバックルーチン p のアドレス
*----------------------------------------------------------------------
SET_TIMER:
    move.l  #TCTL1, %A0        /* A0 に TCTL1 のアドレスをロード */
    andi.w  #0xFFFE, (%A0)     /* タイマーを一旦停止 (安全のため) */
    
    lea.l   task_p, %A0        /* A0 に task_p 変数のアドレスをロード */
    move.l  %D2, (%A0)        /* task_p にコールバックのアドレス (%D2) を保存 */
    
    move.w  #206, TPRER1      /* プリスケーラを 0.1ms 周期に設定 */
    move.w  %D1, TCMP1        /* 割り込み発生時間を設定 */
    move.w  #0x0000, TSTAT1   /* 古いフラグをクリア */
    
    move.l  #IMR, %A0          /* A0 に IMR のアドレスをロード */
    * IMR Bit 1 (Timer1) を 0 (許可/アンマスク) に設定する
    andi.l  #0xFFFFFFFD, (%A0)
    
    * タイマー起動
    * (TCTL1 = 0x0015 -> IRQEN=1, FRR=1, TEN=1)
    move.w  #0x0015, TCTL1
    rts

*----------------------------------------------------------------------
* CALL_RP ルーチン (コールバック呼び出し)
*
* 役割: 割り込みハンドラから呼び出され、task_p のルーチンを実行する。
*----------------------------------------------------------------------
CALL_RP:
    lea.l   task_p, %A0        /* A0 に task_p 変数のアドレスをロード */
    move.l  (%A0), %A0     /* A0 に task_p の中身 (コールバックアドレス) をロード */
    jsr     (%A0)          /* コールバックルーチンを実行 */
    rts

*----------------------------------------------------------------------
* タイマ1 割り込みハンドラ (ベクタ番号 70)
*
* 役割: タイマー割り込み発生時にCPUから直接呼び出される。
* (bootルーチンで 0x118 番地にこのアドレスを登録する必要あり)
*----------------------------------------------------------------------
timer1_interrupt:
    * 全てのレジスタをスタックに退避
    movem.l %D0-%D7/%A0-%A6, -(%SP)

    * 割り込み要因を確認 (mon.s 方式)
    move.w  TSTAT1, %D0       /* TSTAT1 (16bit) を %D0 に読み込む */
    andi.w  #0x0001, %D0      /* Bit 0 (COMPフラグ) だけを取り出す */
    beq     .L_TIMER_EXIT   /* Bit 0 が 0 なら（タイマー要因でないなら）終了 */

    * タイマー割り込みの処理
    move.w  #0x0000, TSTAT1   /* フラグをクリア */
    jsr     CALL_RP           /* コールバック呼び出し */

.L_TIMER_EXIT:
    * 全てのレジスタをスタックから復帰
    movem.l (%SP)+, %D0-%D7/%A0-%A6
    rte                     /* 割り込みから復帰 */

****************************************************************
*** 初期値のあるデータ領域
***************************************************************

DATA1:
	.ascii "hello"
.even
DATA2:
	.ascii "world"
.even
DATA3:
	.ascii "suizu"
.even
DATA4:
	.ascii "hajime"
.even


DATA_POINTA:
	.dc.l DATA1
	.dc.l DATA2
	.dc.l DATA3
	.dc.l DATA4
.even

DATA_NUMBER:
	.dc.l 4
.even


TIME_UP_MSG:
	.ascii "\nTime Up! Next!!\n"
.even


START_MSG:
    .ascii "Typing Practice Start!\r\n"
.even
TIME_MSG:
    .ascii "Total Time: 00:00\r\n"
.even
PROMPT_PREFIX:
    .asciz "\r\nType: "
.even
TIME_UP_MSG:
    .asciz "\r\nTime's Up! Next word...\r\n"
.even


.section .text
.even

/* (1) 総合時間 (mm:ss) を計るコールバック (MAINから起動) */
TIMER_1SEC_CALLBACK:
    movem.l %d0-%d7/%a0-%a6,-(%SP)   /* レジスタ退避 */
    lea.l   TOTAL_SECONDS, %a0
    addq.l  #1, (%a0)
    bsr     UPDATE_TIMER_DISPLAY
    movem.l (%SP)+,%D0-%d7/%a0-%a6   /* レジスタ復帰 */
    rts

/* (2) お題ごとの制限時間を計るコールバック (DISPLAY_NEXT_PROMPTから起動) */
WORD_TIMER_CALLBACK:
    movem.l %a0, -(%SP)
    lea.l   WORD_TIMER_FLAG, %a0
    move.b  #1, (%a0)           /* 時間切れフラグを立てる */
    movem.l (%SP)+, %a0
    rts

/* (3) bsr の直後の .asciz 文字列を表示する */
PUTSTRING_CALL_LIB:
    move.l  (%sp), %a0      /* 戻り先アドレス(文字列の先頭)を %a0 に取得 */
    move.l  %a0, %d2        /* %d2 = p (文字列のアドレス) */
    clr.l   %d3             /* %d3 = size */
COUNT_LEN:
    cmp.b   #0, (%a0)+      /* ヌル文字か？ */
    beq     DO_PUTSTRING
    addq.l  #1, %d3
    bra     COUNT_LEN
DO_PUTSTRING:
    move.l  (%sp), %a0
    add.l   %d3, %a0
    addq.l  #1, %a0         /* +1 (ヌル文字分) */
    move.l  %a0, (%sp)      /* スタック上の戻り先を更新 */
    move.l  #SYSCALL_NUM_PUTSTRING, %d0
    move.l  #0, %d1
    trap    #0
    rts

/* (4) 次のお題を表示し、タイマーをセットする */
DISPLAY_NEXT_PROMPT:
    movem.l %d0-%d3/%a0-%a1, -(%SP)
    move.w  TCN1, %d0       /* 乱数の素 */
    andi.l  #0x0000FFFF, %d0
    move.l  DATA_NUMBER, %d1
    divu.w  %d1, %d0        /* %d0 = [余り | 商] */
    swap    %d0             /* %d0 = [商 | 余り] */
    andi.l  #0x0000FFFF, %d0 /* %d0 = 余り (お題のインデックス) */
    mulu.w  #4, %d0         /* インデックス * 4 (ポインタサイズ) */
    lea.l   DATA_POINTA, %a0
    adda.l  %d0, %a0        /* %a0 = PROMPT_LIST[インデックス] のアドレス */
    move.l  (%a0), %a1      /* %a1 = お題のアドレス (例: "hello" のアドレス) */
    move.l  %a1, CURRENT_DATA_PTR
    clr.l   %d3             /* %d3 = length */
    move.l  %a1, %a0
COUNT_LEN_LOOP:
    cmp.b   #0, (%a0)+
    beq     LEN_CALC_DONE
    addq.l  #1, %d3
    bra     COUNT_LEN_LOOP
LEN_CALC_DONE:
    move.l  %d3, %d1
    mulu.w  #8000, %d1      /* 1文字 0.8秒 */
    move.l  #SYSCALL_NUM_SET_TIMER, %d0
    move.l  #WORD_TIMER_CALLBACK, %d2
    trap    #0
    lea.l   WORD_TIMER_FLAG, %a0
    move.b  #0, (%a0)       /* 時間切れフラグを 0 に */
    lea.l   CURRENT_CHAR_INDEX, %a0
    move.l  #0, (%a0)       /* 文字インデックスを 0 に */
    bsr     PUTSTRING_CALL_LIB
    .asciz "\r\nType: "
.even
    move.l  #SYSCALL_NUM_PUTSTRING, %d0
    move.l  #0, %d1
    move.l  CURRENT_DATA_PTR, %d2
    trap    #0
    movem.l (%SP)+, %d0-%d3/%a0-%a1
    rts

/* (5) 総合時間 (mm:ss) を表示更新する */
UPDATE_TIMER_DISPLAY:
    movem.l %d0-%d3/%a0-%a1, -(%SP)
    bsr     PUTSTRING_CALL_LIB
    .asciz "\rTotal Time: "
.even
    lea.l   TOTAL_SECONDS, %a0
    move.l  (%a0), %d0      /* %d0 = 合計秒数 */
    move.w  #60, %d1
    divu.w  %d1, %d0        /* %d0 = [余り(ss) | 商(mm)] */
    swap    %d0             /* %d0 = [商(mm) | 余り(ss)] */
    move.l  %d0, %d1
    andi.l  #0x0000FFFF, %d1 /* %d1 = 余り(ss) */
    swap    %d0             /* %d0 = 商(mm) */
    andi.l  #0x0000FFFF, %d0
    lea.l   TIME_BUF, %a0
    bsr     NUM_TO_STR_2DIGIT /* %d0(mm) -> TIME_BUF */
    move.l  #SYSCALL_NUM_PUTSTRING, %d0
    move.l  #0, %d1
    move.l  #TIME_BUF, %d2
    move.l  #2, %d3         /* size = 2 */
    trap    #0
    bsr     PUTSTRING_CALL_LIB
    .asciz ":"
.even
    move.l  %d1, %d0        /* %d0 = ss */
    lea.l   TIME_BUF, %a0
    bsr     NUM_TO_STR_2DIGIT /* %d0(ss) -> TIME_BUF */
    move.l  #SYSCALL_NUM_PUTSTRING, %d0
    move.l  #0, %d1
    move.l  #TIME_BUF, %d2
    move.l  #2, %d3         /* size = 2 */
    trap    #0
    bsr     PUTSTRING_CALL_LIB
    .asciz "\r\nType: "
.even
    lea.l   CURRENT_DATA_PTR, %a0
    move.l  (%a0), %d2      /* %d2 = お題のアドレス */
    lea.l   CURRENT_CHAR_INDEX, %a0
    move.l  (%a0), %d3      /* %d3 = size (現在までのインデックス) */
    cmp.l   #0, %d3
    beq     UPDATE_TIMER_END /* まだ入力がなければ何もしない */
    move.l  #SYSCALL_NUM_PUTSTRING, %d0
    move.l  #0, %d1
    trap    #0
UPDATE_TIMER_END:
    movem.l (%SP)+, %d0-%d3/%a0-%a1
    rts
    
/* (6) 2桁の数値をASCIIに変換する */
NUM_TO_STR_2DIGIT:
    move.l  %d0, %d1
    move.w  #10, %d0
    divu.w  %d0, %d1        /* %d1 = [余り(1の位) | 商(10の位)] */
    swap    %d1
    andi.l  #0x0000FFFF, %d1 /* %d1 = 商 (10の位) */
    add.b   #0x30, %d1      /* '0' */
  S move.b  %d1, (%a0)+
    swap    %d1             /* %d1 = [ 0 | 余り(1の位) ] */
    andi.l  #0x0000FFFF, %d1
    add.b   #0x30, %d1      /* '0' */
    move.b  %d1, (%a0)
  t rts

/* (7) BUF の1文字をエコーバックする */
ECHO_CHAR_BUF:
    movem.l %d0-%d3, -(%SP)
    move.l  #SYSCALL_NUM_PUTSTRING, %d0
    move.l  #0, %d1
    move.l  #BUF, %d2
    move.l  #1, %d3
  t trap    #0
    movem.l (%SP)+, %d0-%d3
    rts


*************************************************
* PUTSTRING_CALL_LIB:
* bsr の直後に .asciz で定義された文字列を PUTSTRING で表示する
*************************************************
PUTSTRING_CALL_LIB:
    move.l  (%sp), %a0      ; 戻り先アドレス(文字列の先頭)を %a0 に取得
    move.l  %a0, %d2        ; %d2 = p (文字列のアドレス)
    clr.l   %d3             ; %d3 = size
COUNT_LEN:
    cmp.b   #0, (%a0)+      ; ヌル文字か？
    beq     DO_PUTSTRING
    addq.l  #1, %d3
    bra     COUNT_LEN
DO_PUTSTRING:
    move.l  (%sp), %a0
    add.l   %d3, %a0
    addq.l  #1, %a0         ; +1 (ヌル文字分)
    move.l  %a0, (%sp)      ; スタック上の戻り先を更新
    move.l  #SYSCALL_NUM_PUTSTRING, %d0
    move.l  #0, %d1
    trap    #0
    rts

*************************************************
** DISPLAY_NEXT_PROMPT:
** 次のお題をランダムに選び、表示し、お題タイマーをセットする
*************************************************
DISPLAY_NEXT_PROMPT:
    movem.l %d0-%d3/%a0-%a1, -(%SP)
    
    /* 1. 乱数(0 ～ PROMPT_COUNT-1) を生成*/
    move.w  TCN1, %d0       /* 乱数の素*/
    andi.l  #0x0000FFFF, %d0
    move.l  PROMPT_COUNT, %d1
    divu.w  %d1, %d0        /* %d0 = [余り | 商]*/
    swap    %d0             /* %d0 = [商 | 余り]*/
    andi.l  #0x0000FFFF, %d0 /* %d0 = 余り (お題のインデックス)*/
    
    /* 2. お題のアドレスを取得*/
    mulu.w  #4, %d0         /* インデックス * 4 (ポインタサイズ)*/
    lea.l   PROMPT_LIST, %a0
    adda.l  %d0, %a0        /* %a0 = PROMPT_LIST[インデックス] のアドレス*/
    move.l  (%a0), %a1      /* %a1 = お題のアドレス (例: "hello" のアドレス)*/
    move.l  %a1, CURRENT_PROMPT_PTR
    
    /* 3. お題の長さを計算*/
    clr.l   %d3             /* %d3 = length*/
    move.l  %a1, %a0
COUNT_LEN_LOOP:
    cmp.b   #0, (%a0)+
    beq     LEN_CALC_DONE
    addq.l  #1, %d3
    bra     COUNT_LEN_LOOP
LEN_CALC_DONE:
    
    /* 4. 制限時間 t を計算 (例: 1文字あたり 0.8秒 = 8000)*/
    move.l  %d3, %d1        /* %d1 = length*/
    mulu.w  #8000, %d1      /* %d1 = t (0.1ms単位)*/
    
    /* 5. ★お題ごとタイマーをセット*/
    move.l  #SYSCALL_NUM_SET_TIMER, %d0
    /* %d1 は t (計算済み)*/
    move.l  #WORD_TIMER_CALLBACK, %d2 /* 時間切れフラグを立てるコールバック*/
    trap    #0
    
    /* 6. 変数をリセット*/
    lea.l   WORD_TIMER_FLAG, %a0
    move.b  #0, (%a0)           /* 時間切れフラグを 0 に*/
    lea.l   CURRENT_CHAR_INDEX, %a0
    move.l  #0, (%a0)           /* 文字インデックスを 0 に*/
    
    /* 7. お題を表示*/
    bsr     PUTSTRING_CALL_LIB
    .asciz "\r\nType: "
.even
    move.l  #SYSCALL_NUM_PUTSTRING, %d0
    move.l  #0, %d1
    move.l  CURRENT_PROMPT_PTR, %d2 /* p = お題のアドレス*/
    /* %d3 は length (計算済み)*/
    trap    #0

    movem.l (%SP)+, %d0-%d3/%a0-%a1
    rts

*************************************************
** UPDATE_TIMER_DISPLAY:
** TOTAL_SECONDS を "Time: mm:ss" 形式で表示
*************************************************
UPDATE_TIMER_DISPLAY:
    movem.l %d0-%d3/%a0-%a1, -(%SP)
    
    bsr     PUTSTRING_CALL_LIB  /* "Total Time: "*/
    .asciz "\rTotal Time: " /* \r で行頭に戻る*/
.even
    
    lea.l   TOTAL_SECONDS, %a0
    move.l  (%a0), %d0      /* %d0 = 合計秒数*/
    move.w  #60, %d1
    divu.w  %d1, %d0        /* %d0 = [余り(ss) | 商(mm)]*/
    swap    %d0             /* %d0 = [商(mm) | 余り(ss)]*/
    move.l  %d0, %d1
    andi.l  #0x0000FFFF, %d1 /* %d1 = 余り(ss)*/
    swap    %d0             /* %d0 = 商(mm)*/
    andi.l  #0x0000FFFF, %d0

    lea.l   TIME_BUF, %a0
    bsr     NUM_TO_STR_2DIGIT /* %d0(mm) -> TIME_BUF*/
    
    move.l  #SYSCALL_NUM_PUTSTRING, %d0
    move.l  #0, %d1
    move.l  #TIME_BUF, %d2
    move.l  #2, %d3         /* size = 2*/
    trap    #0

    bsr     PUTSTRING_CALL_LIB
    .asciz ":"
.even
    
    move.l  %d1, %d0        /* %d0 = ss*/
    lea.l   TIME_BUF, %a0
    bsr     NUM_TO_STR_2DIGIT /* %d0(ss) -> TIME_BUF*/

    move.l  #SYSCALL_NUM_PUTSTRING, %d0
    move.l  #0, %d1
    move.l  #TIME_BUF, %d2
    move.l  #2, %d3         /* size = 2*/
    trap    #0
    
    bsr     PUTSTRING_CALL_LIB  /* お題の行頭に戻る*/
    .asciz "\r\nType: "
.even
    
    ; 現在のお題の入力済み部分を再表示する
    lea.l   CURRENT_PROMPT_PTR, %a0
    move.l  (%a0), %d2      /* %d2 = お題のアドレス*/
    lea.l   CURRENT_CHAR_INDEX, %a0
    move.l  (%a0), %d3      /* %d3 = size (現在までのインデックス)*/
    cmp.l   #0, %d3
    beq     UPDATE_TIMER_END /* まだ入力がなければ何もしない*/
    
    move.l  #SYSCALL_NUM_PUTSTRING, %d0
    move.l  #0, %d1
    trap    #0
    
UPDATE_TIMER_END:
    movem.l (%SP)+, %d0-%d3/%a0-%a1
    rts
    
*************************************************
* NUM_TO_STR_2DIGIT: 2桁の数値 (0-99) をASCII文字列に変換
*************************************************
NUM_TO_STR_2DIGIT:
    move.l  %d0, %d1
    move.w  #10, %d0
    divu.w  %d0, %d1        /* %d1 = [余り(1の位) | 商(10の位)]*/
    swap    %d1
    andi.l  #0x0000FFFF, %d1 ; %d1 = 商 (10の位)
    add.b   #0x30, %d1      /* '0'*/
    move.b  %d1, (%a0)+
    swap    %d1             /* %d1 = [ 0 | 余り(1の位) ]*/
    andi.l  #0x0000FFFF, %d1
    add.b   #0x30, %d1      /* '0'*/
    move.b  %d1, (%a0)
    rts

*************************************************
* ECHO_CHAR_BUF:
* BUF [cite: 2648-2650] の内容 (1文字) を PUTSTRING で表示する
*************************************************
ECHO_CHAR_BUF:
    movem.l %d0-%d3, -(%SP)
    move.l  #SYSCALL_NUM_PUTSTRING, %d0
    move.l  #0, %d1
    move.l  #BUF, %d2
    move.l  #1, %d3
    trap    #0
    movem.l (%SP)+, %d0-%d3
    rts







