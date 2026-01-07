#include <stdio.h>
#include <fcntl.h>
#include "mtk_c.h"

FILE *fdopen(int fd, const char *mode);
FILE *fp2;

extern void P(int semid);
extern void V(int semid);

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

/* ゲーム設定 */
#define MAX_LOOPS 20 
#define ATTACK_COST 500  /* 攻撃にかかる費用 */
#define FREEZE_TURNS 3   /* 相手を止める期間 */

/* --------------------------------------------------------------------
   入力・乱数関数
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
        /* 1. 変数の初期化 (リスタート時にここに戻る) */
        
        /* ★排他制御開始: 初期化中は誰も触らせない */
        P(SEM_MUTEX);
        current_price = 100;
        market_open = 1;
        p1_cash = 1000; p1_stocks = 0;
        p2_cash = 1000; p2_stocks = 0;
        p1_ready = 0; p2_ready = 0;
        p1_freeze_timer = 0; p2_freeze_timer = 0;
        loop_count = 0;
        V(SEM_MUTEX);
        /* ★排他制御終了 */

        /* 開始ウェイト（表示被り防止） */
        for(w=0; w<100000; w++); 

        printf("\n\n=== NEW GAME START ===\n");
        printf("--- MARKET OPEN! Price: %d ---\n", current_price);
        fprintf(fp2, "\n\n=== NEW GAME START ===\n");
        fprintf(fp2, "--- MARKET OPEN! Price: %d ---\n", current_price);

        /* 2. ゲームメインループ */
        while(market_open) {
            
            /* フリーズタイマー処理 */
            if (p1_freeze_timer > 0) {
                p1_freeze_timer--;
                if (p1_freeze_timer == 0) printf("\n>> SYSTEM RECOVERED! You can trade now.\n");
            }
            if (p2_freeze_timer > 0) {
                p2_freeze_timer--;
                if (p2_freeze_timer == 0) fprintf(fp2, "\n>> SYSTEM RECOVERED! You can trade now.\n");
            }

            p1_ready = 0;
            p2_ready = 0;

            /* ウェイト処理 */
            for(w=0; w<800000; w++) {
                next++; 
                int p1_ok = (p1_ready == 1) || (p1_freeze_timer > 0);
                int p2_ok = (p2_ready == 1) || (p2_freeze_timer > 0);
                if (p1_ok && p2_ok) break; 
            }

            /* --- 価格変動 --- */
            if ((random() % 100) < 10) { 
                if ((random() % 2) == 0) {
                    /* 暴騰 */
                    change = 30 + (random() % 21);
                    printf("\n!!! BREAKING NEWS: POSITIVE SURPRISE !!!\n");
                    fprintf(fp2, "\n!!! BREAKING NEWS: POSITIVE SURPRISE !!!\n");
                } else {
                    /* 暴落 */
                    change = -30 - (random() % 21);
                    printf("\n!!! BREAKING NEWS: MARKET CRASH !!!\n");
                    fprintf(fp2, "\n!!! BREAKING NEWS: MARKET CRASH !!!\n");
                }
            } else {
                /* 通常変動 */
                change = (random() % 21) - 10;
            }

            /* ★排他制御: 価格更新中はロック */
            P(SEM_MUTEX);
            current_price += change;
            if (current_price < 1) current_price = 1;
            V(SEM_MUTEX);

            loop_count++;

            /* 通知 */
            printf("[NEWS] Price Updated (%d/%d): %d (Change: %d)\n", 
                   loop_count, MAX_LOOPS, current_price, change);
            fprintf(fp2, "[NEWS] Price Updated (%d/%d): %d (Change: %d)\n", 
                    loop_count, MAX_LOOPS, current_price, change);

            if (loop_count >= MAX_LOOPS) {
                market_open = 0;
            }
        }
        
        /* 3. 結果発表 */
        printf("\n--- MARKET CLOSED ---\n");
        fprintf(fp2, "\n--- MARKET CLOSED ---\n");

        /* 計算中の整合性確保 */
        P(SEM_MUTEX);
        int total1 = p1_cash + (p1_stocks * current_price);
        int total2 = p2_cash + (p2_stocks * current_price);
        V(SEM_MUTEX);

        printf("\n=== GAME FINISHED ===\n");
        fprintf(fp2, "\n=== GAME FINISHED ===\n");

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

        /* ★同期制御: P1からの合図があるまでスリープ(Wait)して待つ */
        P(SEM_RESTART);
    }
}

/* --------------------------------------------------------------------
   プレイヤー共通処理
   -------------------------------------------------------------------- */
