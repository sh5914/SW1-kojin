#include "mtk_c.h"

/* グローバル変数の実体定義 [cite: 694-701] */
TCB_TYPE task_tab[NUMTASK + 1];
STACK_TYPE stacks[NUMTASK];
TASK_ID_TYPE ready;
TASK_ID_TYPE curr_task;
TASK_ID_TYPE new_task;
TASK_ID_TYPE next_task;
SEMAPHORE_TYPE semaphore[NUMSEMAPHORE];

/*----------------------------------------------------------------
 * init_kernel() : カーネルの初期化 [cite: 785]
 *----------------------------------------------------------------*/
void init_kernel() {
    int i;

    /* 1. TCB 配列の初期化: すべて空タスクとする */
    for(i = 0; i <= NUMTASK; i++) {
        task_tab[i].next = NULLTASKID;
        task_tab[i].status = 0; /* 0: 未使用と定義 */
    }

    /* 2. ready キューの初期化: 空とする */
    ready = NULLTASKID;

    /* 3. P・Vシステムコールの割り込み処理ルーチンを登録 */
    /* TRAP #1 はベクタ番号 33。アドレスは 33 * 4 = 132 (0x84) */
    /* 割り込みベクタへの書き込み */
    *(void **)0x00000084 = pv_handler;

    /* 4. セマフォの値を初期化する */
    for(i = 0; i < NUMSEMAPHORE; i++) {
        semaphore[i].count = 1;      /* 初期値1 (使用可能) */
        semaphore[i].task_list = NULLTASKID;
        semaphore[i].nst = 0;
    }
}

/*----------------------------------------------------------------
 * init_stack(id) : スタックの初期化 [cite: 803]
 * 指定されたタスクのスーパーバイザスタックに初期値を積む
 *----------------------------------------------------------------*/
void *init_stack(int id) {
    /* スタックの底から積んでいくためにポインタを設定 */
    /* stacks[]は0から始まるため、タスクID(1〜)とずれるので id-1 */
    int *sp = (int *)&stacks[id - 1].sstack[STKSIZE]; 

    /* 図2.8  に従い、スタックにプッシュしていく */
    /* ※スタックはアドレスが減る方向に伸びるため、先にデクリメント(--)する */

    /* 1. initial PC (タスクの開始アドレス) */
    *(--sp) = (int)task_tab[id].task_addr;

    /* 2. initial SR (ステータスレジスタ) */
    /* 上位2バイト分として扱うため、ポインタをshortキャストして操作するか、
       32bitで積んで調整する。ここではシンプルに下位16bitをSRとする形で積む */
    /* 実際は SR(2byte) + 詰め物(2byte) か、PCの後ろに配置される */
    /* 68000の割り込みスタックフレームは PC(4byte) -> SR(2byte) */
    
    /* 注意: Cのポインタ操作で正確に配置するため、spをshort型で操作 */
    short *ssp = (short *)sp;
    *(--ssp) = 0x0000;  /* initial SR: ユーザモード, 割り込み許可 */
    
    /* 3. 15本のレジスタ (D0-D7, A0-A6) + USP 用の領域確保 */
    /* レジスタ退避は movem.l で行われるため、4byte * 15個分 */
    /* さらに USP の保存場所として 4byte 必要 */
    /* 合計 16個の long word 分、ポインタを進める */
    
    int *isp = (int *)ssp; /* intポインタに戻す */
    isp -= 16; /* 16個分下げる (D0-D7, A0-A6, USP) */

    /* initial USP の設定 */
    /* レジスタ退避領域の一番上（スタック的には最後）に USP を入れる */
    /* movem.l で退避される順番と first_task での復帰順序に合わせる */
    /* first_task実装を見ると、スタックトップにあるのが USP */
    
    *isp = (int)&stacks[id - 1].ustack[STKSIZE]; /* ユーザスタックの底を設定 */

    /* 初期化完了時点のスタックポインタ(SSP)を返す */
    return (void *)isp;
}

/*----------------------------------------------------------------
 * set_task(func) : ユーザタスクの登録 [cite: 792]
 *----------------------------------------------------------------*/
void set_task(void (*func)()) {
    int i;
    
    /* 1. タスク IDの決定 (空きスロット探索) */
    /* 0番はNULLTASKIDなので1から探す */
    for(i = 1; i <= NUMTASK; i++) {
        /* status == 0 を未使用と仮定 */
        if (task_tab[i].status == 0) {
            new_task = i;
            break;
        }
    }
    
    /* 空きがない場合の処理（今回は考慮不要だが安全のため） */
    if (i > NUMTASK) return; 

    /* 2. TCBの更新 */
    task_tab[new_task].task_addr = func;
    task_tab[new_task].status = 1; /* 使用中 */

    /* 3. スタックの初期化 */
    /* init_stackを呼び出し、戻り値(SSP)をTCBに保存 */
    task_tab[new_task].stack_ptr = init_stack(new_task);

    /* 4. キューへの登録 */
    /* 別途実装される addq 関数を使って ready キューに追加 */
    addq(&ready, new_task);
}

/*----------------------------------------------------------------
 * begin_sch() : マルチタスク処理の開始 [cite: 808]
 *----------------------------------------------------------------*/
void begin_sch() {
    /* 1. 最初のタスクの決定 */
    /* readyキューから先頭を取り出す */
    curr_task = removeq(&ready);

    /* タスクがなければエラーだが、実験環境なので無視して進むか無限ループ */
    if (curr_task == NULLTASKID) {
        while(1); 
    }

    /* 2. タイマの設定 */
    /* アセンブリ関数の init_timer を呼ぶ */
    init_timer();

    /* 3. 最初のタスクの起動 */
    /* アセンブリ関数の first_task を呼び出し、戻ってこない */
    first_task();
}
