#include <stdio.h>

/* FILE構造体を使うために必要 */
#include <fcntl.h> 

int main() {
    int fd_port2;
    FILE *fp2;
    char buf[256];

    /* fd=3 を UART2 (デバイスID=1) に割り当てる */
    /* 注: 実験テキストにより fcntl の仕様確認が必要 */
    fd_port2 = 3;
    fcntl(fd_port2, F_SETFL, 1); /* 1 = UART2 device ID */

    /* fdopen で FILE構造体と結びつける */
    fp2 = fdopen(fd_port2, "w+");

    printf("Start Port1(Standard)\n");
    fprintf(fp2, "Start Port2(Extended)\n");

    while(1) {
        /* ポート2から読み込んでポート1(標準)に出力 */
        if (fscanf(fp2, "%s", buf) > 0) {
            printf("Received from Port2: %s\n", buf);
        }
        
        /* 逆も実装可能 */
    }
    return 0;
}
