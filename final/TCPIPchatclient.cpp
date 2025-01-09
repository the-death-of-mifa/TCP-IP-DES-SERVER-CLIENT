#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>
#include <conio.h>
#include <cerrno>
#include "RSA.h"
#include "des.h"
#include <D:\msys\home\YYDS\gmp-6.2.0\gmp.h>

#pragma comment(lib, "ws2_32.lib")

#define BUFFER_SIZE 1024

SOCKET client_socket;

int is_logged_in = 0;
char username[50];
int exit_group_chat = 0; // 新的标志变量
int rsa_correct = 0;
int des_correct = 0;

void set_console_color(int color)
{
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, color);
}

// RSA的各种函数/定义起始

#define RSA_KEY_BITS 512
mpz_t n, e, d; // RSA密钥对

void generate_rsa_keypair_and_send(SOCKET server_socket)
{
    mpz_inits(n, e, d, NULL);
    mpz_t p, q, phi, gcd;
    mpz_inits(p, q, phi, gcd, NULL);
    char buffer[BUFFER_SIZE];

    fd_set read_fds;
    struct timeval timeout;
    int bytes_received;

    setKey(n, e, d, RSA_KEY_BITS, 65537);

    // 将公钥发送给客户端
    char n_str[1024], e_str[1024];                           // n_str存储n的16进制字符串，e_str存储e的16进制字符串
    mpz_get_str(n_str, 16, n);                               // 将n转换为16进制字符串
    mpz_get_str(e_str, 16, e);                               // 将e转换为16进制字符串
    sprintf(buffer, "RSA_KEY %s %s 无需解密", n_str, e_str); // 将n和e拼接成字符串
    set_console_color(10);
    printf("发送RSA公钥: %s %s\n", n_str, e_str);
    set_console_color(15);
    send(server_socket, buffer, strlen(buffer), 0); // 发送n和e

    while (1)
    {
        // 清空接收缓冲区
        memset(buffer, 0, BUFFER_SIZE);

        // 设置文件描述符集
        FD_ZERO(&read_fds);
        FD_SET(client_socket, &read_fds);

        // 设置超时时间
        timeout.tv_sec = 5; // 5秒超时
        timeout.tv_usec = 0;

        int select_result = select(0, &read_fds, NULL, NULL, &timeout);
        if (select_result > 0)
        {
            if (FD_ISSET(client_socket, &read_fds))
            {
                // 接收服务器响应
                bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
                if (bytes_received > 0)
                {
                    buffer[bytes_received] = '\0';
                    set_console_color(10);
                    printf("rsa服务器响应: %s\n", buffer);
                    set_console_color(15);
                    if (strstr(buffer, "已存储") != NULL)
                    {
                        rsa_correct = 1;
                    }
                    break; // 成功接收到数据后退出循环
                }
                else if (bytes_received == 0)
                {
                    set_console_color(12);
                    printf("服务器关闭连接。\n");
                    set_console_color(15);
                    break;
                }
                else
                {
                    int error_code = WSAGetLastError();
                    if (error_code == WSAEWOULDBLOCK)
                    {
                        set_console_color(12);
                        printf("recv would block\n");
                        set_console_color(15);
                        continue;
                    }
                    else
                    {
                        set_console_color(12);
                        printf("recv failed: %d\n", error_code);
                        set_console_color(15);
                        break;
                    }
                }
            }
        }
        else if (select_result == 0)
        {
            set_console_color(12);
            printf("等待超时...\n");
            set_console_color(15);
            continue;
        }
        else
        {
            set_console_color(12);
            printf("select failed: %d\n", WSAGetLastError());
            set_console_color(15);
            break;
        }
    }
    return;
}

// RSA的各种函数/定义结束

// DES的各种函数/定义起始

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

