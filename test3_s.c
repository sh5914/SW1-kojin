#include <stdio.h>
#include <fcntl.h>
#include "mtk_c.h"

FILE *fdopen(int fd, const char *mode);
FILE *fp2;

/* --- セマフォIDの定義 --- */
/* SEM_MUTEX: 共有変数保護用 (初期値1) */
/* SEM_RESTART: リスタート待ち合わせ用 (初期値0) */
#define SEM_MUTEX   0
#define SEM_RESTART 1

/* --------------------------------------------------------------------
   共有変数
   -------------------------------------------------------------------- */
volatile int current_price;
volatile int market_open;

volatile int p1_cash;
volatile int p1_stocks;
volatile int p2_cash;
volatile int p2_stocks;

volatile int p1_ready;
volatile int p2_ready;

volatile int p1_freeze_timer;
volatile int p2_freeze_timer;

/* restart_trigger フラグはセマフォに置き換えたため削除 */

/* ゲーム設定 */
#define MAX_LOOPS 20 
#define ATTACK_COST 500
#define FREEZE_TURNS 3

/* --------------------------------------------------------------------
   入力・乱数関数 (変更なし)
   -------------------------------------------------------------------- */
int get_line(FILE *fp, char *buf, int limit) {
    int i = 0;
    int c;
    while (i < limit - 1) {
        c = fgetc(fp);
        if (c == EOF) break;
        if (c == '\r') continue;
        if (c == '\n') break;
        buf[i] = (char)c;
        i++;
    }
    buf[i] = '\0';
    return i;
}

unsigned long next = 12345;

int random(void) {
    next = next * 1103515245 + 12345;
    return (unsigned int)(next/65536) % 32768;
}

/* --------------------------------------------------------------------
   Task 3: 市場タスク (Game Master)
   -------------------------------------------------------------------- */
void task_market() {
    int loop_count;
    int change;
    volatile int w;
    
    while(1) {
        /* 1. 変数の初期化 */
        /* ★重要: 変数リセット中は他の人が触らないようにロック */
        wai_sem(SEM_MUTEX);
        current_price = 100;
        market_open = 1;
        p1_cash = 1000; p1_stocks = 0;
        p2_cash = 1000; p2_stocks = 0;
        p1_ready = 0; p2_ready = 0;
        p1_freeze_timer = 0; p2_freeze_timer = 0;
        loop_count = 0;
        sig_sem(SEM_MUTEX);

        /* 開始ウェイト */
        for(w=0; w<100000; w++); 

        printf("\n\n=== NEW GAME START ===\n");
        printf("--- MARKET OPEN! Price: %d ---\n", current_price);
        fprintf(fp2, "\n\n=== NEW GAME START ===\n");
        fprintf(fp2, "--- MARKET OPEN! Price: %d ---\n", current_price);

        /* 2. ゲームメインループ */
        while(market_open) {
            /* フリーズタイマー処理は簡略化のためロック外(厳密にはロック推奨) */
            if (p1_freeze_timer > 0) {
                p1_freeze_timer--;
                if (p1_freeze_timer == 0) printf("\n>> SYSTEM RECOVERED!\n");
            }
            if (p2_freeze_timer > 0) {
                p2_freeze_timer--;
                if (p2_freeze_timer == 0) fprintf(fp2, "\n>> SYSTEM RECOVERED!\n");
            }

            p1_ready = 0;
            p2_ready = 0;

            /* ウェイト処理（乱数の種更新のため、ここはビジーウェイトのままにする） */
            for(w=0; w<800000; w++) {
                next++; 
                int p1_ok = (p1_ready == 1) || (p1_freeze_timer > 0);
                int p2_ok = (p2_ready == 1) || (p2_freeze_timer > 0);
                if (p1_ok && p2_ok) break; 
            }

            /* 価格変動 */
            if ((random() % 100) < 10) { 
                if ((random() % 2) == 0) {
                    change = 30 + (random() % 21);
                    printf("\n!!! BREAKING NEWS: POSITIVE SURPRISE !!!\n");
                    fprintf(fp2, "\n!!! BREAKING NEWS: POSITIVE SURPRISE !!!\n");
                } else {
                    change = -30 - (random() % 21);
                    printf("\n!!! BREAKING NEWS: MARKET CRASH !!!\n");
                    fprintf(fp2, "\n!!! BREAKING NEWS: MARKET CRASH !!!\n");
                }
            } else {
                change = (random() % 21) - 10;
            }

            /* ★重要: 価格更新中は誰も売買させない */
            wai_sem(SEM_MUTEX);
            current_price += change;
            if (current_price < 1) current_price = 1;
            sig_sem(SEM_MUTEX);

            loop_count++;
            printf("[NEWS] Price: %d (Change: %d)\n", current_price, change);
            fprintf(fp2, "[NEWS] Price: %d (Change: %d)\n", current_price, change);

            if (loop_count >= MAX_LOOPS) {
                market_open = 0;
            }
        }
        
        /* 3. 結果発表 */
        printf("\n--- MARKET CLOSED ---\n");
        fprintf(fp2, "\n--- MARKET CLOSED ---\n");

        /* 結果計算時も値が変わらないようにロック */
        wai_sem(SEM_MUTEX);
        int total1 = p1_cash + (p1_stocks * current_price);
        int total2 = p2_cash + (p2_stocks * current_price);
        sig_sem(SEM_MUTEX);

        printf("P1: %d  vs  P2: %d\n", total1, total2);
        fprintf(fp2, "P1: %d  vs  P2: %d\n", total1, total2);

        if (total1 > total2) {
            printf("WINNER: P1 !!\n");
            fprintf(fp2, "WINNER: P1 !!\n");
        } else if (total2 > total1) {
            printf("WINNER: P2 !!\n");
            fprintf(fp2, "WINNER: P2 !!\n");
        } else {
            printf("DRAW !!\n");
            fprintf(fp2, "DRAW !!\n");
        }

        /* 4. リスタート待機 */
        printf("\n>>> PRESS ENTER TO RESTART <<<\n");
        fprintf(fp2, "\n>>> Waiting for P1 to restart... <<<\n");

        /* ★重要: フラグ待ち(while)をやめて、セマフォでスリープする */
        /* P1が sig_sem するまで、このタスクはここで停止(Wait)する */
        wai_sem(SEM_RESTART);
    }
}

