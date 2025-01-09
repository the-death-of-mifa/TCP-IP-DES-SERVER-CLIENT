#include <stdio.h>
#include <string.h>
#include "des.h"

char *encryptmsg(char *plaintext, char *cipher)
{
    int len = strlen(plaintext);
    char temp[8], tempcipher[8];
    bool temp_bits[64], tempcipher_bits[64];
    int index = 0;

    // 分组处理，每组64位8个字节
    for (int i = 0; i < len; i += 8)
    {
        memset(temp, 0, sizeof(temp));
        memset(tempcipher, 0, sizeof(tempcipher));

        for (int j = 0; j < 8; j++)
        {
            if (i + j < len)
            {
                temp[j] = plaintext[i + j];
            }
            else
            {
                temp[j] = 0; // 对于不足8字节的部分填充0
            }
        }

        // 将字节转换为位
        BytesToBits(temp, temp_bits);

        // 加密
        encrypt(temp_bits, tempcipher_bits);

        // 将位转换为字节
        BitsToBytes(tempcipher_bits, tempcipher);

        for (int j = 0; j < 8; j++)
        {
            cipher[index++] = tempcipher[j];
        }
    }
    return cipher;
}

char *decryptmsg(char *cipher, char *decrypted)
{
    int len = strlen(cipher);
    char temp[8], tempplain[8];
    bool temp_bits[64], tempplain_bits[64];
    int index = 0;

    // 分组处理，每组64位8个字节
    for (int i = 0; i < len; i += 8)
    {
        memset(temp, 0, sizeof(temp));
        memset(tempplain, 0, sizeof(tempplain));

        for (int j = 0; j < 8; j++)
        {
            temp[j] = cipher[i + j];
        }

        // 将字节转换为位
        BytesToBits(temp, temp_bits);

        // 解密
        decrypt(temp_bits, tempplain_bits);

        // 将位转换为字节
        BitsToBytes(tempplain_bits, tempplain);

        for (int j = 0; j < 8; j++)
        {
            decrypted[index++] = tempplain[j];
        }
    }
    return decrypted;
}

// 主函数用于测试
int main()
{
    char plaintext[] = "这是一条DES加密测试长消息";
    char cipher[64] = {0};    // 假设密文缓冲区足够大
    char decrypted[64] = {0}; // 假设解密后的缓冲区足够大
    char byteskey[] = "12345678";
    BytesToBits(byteskey, key);
    // 生成子密钥
    GenerateSubKeys();

    // 加密
    encryptmsg(plaintext, cipher);

    // 打印密文
    printf("Cipher: ");
    for (int i = 0; i < strlen(cipher); i++)
    {
        printf("%02X ", (unsigned char)cipher[i]);
    }
    printf("\n");

    // 解密
    decryptmsg(cipher, decrypted);

    // 打印解密后的明文
    printf("Decrypted: %s\n", decrypted);

    return 0;
}