void receive_des_key(SOCKET client_socket)
{
    mpz_inits(n, e, d, NULL);
    fd_set read_fds;
    struct timeval timeout;
    int bytes_received;
    char keytemp[17] = {0};
    char buffer[BUFFER_SIZE] = "DES_KEY";
    send(client_socket, buffer, strlen(buffer), 0);

    while (1)
    {
        // 清空接收缓冲区
        memset(buffer, 0, BUFFER_SIZE);

        // 设置文件描述符集
        FD_ZERO(&read_fds);
        FD_SET(client_socket, &read_fds);

        // 设置超时时间
        timeout.tv_sec = 5; // 5秒超时
        timeout.tv_usec = 0;

        int select_result = select(0, &read_fds, NULL, NULL, &timeout);
        if (select_result > 0)
        {
            if (FD_ISSET(client_socket, &read_fds))
            {
                // 接收服务器响应
                bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
                if (bytes_received > 0)
                {
                    mpz_t encrypted_msg, decrypted_msg;
                    mpz_init(decrypted_msg);
                    mpz_init_set_str(encrypted_msg, buffer, 16); // 将16进制字符串转换为大整数
                    RSAdecrypt(decrypted_msg, encrypted_msg, d, n);
                    mpz_to_string(buffer, decrypted_msg); // 将大整数转换为字符串
                    mpz_clears(encrypted_msg, decrypted_msg, NULL);
                    buffer[bytes_received] = '\0';
                    set_console_color(10);
                    printf("des服务器响应: %s\n", buffer);
                    set_console_color(15);
                    if (strstr(buffer, "密钥生成成功") != NULL)
                    {
                        if (sscanf(buffer, "DES密钥生成成功: %s", keytemp) == 1) // 从服务器响应中提取DES密钥
                        {
                            for (int i = 0; i < 8; i++)
                            {
                                sscanf(&keytemp[i * 2], "%2hhx", &keytemp[i]); // 两个字符转换为一个字节
                            }
                            BytesToBits(keytemp, key); // 将字节转换为位
                            GenerateSubKeys();         // 生成子密钥
                            des_correct = 1;
                        }
                        else
                        {
                            set_console_color(12);
                            printf("des接收格式错误\n");
                            set_console_color(15);
                        }
                    }
                    break; // 成功接收到数据后退出循环
                }
                else if (bytes_received == 0)
                {
                    set_console_color(12);
                    printf("服务器关闭连接。\n");
                    set_console_color(15);
                    break;
                }
                else
                {
                    int error_code = WSAGetLastError();
                    if (error_code == WSAEWOULDBLOCK)
                    {
                        set_console_color(12);
                        printf("recv would block\n");
                        set_console_color(15);
                        continue;
                    }
                    else
                    {
                        set_console_color(12);
                        printf("recv failed: %d\n", error_code);
                        set_console_color(15);
                        break;
                    }
                }
            }
        }
        else if (select_result == 0)
        {
            set_console_color(12);
            printf("等待超时...\n");
            set_console_color(15);
            continue;
        }
        else
        {
            set_console_color(12);
            printf("select failed: %d\n", WSAGetLastError());
            set_console_color(15);
            break;
        }
    }
}

void receive_and_decrypt_des_key(SOCKET clientSock)
{
    char buffer[BUFFER_SIZE];
    unsigned char tempkey[1024];
    fd_set read_fds;
    struct timeval timeout;
    int bytes_received;
    char keytemp[512] = {0};

    // 发送请求
    sprintf(buffer, "DES_KEY");
    send(clientSock, buffer, strlen(buffer), 0);

    set_console_color(10);
    printf("等待服务器响应...\n");
    set_console_color(15);

    while (1)
    {
        // 清空接收缓冲区
        memset(buffer, 0, BUFFER_SIZE);

        // 设置文件描述符集
        FD_ZERO(&read_fds);
        FD_SET(clientSock, &read_fds);

        // 设置超时时间
        timeout.tv_sec = 5; // 5秒超时
        timeout.tv_usec = 0;

        int select_result = select(0, &read_fds, NULL, NULL, &timeout);
        if (select_result > 0)
        {
            if (FD_ISSET(clientSock, &read_fds))
            {
                // 接收服务器响应
                bytes_received = recv(clientSock, buffer, BUFFER_SIZE - 1, 0);
                if (bytes_received > 0)
                {
                    buffer[bytes_received] = '\0';
                    set_console_color(10);
                    printf("服务器响应: %s\n", buffer);
                    set_console_color(15);
                    if (strstr(buffer, "RSA加密后的DES密钥") != NULL)
                    {
                        if (sscanf(buffer, "RSA加密后的DES密钥: %s", keytemp) == 1) // 从服务器响应中提取DES密钥
                        {
                            mpz_t encrypted_msg, decrypted_msg;
                            mpz_init(decrypted_msg);
                            mpz_init_set_str(encrypted_msg, keytemp, 16); // 将16进制字符串转换为大整数
                            // RSAdecrypt(decrypted_msg, encrypted_msg, d, n);
                            mpz_powm(decrypted_msg, encrypted_msg, d, n);
                            mpz_get_str(buffer, 16, decrypted_msg); // 将大整数转换为16进制字符串
                            mpz_clears(encrypted_msg, decrypted_msg, NULL);
                            set_console_color(10);
                            printf("RSA解密后的DES密钥: %s\n", buffer);
                            set_console_color(15);
                            for (int i = 0; i < 8; i++)
                            {
                                sscanf(&buffer[i * 2], "%2hhx", &tempkey[i]); // 两个字符转换为一个字节
                            }
                            BytesToBits((char *)tempkey, key); // 将字节转换为位
                            set_console_color(10);
                            printf("DES密钥: ");
                            for (int i = 0; i < 64; i++)
                            {
                                printf("%d", key[i]);
                            }
                            printf("\n");
                            set_console_color(15);
                            GenerateSubKeys(); // 生成子密钥
                            des_correct = 1;
                        }
                        else
                        {
                            set_console_color(12);
                            printf("des接收格式错误\n");
                            set_console_color(15);
                        }
                    }

                    break; // 成功接收到数据后退出循环
                }
                else if (bytes_received == 0)
                {
                    set_console_color(12);
                    printf("服务器关闭连接。\n");
                    set_console_color(15);
                    break;
                }
                else
                {
                    int error_code = WSAGetLastError();
                    if (error_code == WSAEWOULDBLOCK)
                    {
                        set_console_color(12);
                        printf("recv would block\n");
                        continue;
                    }
                    else
                    {
                        set_console_color(12);
                        printf("recv failed: %d\n", error_code);
                        break;
                    }
                }
            }
        }
        else if (select_result == 0)
        {
            printf("等待超时...\n");
            continue;
        }
        else
        {
            printf("select failed: %d\n", WSAGetLastError());
            break;
        }
    }
}
// DES的各种函数/定义结束

