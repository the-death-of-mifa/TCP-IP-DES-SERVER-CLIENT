#include <stdio.h>
#include <D:\msys\home\YYDS\gmp-6.2.0\gmp.h>
#include "RSA.h"
#include <string.h>

int main()
{
    // 初始化大整数变量
    mpz_t n, e, d, m, c;
    mpz_inits(n, e, d, m, c, NULL);

    int nbits = 1024; // 密钥长度（比特数）
    int ebits = 16;   // 公钥 e 的长度（比特数）

    printf("正在生成 %d 位密钥...\n", nbits);
    setKey(n, e, d, nbits, ebits);

    // 输出生成的公钥和私钥
    printf("公钥:\n");
    gmp_printf("e = %Zd\n", e);
    gmp_printf("n = %Zd\n", n);

    printf("\n私钥:\n");
    gmp_printf("d = %Zd\n", d);
    gmp_printf("n = %Zd\n", n);

    // 输入明文字符串
    char input[1024];
    printf("\n请输入要加密的明文: ");
    fgets(input, sizeof(input), stdin);
    input[strcspn(input, "\n")] = '\0'; // 去除换行符

    // 将字符串转换为大整数
    string_to_mpz(m, input);

    // 加密操作
    encrypt(c, m, e, n);
    printf("\n加密后的密文:\n");
    gmp_printf("%Zd\n", c);

    // 解密操作
    decrypt(m, c, d, n);

    // 将解密后的大整数转换为字符串
    char output[1024];
    mpz_to_string(output, m);
    printf("\n解密后的明文:\n%s\n", output);

    // 验证加解密是否成功
    if (strcmp(input, output) == 0)
    {
        printf("\n加解密验证成功！\n");
    }
    else
    {
        printf("\n加解密验证失败！\n");
    }

    // 释放内存
    mpz_clears(n, e, d, m, c, NULL);

    return 0;
}
