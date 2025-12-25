#include <stdio.h>
#include <fcntl.h>
#include "mtk_c.h"

FILE *fdopen(int fd, const char *mode);
FILE *fp2;

/* --------------------------------------------------------------------
   共有変数（市場データ・プレイヤー資産）
   -------------------------------------------------------------------- */
volatile int current_price = 100; /* 現在の株価 (初期値100円) */
volatile int market_open = 1;     /* 市場が開いているか */

/* プレイヤー1の資産 */
volatile int p1_cash = 1000;      /* 現金 */
volatile int p1_stocks = 0;       /* 保有株数 */

/* プレイヤー2の資産 */
volatile int p2_cash = 1000;
volatile int p2_stocks = 0;

/* ゲーム設定 */
#define MAX_LOOPS 20 /* 価格変動の回数 */

/* --------------------------------------------------------------------
   入力用関数 (get_line)
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

/* 簡易乱数生成 (線形合同法) */
unsigned long next = 1;

int random(void) {
    next = next * 1103515245 + 12345;
    return (unsigned int)(next/65536) % 32768;
}

/* --------------------------------------------------------------------
   Task 3: 市場タスク (Market Maker & Referee)
   -------------------------------------------------------------------- */
void task_market() {
    int loop_count = 0;
    int change = 0;
    
    printf("--- MARKET OPEN! Price: %d ---\n", current_price);
    fprintf(fp2, "--- MARKET OPEN! Price: %d ---\n", current_price);

    while(market_open) {
        /* ウェイト (ユーザー入力待ち時間) */
        volatile int w;
        for(w=0; w<3000000; w++); /* 500万回ループ */

        /* --- 価格変動ロジック --- */
        
        /* 5%の確率でイベント発生 (0〜99のうち5未満) */
        if ((random() % 100) < 5) {
            
            /* さらに50%の確率で 暴騰 or 暴落 */
            if ((random() % 2) == 0) {
                /* 暴騰 (+30 〜 +50) */
                change = 30 + (random() % 21);
                printf("\n!!! BREAKING NEWS: POSITIVE SURPRISE !!!\n");
                fprintf(fp2, "\n!!! BREAKING NEWS: POSITIVE SURPRISE !!!\n");
            } else {
                /* 暴落 (-30 〜 -50) */
                change = -30 - (random() % 21);
                printf("\n!!! BREAKING NEWS: MARKET CRASH !!!\n");
                fprintf(fp2, "\n!!! BREAKING NEWS: MARKET CRASH !!!\n");
            }
        } 
        else {
            /* 通常変動 (-10 〜 +10) */
            change = (random() % 21) - 10;
        }

        current_price += change;

        /* 株価が1円以下にならないように */
        if (current_price < 1) current_price = 1;

        loop_count++;

        /* 新しい株価と進行状況を通知 */
        printf("[NEWS] Price Updated (%d/%d): %d (Change: %d)\n", 
               loop_count, MAX_LOOPS, current_price, change);
        fprintf(fp2, "[NEWS] Price Updated (%d/%d): %d (Change: %d)\n", 
                loop_count, MAX_LOOPS, current_price, change);

        /* 指定回数変動したら終了 */
        if (loop_count >= MAX_LOOPS) {
            market_open = 0;
        }
    }
    
    printf("\n--- MARKET CLOSED ---\n");
    fprintf(fp2, "\n--- MARKET CLOSED ---\n");

    /* --- 結果発表・勝敗判定 --- */
    
    int total1 = p1_cash + (p1_stocks * current_price);
    int total2 = p2_cash + (p2_stocks * current_price);

    printf("\n==============================\n");
    printf("        GAME FINISHED         \n");
    printf("==============================\n");
    fprintf(fp2, "\n==============================\n");
    fprintf(fp2, "        GAME FINISHED         \n");
    fprintf(fp2, "==============================\n");

    printf("P1 Total: %d  (Cash:%d, Stock:%d)\n", total1, p1_cash, p1_stocks);
    printf("P2 Total: %d  (Cash:%d, Stock:%d)\n", total2, p2_cash, p2_stocks);
    
    fprintf(fp2, "P1 Total: %d  (Cash:%d, Stock:%d)\n", total1, p1_cash, p1_stocks);
    fprintf(fp2, "P2 Total: %d  (Cash:%d, Stock:%d)\n", total2, p2_cash, p2_stocks);

    printf("------------------------------\n");
    fprintf(fp2, "------------------------------\n");

    if (total1 > total2) {
        printf("   WINNER: PLAYER 1 !!!   \n");
        fprintf(fp2, "   WINNER: PLAYER 1 !!!   \n");
    } else if (total2 > total1) {
        printf("   WINNER: PLAYER 2 !!!   \n");
        fprintf(fp2, "   WINNER: PLAYER 2 !!!   \n");
    } else {
        printf("      DRAW (Tie Game)     \n");
        fprintf(fp2, "      DRAW (Tie Game)     \n");
    }
    
    printf("==============================\n");
    fprintf(fp2, "==============================\n");

    while(1);
}

/* --------------------------------------------------------------------
   プレイヤータスク共通処理
   -------------------------------------------------------------------- */
void process_trade(int player_id, char *cmd, FILE *fp, 
                   volatile int *cash, volatile int *stocks) {
    
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
}

/* --------------------------------------------------------------------
   Task 1: Player 1 (Trader A)
   -------------------------------------------------------------------- */
void task_trader1() {
    char buf[16];
    printf("Commands: b=Buy, s=Sell, i=Info\n");

    while(market_open) {
        if (get_line(stdin, buf, 16) > 0) {
            process_trade(1, buf, stdout, &p1_cash, &p1_stocks);
        }
    }
    while(1);
}

/* --------------------------------------------------------------------
   Task 2: Player 2 (Trader B)
   -------------------------------------------------------------------- */
void task_trader2() {
    char buf[16];
    fprintf(fp2, "Commands: b=Buy, s=Sell, i=Info\n");

    while(market_open) {
        if (get_line(fp2, buf, 16) > 0) {
            process_trade(2, buf, fp2, &p2_cash, &p2_stocks);
        }
    }
    while(1);
}

/* --------------------------------------------------------------------
   Main
   -------------------------------------------------------------------- */
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
