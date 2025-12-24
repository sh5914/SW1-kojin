#include <stdio.h>
#include <fcntl.h>
#include "mtk_c.h"

/* --------------------------------------------------------------------
   プロトタイプ宣言とグローバル変数
   -------------------------------------------------------------------- */
FILE *fdopen(int fd, const char *mode);

/* ポート2用のファイルポインタ (Task1, Task2で共有) */
FILE *fp2;

/* --------------------------------------------------------------------
   自作の入力関数: getline
   scanf の代わりにこれを使います。
   1文字ずつ fgetc (内部で inbyte) で読み込み、改行が来たら終了します。
   -------------------------------------------------------------------- */
int getline(FILE *fp, char *buf, int limit) {
    int i = 0;
    int c;

    while (i < limit - 1) {
        /* 1文字読み込む */
        /* ※ fgetc は内部で csys68k.c の read -> inbyte を呼び出します */
        c = fgetc(fp);

        /* エラーまたはEOFなら終了 */
        if (c == EOF) {
            break;
        }

        /* 改行コード(\r や \n)の処理 */
        /* Windows/Unixの違いを吸収するため \r は無視し \n で判定 */
        if (c == '\r') {
            continue;
        }
        if (c == '\n') {
            break; /* 入力完了 */
        }

        /* バッファに格納 */
        buf[i] = (char)c;
        i++;
    }

    /* 文字列の終端(ヌル文字)を必ず付ける */
    buf[i] = '\0';

    return i; /* 読み込んだ文字数を返す */
}

/* --------------------------------------------------------------------
   Task 1: ポート0(PC側) から入力 -> ポート1(拡張側) へ送信
   -------------------------------------------------------------------- */
void task_port0_to_port1() {
    /* スタックオーバーフロー防止のためサイズは小さめに */
    char buf[64];
    
    printf("Task1 Start: Input from Port0 -> Send to Port1\n");

    while(1) {
        /* getline で stdin (ポート0) から1行読み込む */
        /* 入力があるまでここで待機します */
        if (getline(stdin, buf, 64) > 0) {
            
            /* 読み込んだ文字列をポート1へ送信 */
            /* getlineは改行を削除しているので、送信時に \n をつける */
            fprintf(fp2, "[From Port0]: %s\n", buf);
            
            /* 自分の画面にも確認表示 */
            printf("  (Sent to Port1: %s)\n", buf);
        }
    }
}

/* --------------------------------------------------------------------
   Task 2: ポート1(拡張側) から入力 -> ポート0(PC側) へ送信
   -------------------------------------------------------------------- */
void task_port2_to_port0() {
    char buf[64];

    fprintf(fp2, "Task2 Start: Input from Port1 -> Send to Port0\n");

    while(1) {
        /* getline で fp2 (ポート1) から1行読み込む */
        if (getline(fp2, buf, 64) > 0) {
            
            /* 読み込んだ文字列をポート0(標準出力)へ送信 */
            printf("[From Port1]: %s\n", buf);
            
            /* 向こうの画面にも確認表示 */
            fprintf(fp2, "  (Sent to Port0: %s)\n", buf);
        }
    }
}

/* --------------------------------------------------------------------
   Main 関数
   -------------------------------------------------------------------- */
int main() {
    int fd_port2;

    /* --- ポート2 (UART2) の準備 --- */
    /* fd=3 を デバイスID=1 (UART2) に割り当て */
    fd_port2 = 3;
    
    /* csys68k.c の fcntl を呼ぶ */
    fcntl(fd_port2, 1, 1); 

    /* fdopen で FILE構造体を作成 */
    fp2 = fdopen(fd_port2, "r+");

    /* エラーチェック */
    if (fp2 == NULL) {
        printf("PANIC: fdopen failed! Check heap or fd table.\n");
        while(1);
    }

    /* --- カーネルの初期化 --- */
    init_kernel();

    /* タスク登録 */
    set_task(task_port0_to_port1);
    set_task(task_port2_to_port0);

    /* スケジューリング開始 */
    begin_sch();

    return 0;
}
