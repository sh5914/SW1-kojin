#include <stdio.h>
#include <fcntl.h>
#include "mtk_c.h"

/* FILE構造体を使うために必要 */
/* プロトタイプ宣言 */
FILE *fdopen(int fd, const char *mode);

/* ポート2用のファイルポインタをグローバルにする（各タスクで使うため） */
FILE *fp2;

/* -----------------------------------------------------------
 * Task 1: ポート0(stdin) から読んで、ポート1(fp2) へ書く
 * ----------------------------------------------------------- */
void task_port0_to_port1() {
    char buf[64];
    
    /* 起動メッセージをポート0に出す */
    printf("Task1 Start: Input from Port0 -> Send to Port1\n");

    while(1) {
        /* ポート0(標準入力)からの入力を待つ */
        /* ※ここで入力待ちになっても、タイマ割り込みでTask2に切り替わるので止まりません */
        if (scanf("%s", buf) > 0) {
            
            /* 読み込んだ文字をポート1へ出力 */
            fprintf(fp2, "[From Port0]: %s\n", buf);
            
            /* 自分の画面(ポート0)にも確認表示 */
            printf("  (Sent to Port1: %s)\n", buf);
        }
    }
}

/* -----------------------------------------------------------
 * Task 2: ポート1(fp2) から読んで、ポート0(stdout) へ書く
 * ----------------------------------------------------------- */
void task_port2_to_port0() {
    char buf[64];

    /* 起動メッセージをポート1に出す */
    fprintf(fp2, "Task2 Start: Input from Port1 -> Send to Port0\n");

    while(1) {
        /* ポート1からの入力を待つ */
        if (fscanf(fp2, "%s", buf) > 0) {
            
            /* 読み込んだ文字をポート0(標準出力)へ出力 */
            printf("[From Port1]: %s\n", buf);
            
            /* 向こうの画面(ポート1)にも確認表示 */
            fprintf(fp2, "  (Sent to Port0: %s)\n", buf);
        }
    }
}

/* -----------------------------------------------------------
 * Main 関数: 初期化とタスク登録
 * ----------------------------------------------------------- */
int main() {
    int fd_port2;

    /* --- ポート2の準備 --- */
    /* fd=3 を UART2 (デバイスID=1) に割り当てる */
    fd_port2 = 3;
    fcntl(fd_port2, 1, 1); /* arg=1 (UART2) */

    /* fdopen で FILE構造体と結びつける */
    fp2 = fdopen(fd_port2, "r+"); /* 読み書き両用 */


    /* --- カーネルの初期化とタスク起動 --- */
    init_kernel();

    /* タスクを登録 */
    set_task(task_port0_to_port1);
    set_task(task_port2_to_port0);

    /* マルチタスク開始 (ここからは戻ってこない) */
    begin_sch();

    return 0;
}