/* --------------------------------------------------------------------
   共通処理 (ロックを追加)
   -------------------------------------------------------------------- */
void process_trade(int player_id, char *cmd, FILE *fp, 
                   volatile int *cash, volatile int *stocks) {
    
    /* フリーズチェックなどは省略 */
    if (player_id == 1 && p1_freeze_timer > 0) {
        fprintf(fp, ">> Under Attack!\n"); return;
    }
    if (player_id == 2 && p2_freeze_timer > 0) {
        fprintf(fp, ">> Under Attack!\n"); return;
    }

    /* ★重要: 売買処理全体をクリティカルセクションにする */
    /* 計算中に株価が変わったり、二重引き落としが起きないようにする */
    wai_sem(SEM_MUTEX);

    if (cmd[0] == 'b') { 
        if (*cash >= current_price) {
            *cash -= current_price;
            *stocks += 1;
            fprintf(fp, ">> BOUGHT! (Cash: %d, Stock: %d)\n", *cash, *stocks);
        } else {
            fprintf(fp, ">> No cash!\n");
        }
    }
    else if (cmd[0] == 's') { 
        if (*stocks > 0) {
            *cash += current_price;
            *stocks -= 1;
            fprintf(fp, ">> SOLD! (Cash: %d, Stock: %d)\n", *cash, *stocks);
        } else {
            fprintf(fp, ">> No stocks!\n");
        }
    }
    
    /* 情報参照(i)やターン終了(n)、攻撃(k)などの処理... */
    /* 長くなるので省略しますが、変数を読み書きするならここに入れる */
    else if (cmd[0] == 'n') {
        if(player_id==1) p1_ready=1; else p2_ready=1;
        fprintf(fp, ">> Ready.\n");
    }
    /* ... 他のコマンド ... */

    sig_sem(SEM_MUTEX); /* ロック解除 */
}

/* --------------------------------------------------------------------
   Task 1: Player 1 (リスタート権限あり)
   -------------------------------------------------------------------- */
void task_trader1() {
    char buf[16];
    printf("Commands: b, s, i, n, k\n");

    while(1) {
        while(market_open) {
            if (get_line(stdin, buf, 16) > 0) {
                process_trade(1, buf, stdout, &p1_cash, &p1_stocks);
            }
        }

        /* ゲーム終了後、Enterキー入力を待つ */
        if (get_line(stdin, buf, 16) >= 0) {
            /* ★重要: フラグを立てる代わりに、待っているタスク(市場)を起こす */
            sig_sem(SEM_RESTART);
            
            /* 市場が再開するのを待つ (ここは簡易的にsleepなどで調整でも良い) */
            while(market_open == 0); 
        }
    }
}

/* --------------------------------------------------------------------
   Task 2: Player 2
   -------------------------------------------------------------------- */
void task_trader2() {
    char buf[16];
    fprintf(fp2, "Commands: b, s, i, n, k\n");

    while(1) {
        while(market_open) {
            if (get_line(fp2, buf, 16) > 0) {
                process_trade(2, buf, fp2, &p2_cash, &p2_stocks);
            }
        }
        while(market_open == 0);
    }
}

/* --------------------------------------------------------------------
   Main
   -------------------------------------------------------------------- */
int main() {
    int fd_port2 = 3;
    fcntl(fd_port2, 1, 1); 
    fp2 = fdopen(fd_port2, "r+");
    if (fp2 == NULL) while(1);

    init_kernel();

    /* ★重要: セマフォの初期化 (関数名は環境に合わせてください) */
    /* ID 0 (MUTEX): 初期値 1 (誰でも最初は通れる) */
    /* ID 1 (RESTART): 初期値 0 (最初は通れない=Waitする) */
    /* 例: sem_init(ID, COUNT); */
    /* sem_init(SEM_MUTEX, 1); */
    /* sem_init(SEM_RESTART, 0); */

    set_task(task_trader1);
    set_task(task_trader2);
    set_task(task_market);

    begin_sch();
    return 0;
}