void register_user()
{
    char username[50], password[50], buffer[BUFFER_SIZE], encrypted[BUFFER_SIZE], decrypted[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    memset(encrypted, 0, BUFFER_SIZE);
    memset(decrypted, 0, BUFFER_SIZE);
    fd_set read_fds;
    struct timeval timeout;
    int bytes_received;
    set_console_color(14); // 提示信息颜色
    printf("请输入用户名: ");
    scanf("%s", username);
    printf("请输入密码: ");
    scanf("%s", password);
    set_console_color(15); // 恢复默认颜色
    sprintf(buffer, "REGISTER %s %s", username, password);
    encryptmsg(buffer, encrypted);
    send(client_socket, encrypted, strlen(encrypted) + 512, 0);
    memset(buffer, 0, BUFFER_SIZE);
    memset(encrypted, 0, BUFFER_SIZE);

    while (1)
    {
        // 清空接收缓冲区
        memset(buffer, 0, BUFFER_SIZE);

        // 设置文件描述符集
        FD_ZERO(&read_fds);
        FD_SET(client_socket, &read_fds);

        // 设置超时时间
        timeout.tv_sec = 5; // 5秒超时
        timeout.tv_usec = 0;

        int select_result = select(0, &read_fds, NULL, NULL, &timeout);
        if (select_result > 0)
        {
            if (FD_ISSET(client_socket, &read_fds))
            {
                // 接收服务器响应
                bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
                if (bytes_received > 0)
                {
                    decryptmsg(buffer, decrypted);

                    set_console_color(10); // 正常信息颜色
                    printf("服务器响应: %s\n", decrypted);
                    set_console_color(15); // 恢复默认颜色
                    break;                 // 成功接收到数据后退出循环
                }
                else if (bytes_received == 0)
                {
                    set_console_color(12); // 错误信息颜色
                    printf("服务器关闭连接。\n");
                    set_console_color(15); // 恢复默认颜色
                    break;
                }
                else
                {
                    int error_code = WSAGetLastError();
                    if (error_code == WSAEWOULDBLOCK)
                    {
                        set_console_color(12); // 错误信息颜色
                        printf("recv would block\n");
                        set_console_color(15); // 恢复默认颜色
                        continue;
                    }
                    else
                    {
                        set_console_color(12); // 错误信息颜色
                        printf("recv failed: %d\n", error_code);
                        set_console_color(15); // 恢复默认颜色
                        break;
                    }
                }
            }
        }
        else if (select_result == 0)
        {
            set_console_color(12); // 错误信息颜色
            printf("等待超时...\n");
            set_console_color(15); // 恢复默认颜色
            continue;
        }
        else
        {
            set_console_color(12); // 错误信息颜色
            printf("select failed: %d\n", WSAGetLastError());
            set_console_color(15); // 恢复默认颜色
            break;
        }
    }
    return;
}

int login_user()
{
    char password[50], buffer[BUFFER_SIZE], encrypted[BUFFER_SIZE], decrypted[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    memset(encrypted, 0, BUFFER_SIZE);
    memset(decrypted, 0, BUFFER_SIZE);
    set_console_color(14); // 提示信息颜色
    printf("请输入用户名: ");
    scanf("%s", username);
    printf("请输入密码: ");
    scanf("%s", password);
    set_console_color(15); // 恢复默认颜色
    sprintf(buffer, "LOGIN %s %s", username, password);
    encryptmsg(buffer, encrypted);
    send(client_socket, encrypted, strlen(encrypted) + 512, 0);
    Sleep(500);
    memset(buffer, 0, BUFFER_SIZE);
    memset(encrypted, 0, BUFFER_SIZE);

    Sleep(1000);
    if (is_logged_in == 1)
    {
        set_console_color(10); // 正常信息颜色
        printf("登录成功！\n");
        set_console_color(15); // 恢复默认颜色
        return 1;
    }

    return 0;
}

void send_message()
{
    char message[BUFFER_SIZE], encrypted[BUFFER_SIZE], decrypted[BUFFER_SIZE];
    int count = 0;
    memset(message, 0, BUFFER_SIZE);
    memset(encrypted, 0, BUFFER_SIZE);
    memset(decrypted, 0, BUFFER_SIZE);
    fd_set readfds;
    struct timeval tv;
    int bytes_received;

    while (true)
    {
        set_console_color(14); // 提示信息颜色
        if (count > 0)
        {
            printf("请输入消息: ");
        }
        // scanf("%s", message);
        fgets(message, sizeof(message), stdin);
        set_console_color(15); // 恢复默认颜色
        if (strncmp(message, "exit", 4) == 0)
        {
            set_console_color(10); // 正常信息颜色
            printf("退出聊天。\n");
            set_console_color(15); // 恢复默认颜色
            memset(message, 0, BUFFER_SIZE);
            break;
        }

        encryptmsg(message, encrypted);
        send(client_socket, encrypted, strlen(encrypted) + 512, 0);
        memset(message, 0, BUFFER_SIZE);
        memset(encrypted, 0, BUFFER_SIZE);

        while (true)
        {
            memset(message, 0, BUFFER_SIZE);
            FD_ZERO(&readfds);
            FD_SET(client_socket, &readfds);
            tv.tv_sec = 5;
            tv.tv_usec = 0;

            int select_result = select(0, &readfds, NULL, NULL, &tv);
            if (select_result > 0)
            {
                if (FD_ISSET(client_socket, &readfds))
                {
                    bytes_received = recv(client_socket, message, BUFFER_SIZE - 1, 0);
                    if (bytes_received < 0)
                    {
                        int error_code = WSAGetLastError();
                        if (error_code == WSAETIMEDOUT)
                        {
                            continue;
                        }
                        else if (error_code == WSAEWOULDBLOCK)
                        {
                            Sleep(100);
                            continue;
                        }
                        else
                        {
                            set_console_color(12); // 错误信息颜色
                            printf("recv failed: %d\n", error_code);
                            set_console_color(15); // 恢复默认颜色
                            break;
                        }
                    }
                    else if (bytes_received == 0)
                    {
                        set_console_color(12); // 错误信息颜色
                        printf("服务器关闭连接。\n");
                        set_console_color(15); // 恢复默认颜色
                        break;
                    }
                    else
                    {
                        message[bytes_received] = '\0';
                        decryptmsg(message, decrypted);

                        set_console_color(10); // 正常信息颜色

                        if (count == 0)
                        {
                            count++;
                            break;
                        }
                        printf("服务器响应: %s\n", decrypted);
                        count++;
                        set_console_color(15); // 恢复默认颜色
                        break;
                    }
                }
            }
            else if (select_result == 0)
            {
                continue;
            }
            else
            {
                set_console_color(12); // 错误信息颜色
                printf("select failed: %d\n", WSAGetLastError());
                set_console_color(15); // 恢复默认颜色
                break;
            }
        }
    }
}

DWORD WINAPI send_group_messages(LPVOID lpParam)
{
    int group_id = (intptr_t)lpParam;
    char input[BUFFER_SIZE], encrypted[BUFFER_SIZE], decrypted[BUFFER_SIZE];
    int input_len = 0;

    while (!exit_group_chat) // 修改循环条件
    {
        // 检查是否有键盘输入
        if (_kbhit())
        {
            int ch = _getch();
            if (ch == '\r')
            {                            // 检测到回车键
                printf("\n");            // 换行
                input[input_len] = '\0'; // 结束字符串
                char group_msg[BUFFER_SIZE];
                memset(group_msg, 0, BUFFER_SIZE);
                sprintf(group_msg, "GROUP_MSG %d %s %s", group_id, username, input);

                if (strncmp(input, "exit", 4) == 0)
                {
                    memset(encrypted, 0, BUFFER_SIZE);
                    encryptmsg(group_msg, encrypted);
                    send(client_socket, encrypted, strlen(encrypted) + 512, 0);
                    set_console_color(12); // 设置错误信息颜色
                    printf("退出群聊\n");
                    set_console_color(15); // 恢复正常颜色
                    exit_group_chat = 1;   // 设置标志变量
                    memset(input, 0, BUFFER_SIZE);
                    memset(group_msg, 0, BUFFER_SIZE);
                    memset(encrypted, 0, BUFFER_SIZE);
                    break;
                }
                else
                {
                    memset(encrypted, 0, BUFFER_SIZE);
                    encryptmsg(group_msg, encrypted);
                    send(client_socket, encrypted, strlen(encrypted) + 512, 0);
                    memset(input, 0, BUFFER_SIZE);
                    memset(group_msg, 0, BUFFER_SIZE);
                    memset(encrypted, 0, BUFFER_SIZE);
                }

                input_len = 0; // 重置输入缓冲区
            }
            else if (ch == '\b' && input_len > 0)
            { // 处理退格键
                input_len--;
                printf("\b \b"); // 删除字符
            }
            else if (ch >= 32 && ch <= 126)
            { // 处理可打印字符
                input[input_len++] = ch;
                printf("%c", ch); // 打印字符
            }
        }
    }
    return 0;
}

DWORD WINAPI receive_messages(LPVOID lpParam)
{
    char buffer[BUFFER_SIZE], encrypted[BUFFER_SIZE], decrypted[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    memset(encrypted, 0, BUFFER_SIZE);
    memset(decrypted, 0, BUFFER_SIZE);
    int bytes_received;

    while (!exit_group_chat) // 修改循环条件
    {
        bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received < 0)
        {
            int error_code = WSAGetLastError();
            if (error_code == WSAETIMEDOUT) // 超时错误
            {
                set_console_color(12); // 设置错误信息颜色
                printf("接收超时,未收到服务器响应。\n");
                set_console_color(15); // 恢复正常颜色
            }
            else if (error_code == WSAEWOULDBLOCK) // 非阻塞模式下没有数据可接收
            {
                Sleep(100); // 等待0.1秒
                continue;
            }
            else
            {
                set_console_color(12); // 设置错误信息颜色
                printf("recv failed: %d\n", error_code);
                set_console_color(15); // 恢复正常颜色
                break;                 // 跳出循环
            }
        }
        else if (bytes_received == 0)
        {
            set_console_color(12); // 设置错误信息颜色
            printf("服务器关闭连接。\n");
            set_console_color(15); // 恢复正常颜色
            break;                 // 跳出循环
        }
        else // 接收到数据
        {
            if (des_correct == 1)
            {
                memset(decrypted, 0, BUFFER_SIZE);
                decryptmsg(buffer, decrypted);
                set_console_color(10); // 设置正常信息颜色
                printf("[系统消息]执行解密\n");
                set_console_color(15); // 恢复正常颜色
            }

            if (strlen(decrypted) > 0 && is_logged_in == 0)
            {
                const char *status = "登录成功";
                const char *status2 = "登录失败";
                if (strstr(decrypted, status) != NULL)
                {
                    is_logged_in = 1;
                    set_console_color(10); // 设置正常信息颜色
                    printf("%d\n", is_logged_in);
                    printf("服务器响应2: %s\n", decrypted);
                    set_console_color(15); // 恢复正常颜色
                }
                else if (strstr(decrypted, status2) != NULL)
                {
                    set_console_color(12); // 设置错误信息颜色
                    printf("登录失败请重试。\n");
                    set_console_color(15); // 恢复正常颜色
                }
                else if (strstr(decrypted, "群聊") != NULL)
                {
                }
                else
                {
                    set_console_color(10); // 设置正常信息颜色
                    printf("服务器响应1: %s\n", decrypted);
                    set_console_color(15); // 恢复正常颜色
                }
            }
        }
    }

    return 0;
}

void create_group()
{
    char group_name[50], buffer[BUFFER_SIZE], encrypted[BUFFER_SIZE], decrypted[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    memset(encrypted, 0, BUFFER_SIZE);
    memset(decrypted, 0, BUFFER_SIZE);
    fd_set read_fds;
    struct timeval timeout;
    int bytes_received;
    printf("请输入群聊名称: ");
    scanf("%s", group_name);

    sprintf(buffer, "CREATE_GROUP %s %s", group_name, username);
    encryptmsg(buffer, encrypted);
    send(client_socket, encrypted, strlen(encrypted) + 512, 0);

    while (1)
    {
        // 清空接收缓冲区
        memset(buffer, 0, BUFFER_SIZE);

        // 设置文件描述符集
        FD_ZERO(&read_fds);
        FD_SET(client_socket, &read_fds);

        // 设置超时时间
        timeout.tv_sec = 5; // 5秒超时
        timeout.tv_usec = 0;

        int select_result = select(0, &read_fds, NULL, NULL, &timeout);
        if (select_result > 0)
        {
            if (FD_ISSET(client_socket, &read_fds))
            {
                // 接收服务器响应
                bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
                if (bytes_received > 0)
                {
                    buffer[bytes_received] = '\0';
                    decryptmsg(buffer, decrypted);
                    set_console_color(10); // 设置正常信息颜色
                    printf("服务器响应: %s\n", decrypted);
                    set_console_color(15); // 恢复正常颜色
                    break;                 // 成功接收到数据后退出循环
                }
                else if (bytes_received == 0)
                {
                    set_console_color(12); // 设置错误信息颜色
                    printf("服务器关闭连接。\n");
                    set_console_color(15); // 恢复正常颜色
                    break;
                }
                else
                {
                    int error_code = WSAGetLastError();
                    if (error_code == WSAEWOULDBLOCK)
                    {
                        set_console_color(12); // 设置错误信息颜色
                        printf("recv would block\n");
                        set_console_color(15); // 恢复正常颜色
                        continue;
                    }
                    else
                    {
                        set_console_color(12); // 设置错误信息颜色
                        printf("recv failed: %d\n", error_code);
                        set_console_color(15); // 恢复正常颜色
                        break;
                    }
                }
            }
        }
        else if (select_result == 0)
        {
            set_console_color(12); // 设置错误信息颜色
            printf("等待超时...\n");
            set_console_color(15); // 恢复正常颜色
            continue;
        }
        else
        {
            set_console_color(12); // 设置错误信息颜色
            printf("select failed: %d\n", WSAGetLastError());
            set_console_color(15); // 恢复正常颜色
            break;
        }
    }
}

void list_groups()
{
    char buffer[BUFFER_SIZE], decrypted[BUFFER_SIZE], encrypted[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    memset(decrypted, 0, BUFFER_SIZE);
    memset(encrypted, 0, BUFFER_SIZE);
    fd_set read_fds;
    struct timeval timeout;
    int bytes_received;

    // 发送请求
    sprintf(buffer, "LIST_GROUPS");
    encryptmsg(buffer, encrypted);
    send(client_socket, encrypted, strlen(encrypted) + 512, 0);

    set_console_color(10); // 设置正常信息颜色
    printf("等待服务器响应...\n");
    set_console_color(15); // 恢复正常颜色

    while (1)
    {
        // 清空接收缓冲区
        memset(buffer, 0, BUFFER_SIZE);

        // 设置文件描述符集
        FD_ZERO(&read_fds);
        FD_SET(client_socket, &read_fds);

        // 设置超时时间
        timeout.tv_sec = 5; // 5秒超时
        timeout.tv_usec = 0;

        int select_result = select(0, &read_fds, NULL, NULL, &timeout);
        if (select_result > 0)
        {
            if (FD_ISSET(client_socket, &read_fds))
            {
                // 接收服务器响应
                bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
                if (bytes_received > 0)
                {
                    decryptmsg(buffer, decrypted);

                    set_console_color(10); // 设置正常信息颜色
                    printf("服务器响应: %s\n", decrypted);
                    set_console_color(15); // 恢复正常颜色
                    break;                 // 成功接收到数据后退出循环
                }
                else if (bytes_received == 0)
                {
                    set_console_color(12); // 设置错误信息颜色
                    printf("服务器关闭连接。\n");
                    set_console_color(15); // 恢复正常颜色
                    break;
                }
                else
                {
                    int error_code = WSAGetLastError();
                    if (error_code == WSAEWOULDBLOCK)
                    {
                        set_console_color(12); // 设置错误信息颜色
                        printf("recv would block\n");
                        set_console_color(15); // 恢复正常颜色
                        continue;
                    }
                    else
                    {
                        set_console_color(12); // 设置错误信息颜色
                        printf("recv failed: %d\n", error_code);
                        set_console_color(15); // 恢复正常颜色
                        break;
                    }
                }
            }
        }
        else if (select_result == 0)
        {
            set_console_color(12); // 设置错误信息颜色
            printf("等待超时...\n");
            set_console_color(15); // 恢复正常颜色
            continue;
        }
        else
        {
            set_console_color(12); // 设置错误信息颜色
            printf("select failed: %d\n", WSAGetLastError());
            set_console_color(15); // 恢复正常颜色
            break;
        }
    }
}

void enter_group()
{
    int group_id, timetolive = 0;
    char buffer[BUFFER_SIZE], encrypted[BUFFER_SIZE], decrypted[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    memset(encrypted, 0, BUFFER_SIZE);
    memset(decrypted, 0, BUFFER_SIZE);
    fd_set read_fds;
    struct timeval timeout;
    int bytes_received;
    bool received_response = false;

    printf("请输入群聊ID: ");
    scanf("%d", &group_id);

    sprintf(buffer, "ENTER_GROUP %d %s", group_id, username);
    encryptmsg(buffer, encrypted);
    send(client_socket, encrypted, strlen(encrypted) + 512, 0);

    // 等待服务器的响应
    while (!received_response)
    {
        FD_ZERO(&read_fds);
        FD_SET(client_socket, &read_fds);
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;

        int activity = select(0, &read_fds, NULL, NULL, &timeout);

        if (activity < 0)
        {
            set_console_color(12); // 设置错误信息颜色
            printf("select 出错\n");
            set_console_color(15); // 恢复正常颜色
            return;
        }
        else if (activity == 0)
        {
            set_console_color(12); // 设置错误信息颜色
            printf("等待服务器响应超时,正在重试...\n");
            set_console_color(15); // 恢复正常颜色
            timetolive++;
            if (timetolive >= 3)
            {
                set_console_color(12); // 设置错误信息颜色
                printf("等待服务器响应超时,退出群聊\n");
                set_console_color(15); // 恢复正常颜色
                return;
            }
            continue;
        }

        if (FD_ISSET(client_socket, &read_fds))
        {
            memset(buffer, 0, BUFFER_SIZE);
            memset(decrypted, 0, BUFFER_SIZE);
            bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
            if (bytes_received <= 0)
            {
                if (WSAGetLastError() == WSAEWOULDBLOCK)
                {
                    Sleep(100);
                    continue;
                }
                set_console_color(12); // 设置错误信息颜色
                printf("接收消息出错或连接已关闭\n");
                set_console_color(15); // 恢复正常颜色
                return;
            }
            decryptmsg(buffer, decrypted);

            // 检查是否包含成功或失败的消息
            if (strstr(decrypted, "成功") != NULL)
            {
                set_console_color(10); // 设置正常信息颜色
                printf("进入群聊成功\n");
                set_console_color(15); // 恢复正常颜色
                received_response = true;
            }
            else if (strstr(decrypted, "失败") != NULL)
            {
                set_console_color(12); // 设置错误信息颜色
                printf("进入群聊失败\n");
                set_console_color(15); // 恢复正常颜色
                return;
            }
        }
    }

    // 重置群聊状态
    exit_group_chat = 0;

    // 创建发送消息的线程
    HANDLE send_thread = CreateThread(NULL, 0, send_group_messages, (LPVOID)(intptr_t)group_id, 0, NULL);

    // 群聊消息接收循环
    while (!exit_group_chat)
    {
        memset(buffer, 0, BUFFER_SIZE);
        memset(decrypted, 0, BUFFER_SIZE);
        FD_ZERO(&read_fds);
        FD_SET(client_socket, &read_fds);
        timeout.tv_sec = 1; // 减少超时时间，使响应更及时
        timeout.tv_usec = 0;

        int activity = select(0, &read_fds, NULL, NULL, &timeout);

        if (activity < 0)
        {
            set_console_color(12); // 设置错误信息颜色
            printf("select 出错\n");
            set_console_color(15); // 恢复正常颜色
            break;
        }

        if (activity > 0 && FD_ISSET(client_socket, &read_fds))
        {
            bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);

            if (bytes_received <= 0)
            {
                if (WSAGetLastError() == WSAEWOULDBLOCK)
                {
                    Sleep(100);
                    continue;
                }
                set_console_color(12); // 设置错误信息颜色
                printf("接收消息出错或连接已关闭\n");
                set_console_color(15); // 恢复正常颜色
                break;
            }
            memset(decrypted, 0, BUFFER_SIZE);
            buffer[bytes_received] = '\0';
            decryptmsg(buffer, decrypted);

            set_console_color(10); // 设置正常信息颜色
            printf("群聊[%d]: %s\n", group_id, decrypted);
            set_console_color(15); // 恢复正常颜色
        }
    }

    // 等待发送线程结束
    WaitForSingleObject(send_thread, INFINITE);
    CloseHandle(send_thread);

    // 重置状态
    exit_group_chat = 0;
}

void delete_group()
{
    int group_id;
    char buffer[BUFFER_SIZE], encrypted[BUFFER_SIZE], decrypted[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    memset(encrypted, 0, BUFFER_SIZE);
    memset(decrypted, 0, BUFFER_SIZE);
    fd_set read_fds;
    struct timeval timeout;
    int bytes_received;

    printf("请输入要删除的群聊ID: ");
    scanf("%d", &group_id);

    sprintf(buffer, "DELETE_GROUP %d %s", group_id, username);
    encryptmsg(buffer, encrypted);
    send(client_socket, encrypted, strlen(encrypted) + 512, 0);

    while (1)
    {
        // 清空接收缓冲区
        memset(buffer, 0, BUFFER_SIZE);

        // 设置文件描述符集
        FD_ZERO(&read_fds);
        FD_SET(client_socket, &read_fds);

        // 设置超时时间
        timeout.tv_sec = 5; // 5秒超时
        timeout.tv_usec = 0;

        int select_result = select(0, &read_fds, NULL, NULL, &timeout);
        if (select_result > 0)
        {
            if (FD_ISSET(client_socket, &read_fds))
            {
                // 接收服务器响应
                bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
                if (bytes_received > 0)
                {
                    buffer[bytes_received] = '\0';
                    decryptmsg(buffer, decrypted);

                    set_console_color(10); // 设置正常信息颜色
                    printf("服务器响应: %s\n", decrypted);
                    set_console_color(15); // 恢复正常颜色
                    break;                 // 成功接收到数据后退出循环
                }
                else if (bytes_received == 0)
                {
                    set_console_color(12); // 设置错误信息颜色
                    printf("服务器关闭连接。\n");
                    set_console_color(15); // 恢复正常颜色
                    break;
                }
                else
                {
                    int error_code = WSAGetLastError();
                    if (error_code == WSAEWOULDBLOCK)
                    {
                        set_console_color(12); // 设置错误信息颜色
                        printf("recv would block\n");
                        set_console_color(15); // 恢复正常颜色
                        continue;
                    }
                    else
                    {
                        set_console_color(12); // 设置错误信息颜色
                        printf("recv failed: %d\n", error_code);
                        set_console_color(15); // 恢复正常颜色
                        break;
                    }
                }
            }
        }
        else if (select_result == 0)
        {
            set_console_color(12); // 设置错误信息颜色
            printf("等待超时...\n");
            set_console_color(15); // 恢复正常颜色
            continue;
        }
        else
        {
            set_console_color(12); // 设置错误信息颜色
            printf("select failed: %d\n", WSAGetLastError());
            set_console_color(15); // 恢复正常颜色
            break;
        }
    }
}

void display_group_menu()
{
    int choice;
    while (1)
    {
        set_console_color(11); // Light Cyan
        printf("╔═════════════════════════╗\n");
        printf("║       群聊菜单          ║\n");
        printf("╠═════════════════════════╣\n");
        printf("║ 1. 创建群聊             ║\n");
        printf("║ 2. 查看群聊列表         ║\n");
        printf("║ 3. 加入群聊             ║\n");
        printf("║ 4. 删除群聊             ║\n");
        printf("║ 5. 返回主菜单           ║\n");
        printf("╚═════════════════════════╝\n");
        set_console_color(15); // White
        printf("请选择: ");
        scanf("%d", &choice);

        switch (choice)
        {
        case 1:
            create_group();
            break;
        case 2:
            list_groups();
            break;
        case 3:
            enter_group();
            break;
        case 4:
            delete_group();
            break;
        case 5:
            return;
        default:
            set_console_color(12); // Light Red
            printf("无效选择,请重新选择。\n");
            set_console_color(15); // White
        }
    }
}

void display_main_menu()
{
    HANDLE recv_thread = CreateThread(NULL, 0, receive_messages, NULL, 0, NULL);
    int choice;
    Sleep(1000);

    while (1)
    {
        set_console_color(14); // Yellow
        printf("╔═════════════════════════╗\n");
        printf("║        主菜单           ║\n");
        printf("╠═════════════════════════╣\n");
        if (rsa_correct == 1 && des_correct == 1)
        {
            printf("║ 1. 注册                 ║\n");
            printf("║ 2. 登录                 ║\n");
        }
        if (is_logged_in == 0)
        {
            printf("║ 3. RSA密钥对的生成和发送║\n");
            if (rsa_correct == 1)
            {
                printf("║ 4. 申请DES密钥          ║\n");
            }
        }

        if (is_logged_in == 1 && rsa_correct == 1 && des_correct == 1)
        {
            printf("║ 5. 发送消息             ║\n");
            printf("║ 6. 群聊                 ║\n");
        }

        printf("║ 7. 退出                 ║\n");
        printf("╚═════════════════════════╝\n");
        set_console_color(15); // White
        printf("请选择: ");
        scanf("%d", &choice);

        switch (choice)
        {
        case 1:
            if (rsa_correct == 1 && des_correct == 1)
            {
                register_user();
            }
            else
            {
                set_console_color(12); // Light Red
                printf("RSA密钥对未发送,DES密钥未获取,无法注册\n");
                set_console_color(15); // White
            }
            break;
        case 2:
            if (is_logged_in == 0 && rsa_correct == 1 && des_correct == 1)
            {
                login_user();
            }
            else
            {
                set_console_color(12); // Light Red
                printf("您已登录或RSA密钥对未发送,DES密钥未获取\n");
                set_console_color(15); // White
            }
            break;
        case 3:
            if (is_logged_in == 0)
            {
                generate_rsa_keypair_and_send(client_socket);
            }
            else
            {
                set_console_color(12); // Light Red
                printf("错误。\n");
                set_console_color(15); // White
            }
            break;
        case 4:
            if (is_logged_in == 0)
            {
                if (rsa_correct == 1)
                {
                    set_console_color(10); // Light Green
                    printf("RSA密钥对已生成并发送,进入申请DES密钥功能\n");
                    set_console_color(15); // White
                    // receive_des_key(client_socket);
                    receive_and_decrypt_des_key(client_socket);
                }
                else
                {
                    set_console_color(12); // Light Red
                    printf("请先进行RSA密钥对的生成和发送。\n");
                    set_console_color(15); // White
                }
            }
            else
            {
                set_console_color(12); // Light Red
                printf("请先登录。\n");
                set_console_color(15); // White
            }
            break;
        case 5:
            if (is_logged_in == 1 && rsa_correct == 1 && des_correct == 1)
            {
                set_console_color(10); // Light Green
                printf("进入发送消息功能\n");
                set_console_color(15); // White
                send_message();
            }
            else
            {
                set_console_color(12); // Light Red
                printf("请先生成RSA密钥对生成和发送并申请DES密钥或者登录。\n");
                set_console_color(15); // White
            }
            break;
        case 6:
            if (is_logged_in == 1 && rsa_correct == 1 && des_correct == 1)
            {
                set_console_color(10); // Light Green
                printf("进入群聊功能\n");
                set_console_color(15); // White
                display_group_menu();
            }
            else
            {
                set_console_color(12); // Light Red
                printf("请先生成RSA密钥对生成和发送并申请DES密钥或者登录。\n");
                set_console_color(15); // White
            }
            break;
        case 7:
            set_console_color(14); // Yellow
            printf("退出程序,请稍等...\n");
            Sleep(1000);
            printf("程序已退出。\n");
            set_console_color(15); // White
            return;
        default:
            set_console_color(12); // Light Red
            printf("无效选择,请重新选择。\n");
            set_console_color(15); // White
        }
    }
    WaitForSingleObject(recv_thread, INFINITE);
    CloseHandle(recv_thread);
    closesocket(client_socket);
    WSACleanup();
}

int main(int argc, char *argv[])
{
    WSADATA ws;
    WSAStartup(MAKEWORD(2, 2), &ws);
    client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    server_addr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");

    if (connect(client_socket, (sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR)
    {
        printf("连接服务器失败: %d\n", WSAGetLastError());
        return 1;
    }

    u_long mode = 1;
    ioctlsocket(client_socket, FIONBIO, &mode);

    printf("连接服务器成功\n");

    display_main_menu();

    closesocket(client_socket);
    WSACleanup();
    return 0;
}