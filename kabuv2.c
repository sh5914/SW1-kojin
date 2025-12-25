#include <stdio.h>
#include <fcntl.h>
#include "mtk_c.h"

FILE *fdopen(int fd, const char *mode);
FILE *fp2;

/* --------------------------------------------------------------------
   共有変数
   -------------------------------------------------------------------- */
volatile int current_price = 100;
volatile int market_open = 1;

volatile int p1_cash = 1000;
volatile int p1_stocks = 0;
volatile int p2_cash = 1000;
volatile int p2_stocks = 0;

/* 【変更】両方の合意用フラグ */
volatile int p1_ready = 0;
volatile int p2_ready = 0;

#define MAX_LOOPS 20 

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

unsigned long next = 1;
int random(void) {
    next = next * 1103515245 + 12345;
    return (unsigned int)(next/65536) % 32768;
}

/* --------------------------------------------------------------------
   Task 3: 市場タスク
   -------------------------------------------------------------------- */
void task_market() {
    int loop_count = 0;
    int change = 0;
    volatile int w;
    
    printf("--- MARKET OPEN! Price: %d ---\n", current_price);
    fprintf(fp2, "--- MARKET OPEN! Price: %d ---\n", current_price);

    while(market_open) {
        
        /* ターン開始時にReadyフラグをリセット */
        p1_ready = 0;
        p2_ready = 0;

        /* --- ウェイト処理 (両者合意でスキップ) --- */
        for(w=0; w<5000000; w++) {
            /* 両方とも ready になったらブレイク */
            if (p1_ready == 1 && p2_ready == 1) {
                break; 
            }
        }

        /* --- 価格変動 --- */
        if ((random() % 100) < 5) { /* 5% Event */
            if ((random() % 2) == 0) {
                change = 30 + (random() % 21);
                printf("\n!!! BREAKING NEWS: POSITIVE SURPRISE !!!\n");
                fprintf(fp2, "\n!!! BREAKING NEWS: POSITIVE SURPRISE !!!\n");
            } else {
                change = -30 - (random() % 21);
                printf("\n!!! BREAKING NEWS: MARKET CRASH !!!\n");
                fprintf(fp2, "\n!!! BREAKING NEWS: MARKET CRASH !!!\n");
            }
        } 
        else {
            change = (random() % 21) - 10;
        }

        current_price += change;
        if (current_price < 1) current_price = 1;
        loop_count++;

        printf("[NEWS] Price Updated (%d/%d): %d (Change: %d)\n", 
               loop_count, MAX_LOOPS, current_price, change);
        fprintf(fp2, "[NEWS] Price Updated (%d/%d): %d (Change: %d)\n", 
                loop_count, MAX_LOOPS, current_price, change);

        if (loop_count >= MAX_LOOPS) {
            market_open = 0;
        }
    }
    
    /* --- 結果発表 --- */
    printf("\n--- MARKET CLOSED ---\n");
    fprintf(fp2, "\n--- MARKET CLOSED ---\n");

    int total1 = p1_cash + (p1_stocks * current_price);
    int total2 = p2_cash + (p2_stocks * current_price);

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

    while(1);
}

/* --------------------------------------------------------------------
   プレイヤー共通処理
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
    /* 【変更】Nextコマンド: 自分のフラグだけ立てる */
    else if (cmd[0] == 'n') { 
        if (player_id == 1) {
            p1_ready = 1;
            fprintf(fp, ">> Waiting for Player 2...\n");
        } else {
            p2_ready = 1;
            fprintf(fp, ">> Waiting for Player 1...\n");
        }
    }
}

/* --------------------------------------------------------------------
   Tasks & Main
   -------------------------------------------------------------------- */
void task_trader1() {
    char buf[16];
    printf("Commands: b=Buy, s=Sell, i=Info, n=Next\n");

    while(market_open) {
        if (get_line(stdin, buf, 16) > 0) {
            process_trade(1, buf, stdout, &p1_cash, &p1_stocks);
        }
    }
    while(1);
}

void task_trader2() {
    char buf[16];
    fprintf(fp2, "Commands: b=Buy, s=Sell, i=Info, n=Next\n");

    while(market_open) {
        if (get_line(fp2, buf, 16) > 0) {
            process_trade(2, buf, fp2, &p2_cash, &p2_stocks);
        }
    }
    while(1);
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
