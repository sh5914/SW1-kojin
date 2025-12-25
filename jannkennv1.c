#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include "mtk_c.h"

/* --------------------------------------------------------------------
   設定・定数
   -------------------------------------------------------------------- */
FILE *fdopen(int fd, const char *mode);
FILE *fp2; /* ポート2用 */

#define HAND_NONE  0
#define HAND_GU    1
#define HAND_CHOKI 2
#define HAND_PA    3

/* 勝った時の獲得ポイント */
#define PT_GU    3
#define PT_CHOKI 2
#define PT_PA    1
#define GOAL_SCORE 10 /* 勝利目標 */

/* 共有変数 */
volatile int p1_hand = HAND_NONE;
volatile int p2_hand = HAND_NONE;
volatile int score1 = 0;
volatile int score2 = 0;

/* --------------------------------------------------------------------
   自作入力関数 (test3.cから流用)
   -------------------------------------------------------------------- */
int getline(FILE *fp, char *buf, int limit) {
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

int str_to_hand(char *str) {
    if (str[0] == 'g') return HAND_GU;
    if (str[0] == 'c') return HAND_CHOKI;
    if (str[0] == 'p') return HAND_PA;
    return HAND_NONE;
}

char *hand_to_str(int hand) {
    if (hand == HAND_GU) return "GU";
    if (hand == HAND_CHOKI) return "CHOKI";
    if (hand == HAND_PA) return "PA";
    return "Error";
}

/* --------------------------------------------------------------------
   Task 1: Player 1 (Port 0)
   -------------------------------------------------------------------- */
void task_player1() {
    char buf[16];
    int hand;

    printf("\n*** Strategic Janken [Player 1] ***\n");
    printf("Rules: GU=3pt, CHOKI=2pts, PA=1pts\n");
    printf("Goal: %d points\n", GOAL_SCORE);

    while(1) {
        while (p1_hand != HAND_NONE); /* 待機 */

        /* 現在のスコア表示 */
        printf("\n[Score] You: %d  vs  P2: %d\n", score1, score2);
        printf("[P1] Your Turn (g/c/p): ");
        
        if (getline(stdin, buf, 16) > 0) {
            hand = str_to_hand(buf);
            if (hand == HAND_NONE) {
                printf("Invalid! (g/c/p)\n");
                continue;
            }
            printf("Selected! Waiting for P2...\n");
            p1_hand = hand;
        }
    }
}

/* --------------------------------------------------------------------
   Task 2: Player 2 (Port 1)
   -------------------------------------------------------------------- */
void task_player2() {
    char buf[16];
    int hand;

    fprintf(fp2, "\n*** Strategic Janken [Player 2] ***\n");
    fprintf(fp2, "Rules: GU=1pt, CHOKI=2pts, PA=3pts\n");
    fprintf(fp2, "Goal: %d points\n", GOAL_SCORE);

    while(1) {
        while (p2_hand != HAND_NONE); /* 待機 */

        fprintf(fp2, "\n[Score] You: %d  vs  P1: %d\n", score2, score1);
        fprintf(fp2, "[P2] Your Turn (g/c/p): ");
        
        if (getline(fp2, buf, 16) > 0) {
            hand = str_to_hand(buf);
            if (hand == HAND_NONE) {
                fprintf(fp2, "Invalid! (g/c/p)\n");
                continue;
            }
            fprintf(fp2, "Selected! Waiting for P1...\n");
            p2_hand = hand;
        }
    }
}

/* --------------------------------------------------------------------
   Task 3: 審判 (Referee)
   -------------------------------------------------------------------- */
void task_referee() {
    int earned_pt = 0;
    int winner = 0; /* 0:Draw, 1:P1, 2:P2 */

    while(1) {
        /* 手が出揃うのを待つ */
        if (p1_hand != HAND_NONE && p2_hand != HAND_NONE) {
            
            /* 表示ウェイト */
            int w; for(w=0; w<10000; w++);

            char *h1s = hand_to_str(p1_hand);
            char *h2s = hand_to_str(p2_hand);

            /* 結果表示ヘッダ */
            printf("\n--- BATTLE ---\n");
            printf("P1: %s  vs  P2: %s\n", h1s, h2s);
            fprintf(fp2, "\n--- BATTLE ---\n");
            fprintf(fp2, "P2: %s  vs  P1: %s\n", h2s, h1s);

            /* --- 勝敗判定とポイント計算 --- */
            winner = 0;
            earned_pt = 0;

            if (p1_hand == p2_hand) {
                winner = 0; /* あいこ */
            }
            /* P1が勝つパターン */
            else if ((p1_hand == HAND_GU    && p2_hand == HAND_CHOKI) ||
                     (p1_hand == HAND_CHOKI && p2_hand == HAND_PA)    ||
                     (p1_hand == HAND_PA    && p2_hand == HAND_GU)) {
                winner = 1;
                /* 勝った手によってポイント決定 */
                if (p1_hand == HAND_GU)    earned_pt = PT_GU;
                if (p1_hand == HAND_CHOKI) earned_pt = PT_CHOKI;
                if (p1_hand == HAND_PA)    earned_pt = PT_PA;
            }
            /* P2が勝つパターン */
            else {
                winner = 2;
                if (p2_hand == HAND_GU)    earned_pt = PT_GU;
                if (p2_hand == HAND_CHOKI) earned_pt = PT_CHOKI;
                if (p2_hand == HAND_PA)    earned_pt = PT_PA;
            }

            /* --- 結果反映 --- */
            if (winner == 0) {
                printf(">> DRAW (0 pts)\n");
                fprintf(fp2, ">> DRAW (0 pts)\n");
            }
            else if (winner == 1) {
                score1 += earned_pt;
                printf(">> YOU WIN! (+%d pts)\n", earned_pt);
                fprintf(fp2, ">> YOU LOSE... (P1 +%d pts)\n", earned_pt);
            }
            else {
                score2 += earned_pt;
                printf(">> YOU LOSE... (P2 +%d pts)\n", earned_pt);
                fprintf(fp2, ">> YOU WIN! (+%d pts)\n", earned_pt);
            }

            /* --- 優勝決定判定 --- */
            if (score1 >= GOAL_SCORE || score2 >= GOAL_SCORE) {
                printf("\n===========================\n");
                fprintf(fp2, "\n===========================\n");
                
                if (score1 >= GOAL_SCORE) {
                    printf("  CONGRATULATIONS! P1 WINS!  \n");
                    fprintf(fp2, "      GAME OVER. P1 WINS.    \n");
                } else {
                    printf("      GAME OVER. P2 WINS.    \n");
                    fprintf(fp2, "  CONGRATULATIONS! P2 WINS!  \n");
                }
                
                printf("===========================\n");
                fprintf(fp2, "===========================\n");

                /* スコアリセットして再開 */
                score1 = 0;
                score2 = 0;
                printf("New Game Starts...\n");
                fprintf(fp2, "New Game Starts...\n");
            }

            /* 手をリセット（次のターンへ） */
            p1_hand = HAND_NONE;
            p2_hand = HAND_NONE;
        }
    }
}

/* --------------------------------------------------------------------
   Main 関数
   -------------------------------------------------------------------- */
int main() {
    int fd_port2;

    fd_port2 = 3;
    fcntl(fd_port2, 1, 1); 
    fp2 = fdopen(fd_port2, "r+");

    if (fp2 == NULL) {
        printf("PANIC: fdopen failed!\n");
        while(1);
    }

    init_kernel();

    set_task(task_player1);
    set_task(task_player2);
    set_task(task_referee);

    begin_sch();
    return 0;
}