void process_trade(int player_id, char *cmd, FILE *fp, 
                   volatile int *cash, volatile int *stocks) {
    
    /* 1. フリーズチェック */
    if (player_id == 1 && p1_freeze_timer > 0) {
        fprintf(fp, ">> SYSTEM ERROR: You are under CYBER ATTACK! (%d turns left)\n", p1_freeze_timer);
        return;
    }
    if (player_id == 2 && p2_freeze_timer > 0) {
        fprintf(fp, ">> SYSTEM ERROR: You are under CYBER ATTACK! (%d turns left)\n", p2_freeze_timer);
        return;
    }

    /* ★排他制御開始 */
    P(SEM_MUTEX);

    /* 2. コマンド処理 */
    if (cmd[0] == 'b') { /* Buy */
        if (*cash >= current_price) {
            *cash -= current_price;
            *stocks += 1;
            fprintf(fp, ">> BOUGHT! (Cash: %d, Stock: %d)\n", *cash, *stocks);
        } else {
            fprintf(fp, ">> Not enough cash!\n");
        }
    }
    else if (cmd[0] == 's') { /* Sell */
        if (*stocks > 0) {
            *cash += current_price;
            *stocks -= 1;
            fprintf(fp, ">> SOLD! (Cash: %d, Stock: %d)\n", *cash, *stocks);
        } else {
            fprintf(fp, ">> No stocks to sell!\n");
        }
    }
    else if (cmd[0] == 'i') { /* Info */
        fprintf(fp, ">> Assets: Cash=%d, Stocks=%d (Total: %d)\n", 
                *cash, *stocks, *cash + (*stocks * current_price));
    }
    else if (cmd[0] == 'n') { /* Next */
        if (player_id == 1) {
            p1_ready = 1;
            fprintf(fp, ">> Waiting for Player 2...\n");
        } else {
            p2_ready = 1;
            fprintf(fp, ">> Waiting for Player 1...\n");
        }
    }
    else if (cmd[0] == 'k') { /* Kill / Cyber Attack */
        if (*cash >= ATTACK_COST) {
            *cash -= ATTACK_COST;
            
            if (player_id == 1) {
                p2_freeze_timer = FREEZE_TURNS;
                fprintf(fp, ">> CYBER ATTACK LAUNCHED on P2! (-%d Cash)\n", ATTACK_COST);
                printf(">> ALERT: CYBER ATTACK DETECTED on P2!\n");
                fprintf(fp2, "\n>> WARNING: YOU HAVE BEEN HACKED BY P1!\n");
            } else {
                p1_freeze_timer = FREEZE_TURNS;
                fprintf(fp, ">> CYBER ATTACK LAUNCHED on P1! (-%d Cash)\n", ATTACK_COST);
                fprintf(fp2, ">> ALERT: CYBER ATTACK DETECTED on P1!\n");
                printf("\n>> WARNING: YOU HAVE BEEN HACKED BY P2!\n");
            }

        } else {
            fprintf(fp, ">> Not enough cash for attack! (Need %d)\n", ATTACK_COST);
        }
    }

    /* ★排他制御終了 */
    V(SEM_MUTEX);
}

/* --------------------------------------------------------------------
   Tasks & Main
   -------------------------------------------------------------------- */
void task_trader1() {
    char buf[16];
    printf("Commands: b=Buy, s=Sell, i=Info, n=Next, k=Attack(%d)\n", ATTACK_COST);

    while(1) {
        /* ゲーム中のループ */
        while(market_open) {
            if (get_line(stdin, buf, 16) > 0) {
                process_trade(1, buf, stdout, &p1_cash, &p1_stocks);
            }
        }

        /* ゲーム終了後、Enterキー入力を待つ */
        if (get_line(stdin, buf, 16) >= 0) {
            /* ★同期制御: 待っている市場タスクを起こす(Signal) */
            V(SEM_RESTART);
            
            /* 市場タスクが変数を初期化し、market_openを1にするのを待つ */
            while(market_open == 0); 
        }
    }
}

void task_trader2() {
    char buf[16];
    fprintf(fp2, "Commands: b=Buy, s=Sell, i=Info, n=Next, k=Attack(%d)\n", ATTACK_COST);

    while(1) {
        /* ゲーム中のループ */
        while(market_open) {
            if (get_line(fp2, buf, 16) > 0) {
                process_trade(2, buf, fp2, &p2_cash, &p2_stocks);
            }
        }

        /* ゲーム終了後、次のゲーム開始まで待機 */
        /* P1がEnterを押してMarketが再開させるのを待つ */
        while(market_open == 0);
    }
}

int main() {
    int fd_port2;

    fd_port2 = 3;
    fcntl(fd_port2, 1, 1); 
    fp2 = fdopen(fd_port2, "r+");
    if (fp2 == NULL) while(1);

    init_kernel();


    set_task(task_trader1);
    set_task(task_trader2);
    set_task(task_market);

    begin_sch();
    return 0;
}
