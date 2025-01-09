#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <map>
#include <string.h>
#include <winsock2.h>
#include <windows.h>
#include <time.h>
#include <ws2tcpip.h>
#include "mysql/mysql.h"
#include <vector>
#include <algorithm>
#include "RSA.h"
#include "des.h"
#include <D:\msys\home\YYDS\gmp-6.2.0\gmp.h>

#pragma comment(lib, "ws2_32.lib")

#define BUFFER_SIZE 3072

struct clientinfo
{
    SOCKET clientSocket;
    char username[50];
    char rsa_n[1024];
    char rsa_e[1024];
    bool des_key[64];
};

std::map<SOCKET, clientinfo> clients;

MYSQL *conn;

void initialize_database()
{
    conn = mysql_init(NULL);
    if (conn == NULL)
    {
        fprintf(stderr, "mysql_init failed\n");
        exit(EXIT_FAILURE);
    }
    if (mysql_real_connect(conn, "127.0.0.1", "root", "gang", "chat_db", 0, NULL, 0) == NULL)
    {
        fprintf(stderr, "mysql_real_connect failed\n");
        mysql_close(conn);
        exit(EXIT_FAILURE);
    }
}

void close_database()
{
    mysql_close(conn);
}

// RSA的各种函数/定义起始

// RSA的各种函数/定义结束

// DES的各种函数/定义起始
// 生成64位随机密钥
void generateRandom_DESKey_and_send(SOCKET clientSock)
{
    char rsaenmsg[BUFFER_SIZE];
    char success_msg[BUFFER_SIZE];
    unsigned char des_key_str[8];
    srand(time(0)); // 设置随机数种子
    for (int i = 0; i < 8; i++)
    {
        des_key_str[i] = rand() % 256;
    }

    // 将DES密钥转换为十六进制字符串
    char des_key_hex[17]; // 16个字符 + 1个空字符
    memset(des_key_hex, 0, sizeof(des_key_hex));
    for (int i = 0; i < 8; i++)
    {
        sprintf(&des_key_hex[i * 2], "%02x", des_key_str[i]);
    }
    des_key_hex[16] = '\0'; // 确保字符串以 '\0' 结尾

    mpz_t rsa_e, rsa_n;
    mpz_init_set_str(rsa_e, clients[clientSock].rsa_e, 16);
    mpz_init_set_str(rsa_n, clients[clientSock].rsa_n, 16);
    BytesToBits((char *)des_key_str, clients[clientSock].des_key);
    /*sprintf(success_msg, "DES密钥生成成功: %s", des_key_hex);
    BytesToBits((char *)des_key_str, clients[clientSock].des_key);*/
    printf("DES字符串密钥生成成功: %s\n", des_key_hex);

    // 将success_msg转换为一个大整数
    mpz_t des_key_mpz;
    mpz_init(des_key_mpz);
    mpz_set_str(des_key_mpz, des_key_hex, 16);

    mpz_t rsaenkey_mpz;
    mpz_init(rsaenkey_mpz);
    printf("RSA加密DES密钥\n");
    RSAencrypt(rsaenkey_mpz, des_key_mpz, rsa_e, rsa_n);
    mpz_get_str(rsaenmsg, 16, rsaenkey_mpz); // 将大整数转换为16进制字符串
    mpz_clear(rsaenkey_mpz);
    mpz_clear(des_key_mpz);
    mpz_clear(rsa_e);
    mpz_clear(rsa_n);

    snprintf(success_msg, sizeof(success_msg), "RSA加密后的DES密钥: %s", rsaenmsg);
    send(clientSock, success_msg, strlen(success_msg), 0);
    printf("RSA加密后的DES密钥: %s\n", rsaenmsg);
    printf("客户端套接字: %llu, DES密钥: ", clientSock);
    for (int i = 0; i < 64; i++)
    {
        printf("%d", clients[clientSock].des_key[i]);
    }
    printf("\n");
}

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

// DES的各种函数/定义结束

void add_client_socket(SOCKET clientSock)
{
    if (clients.find(clientSock) != clients.end())
    {
        printf("客户端 %d 已经存在\n", clientSock);
        return;
    }
    printf("客户端 %d 连接成功\n", clientSock);
    clientinfo clientinfo;                // 创建一个clientinfo结构体
    clientinfo.clientSocket = clientSock; // 将客户端套接字赋值给结构体
    clients[clientSock] = clientinfo;
}

void remove_client_socket(int clientSock)
{
    printf("客户端 %d 断开连接\n", clientSock);
    closesocket(clientSock);
    // 删除结构体中的数据
    auto it = clients.find(clientSock);
    if (it != clients.end())
    {
        it->second.clientSocket = INVALID_SOCKET;
        memset(it->second.username, 0, sizeof(it->second.username));
        memset(it->second.rsa_n, 0, sizeof(it->second.rsa_n));
        memset(it->second.rsa_e, 0, sizeof(it->second.rsa_e));
        memset(it->second.des_key, 0, sizeof(it->second.des_key)); // 将DES密钥清零

        clients.erase(it);
    }
}

void send_to_all_clients(char *message)
{
    char enbuf[BUFFER_SIZE];
    for (auto it = clients.begin(); it != clients.end(); ++it) // 遍历所有客户端
    {
        memset(key, 0, sizeof(key));
        // 修改为对应套接字密钥
        for (int index = 0; index < 64; index++)
        {
            key[index] = it->second.des_key[index];
        }
        printf("客户端套接字: %llu, DES密钥: ", it->first);
        for (int j = 0; j < 64; j++)
        {
            printf("%d", key[j]);
        }
        GenerateSubKeys(); // 生成子密钥
        encryptmsg(message, enbuf);
        send(it->first, enbuf, strlen(enbuf), 0);
        memset(enbuf, 0, sizeof(enbuf));
        memset(key, 0, sizeof(key));
    }
}

int register_user(const char *username, const char *password)
{
    char query[256];
    memset(query, 0, sizeof(query));
    sprintf(query, "INSERT INTO users (username, password) VALUES ('%s', '%s')", username, password);
    if (mysql_query(conn, query))
    {
        fprintf(stderr, "用户注册失败: %s\n", mysql_error(conn));
        return 1;
    }
    else
    {
        printf("用户注册成功!\n");
        return 0;
    }
}

int authenticate_user(const char *username, const char *password)
{
    char query[256];
    memset(query, 0, sizeof(query));
    sprintf(query, "SELECT * FROM users WHERE username='%s' AND password='%s'", username, password);
    if (mysql_query(conn, query))
    {
        fprintf(stderr, "认证失败 %s\n", mysql_error(conn));
        return 0;
    }
    MYSQL_RES *result = mysql_store_result(conn);
    if (result == NULL)
    {
        fprintf(stderr, "mysql_store_result failed: %s\n", mysql_error(conn));
        return 0;
    }
    int rows = mysql_num_rows(result);
    mysql_free_result(result);
    return rows > 0;
}

int create_group(const char *group_name, const char *creator)
{
    char query[256];
    memset(query, 0, sizeof(query));
    sprintf(query, "INSERT INTO `groups` (name, creator) VALUES ('%s', '%s')", group_name, creator);
    if (mysql_query(conn, query))
    {
        fprintf(stderr, "创建群聊失败: %s\n", mysql_error(conn));
        printf("创建群聊失败!\n");
        return 1;
    }
    else
    {
        printf("群聊创建成功!\n");
        return 0;
    }
}

void list_groups(SOCKET client_sock)
{
    if (mysql_query(conn, "SELECT id, name FROM `groups` ORDER BY id"))
    {
        printf("查询群聊失败: %s\n", mysql_error(conn));
        return;
    }
    MYSQL_RES *result = mysql_store_result(conn);
    if (result == NULL)
    {
        printf("mysql_store_result failed: %s\n", mysql_error(conn));
        return;
    }
    // 如果为空
    if (mysql_num_rows(result) == 0)
    {
        char msg[] = "没有群聊";
        printf("%s\n", msg);
        encryptmsg(msg, msg);
        send(client_sock, msg, strlen(msg), 0);

        return;
    }

    MYSQL_ROW row;
    char buffer[BUFFER_SIZE];
    while ((row = mysql_fetch_row(result)))
    {
        sprintf(buffer, "群聊ID: %s, 群聊名称: %s\n", row[0], row[1]);
        printf("%s", buffer);
        encryptmsg(buffer, buffer);
        send(client_sock, buffer, strlen(buffer), 0);
        memset(buffer, 0, sizeof(buffer));
    }
    mysql_free_result(result);
}

int delete_group(SOCKET client_sock, char *group_id, const char *username)
{
    // 首先检查当前用户是否是群聊的创建者
    char failuremsg[BUFFER_SIZE];
    char successmsg[BUFFER_SIZE];
    char check_query[256];
    memset(check_query, 0, sizeof(check_query));
    memset(failuremsg, 0, sizeof(failuremsg));
    memset(successmsg, 0, sizeof(successmsg));
    sprintf(check_query, "SELECT * FROM `groups` WHERE id=%s AND creator='%s'", group_id, username);
    if (mysql_query(conn, check_query))
    {
        printf("删除群聊失败: %s\n", mysql_error(conn));
        strcpy(failuremsg, "删除群聊失败\n");
        encryptmsg(failuremsg, failuremsg);
        send(client_sock, failuremsg, strlen(failuremsg), 0);
        memset(failuremsg, 0, sizeof(failuremsg));
        return 1;
    }
    MYSQL_RES *result = mysql_store_result(conn);
    if (result == NULL) // 查询失败
    {
        printf("mysql_store_result failed: %s\n", mysql_error(conn));
        strcpy(failuremsg, "删除群聊失败\n");
        encryptmsg(failuremsg, failuremsg);
        send(client_sock, failuremsg, strlen(failuremsg), 0);
        memset(failuremsg, 0, sizeof(failuremsg));
        return 1;
    }

    int rows = mysql_num_rows(result);
    mysql_free_result(result);

    if (rows == 0)
    {
        printf("mysql_store_result failed: %s\n", mysql_error(conn));
        strcpy(failuremsg, "删除群聊失败: 用户不是该群聊的创建者\n");
        encryptmsg(failuremsg, failuremsg);
        send(client_sock, failuremsg, strlen(failuremsg), 0);
        memset(failuremsg, 0, sizeof(failuremsg));
        return 1;
    }

    // 如果是群聊的创建者，则执行删除操作
    char query[256];
    memset(query, 0, sizeof(query));
    sprintf(query, "DELETE FROM `groups` WHERE id=%s AND creator='%s'", group_id, username);
    if (mysql_query(conn, query))
    {
        printf("删除群聊失败: %s\n", mysql_error(conn));
        strcpy(failuremsg, "删除群聊失败: 用户不是该群聊的创建者\n");
        encryptmsg(failuremsg, failuremsg);
        send(client_sock, failuremsg, strlen(failuremsg), 0);
        memset(failuremsg, 0, sizeof(failuremsg));
        return 1;
    }
    else
    {
        strcpy(successmsg, "群聊删除成功\n");
        printf("%s", successmsg);
        encryptmsg(successmsg, successmsg);
        send(client_sock, successmsg, strlen(successmsg), 0);
        memset(successmsg, 0, sizeof(successmsg));
        return 0;
    }
}

int enter_group(SOCKET client_sock, const char *group_id, const char *username)
{
    char query[256];
    memset(query, 0, sizeof(query));
    sprintf(query, "INSERT INTO `group_users` (group_id, username) VALUES (%s, '%s')", group_id, username);
    if (mysql_query(conn, query))
    {
        printf("加入群聊失败: %s\n", mysql_error(conn));
        return 1;
    }
    return 0;
}

void initialize_socket(SOCKET &sListen, WSAEVENT &myevent, USHORT nPort, WSAEVENT eventArray[], SOCKET sockArray[], int &nEventTotal)
{
    WSADATA ws;
    WSAStartup(MAKEWORD(2, 2), &ws);
    sListen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(nPort);
    sin.sin_addr.S_un.S_addr = INADDR_ANY;
    bind(sListen, (struct sockaddr *)&sin, sizeof(struct sockaddr));
    listen(sListen, 5);

    myevent = WSACreateEvent();
    WSAEventSelect(sListen, myevent, FD_ACCEPT | FD_CLOSE);
    eventArray[nEventTotal] = myevent;
    sockArray[nEventTotal++] = sListen;
}

void handle_events(WSAEVENT eventArray[], SOCKET sockArray[], int &nEventTotal)
{
    while (TRUE)
    {
        int nIndex = WSAWaitForMultipleEvents(nEventTotal, eventArray, FALSE, WSA_INFINITE, FALSE) - WSA_WAIT_EVENT_0;
        for (int i = nIndex; i < nEventTotal; i++)
        {
            if (WSAWaitForMultipleEvents(1, &eventArray[i], TRUE, 1000, FALSE) != WSA_WAIT_TIMEOUT)
            {
                WSANETWORKEVENTS event1;
                WSAEnumNetworkEvents(sockArray[i], eventArray[i], &event1);

                if (event1.lNetworkEvents & FD_ACCEPT && event1.iErrorCode[FD_ACCEPT_BIT] == 0)
                {
                    if (nEventTotal > WSA_MAXIMUM_WAIT_EVENTS)
                        continue;
                    SOCKET sNew = accept(sockArray[i], NULL, NULL);
                    add_client_socket(sNew);
                    WSAEVENT newEvent = WSACreateEvent();
                    WSAEventSelect(sNew, newEvent, FD_READ | FD_CLOSE | FD_WRITE);
                    eventArray[nEventTotal] = newEvent;
                    sockArray[nEventTotal++] = sNew;
                    sockaddr_in clientAddr;
                    int addrLen = sizeof(clientAddr);
                    getpeername(sNew, (sockaddr *)&clientAddr, &addrLen);
                    printf("新客户端已连接[IP:%s, PORT:%d]\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));
                }
                else if (event1.lNetworkEvents & FD_READ && event1.iErrorCode[FD_READ_BIT] == 0)
                {
                    const char *register_cmd = "REGISTER";
                    const char *login_cmd = "LOGIN";
                    const char *create_group_cmd = "CREATE_GROUP";
                    const char *list_groups_cmd = "LIST_GROUPS";
                    const char *enter_group_cmd = "ENTER_GROUP";
                    const char *group_msg_cmd = "GROUP_MSG";
                    const char *delete_group_cmd = "DELETE_GROUP";
                    const char *rsa_cmd = "RSA_KEY";
                    const char *des_cmd = "DES_KEY";
                    char szText[BUFFER_SIZE], temptext[BUFFER_SIZE];
                    char enbuf[BUFFER_SIZE];
                    char debuf[BUFFER_SIZE];
                    char cipher[BUFFER_SIZE];
                    char des_key_str[65];
                    memset(szText, 0, sizeof(szText));
                    memset(temptext, 0, sizeof(temptext));
                    memset(enbuf, 0, sizeof(enbuf));
                    memset(debuf, 0, sizeof(debuf));
                    memset(cipher, 0, sizeof(cipher));
                    memset(des_key_str, 0, sizeof(des_key_str));
                    int nRecv = recv(sockArray[i], szText, sizeof(szText) - 1, 0);
                    if (nRecv > 0)
                    {
                        szText[nRecv] = '\0';
                        if (strncmp(szText, rsa_cmd, strlen(rsa_cmd)) == 0)
                        {
                            // 这里存储客户端发来的RSA公钥
                            char rsa_n[1024], rsa_e[1024];
                            if (sscanf(szText, "RSA_KEY %s %s", rsa_n, rsa_e) == 2)
                            {
                                strcpy(clients[sockArray[i]].rsa_n, rsa_n);
                                strcpy(clients[sockArray[i]].rsa_e, rsa_e);
                                char success_msg[256];
                                sprintf(success_msg, "[系统消息] 客户端 %d 的RSA公钥已存储\n", sockArray[i]);
                                send(sockArray[i], success_msg, strlen(success_msg), 0);
                                printf("%s", success_msg);
                                memset(success_msg, 0, sizeof(success_msg));
                            }
                        }
                        if (strcmp(szText, "DES_KEY") == 0)
                        {
                            printf("收到申请DES密钥请求\n");
                            generateRandom_DESKey_and_send(sockArray[i]);
                        }

                        if (strstr(szText, "无需解密") == NULL && strstr(szText, rsa_cmd) == NULL && strstr(szText, des_cmd) == NULL)
                        {
                            // 获取DES密钥
                            for (int index = 0; index < 64; index++)
                            {
                                key[index] = clients[sockArray[i]].des_key[index];
                            }
                            printf("客户端套接字:%d DES密钥:", sockArray[i]);
                            for (int j = 0; j < 64; j++)
                            {
                                printf("%d", key[j]);
                            }
                            printf("\n");
                            GenerateSubKeys(); // 生成子密钥
                            decryptmsg(szText, temptext);
                            memset(szText, 0, sizeof(szText));
                            strcpy(szText, temptext);
                            printf("[系统消息]执行解密:%s\n", szText);
                        }

                        if (strncmp(szText, register_cmd, strlen(register_cmd)) == 0)
                        {
                            char username[256], password[256];
                            if (sscanf(szText, "REGISTER %s %s", username, password) == 2)
                            {
                                if (register_user(username, password) == 0)
                                {
                                    char success_msg[] = "[系统消息] 注册成功!";
                                    encryptmsg(success_msg, enbuf);
                                    send(sockArray[i], enbuf, strlen(enbuf), 0);
                                    memset(enbuf, 0, sizeof(enbuf));
                                    memset(key, 0, sizeof(key));
                                }
                                else
                                {
                                    char failure_msg[] = "[系统消息] 注册失败!";
                                    encryptmsg(failure_msg, enbuf);
                                    send(sockArray[i], enbuf, strlen(enbuf), 0);
                                    memset(enbuf, 0, sizeof(enbuf));
                                    memset(key, 0, sizeof(key));
                                }
                            }
                            else
                            {
                                char error_msg[] = "[系统消息] 注册格式错误!";
                                encryptmsg(error_msg, enbuf);
                                send(sockArray[i], enbuf, strlen(enbuf), 0);
                                memset(enbuf, 0, sizeof(enbuf));
                                memset(key, 0, sizeof(key));
                            }
                        }

                        char username[256], password[256];
                        if (sscanf(szText, "LOGIN %s %s", username, password) == 2)
                        {
                            if (authenticate_user(username, password))
                            {
                                char success_msg[] = "[系统消息] 登录成功!";
                                encryptmsg(success_msg, enbuf);
                                send(sockArray[i], enbuf, strlen(enbuf), 0);
                                memset(enbuf, 0, sizeof(enbuf));
                                memset(key, 0, sizeof(key));
                                strcpy(clients[sockArray[i]].username, username);
                                printf("用户 %s 已登录\n", username);
                                printf("服务器套接字: %d\n", sockArray[i]);
                            }
                            else
                            {
                                char failure_msg[] = "[系统消息] 登录失败,用户名或密码错误!";
                                encryptmsg(failure_msg, enbuf);
                                send(sockArray[i], enbuf, strlen(enbuf), 0);
                                memset(enbuf, 0, sizeof(enbuf));
                                memset(key, 0, sizeof(key));
                                printf("登录失败,用户名或密码错误\n");
                            }
                        }

                        if (strncmp(szText, create_group_cmd, strlen(create_group_cmd)) == 0)
                        {
                            char group_name[256];
                            char creator[256];
                            if (sscanf(szText, "CREATE_GROUP %s %s", group_name, creator) == 2)
                            {
                                if (create_group(group_name, creator) == 0)
                                {
                                    char success_msg[] = "[系统消息] 群聊创建成功!";
                                    encryptmsg(success_msg, enbuf);
                                    send(sockArray[i], enbuf, strlen(enbuf), 0);
                                    memset(enbuf, 0, sizeof(enbuf));
                                    memset(key, 0, sizeof(key));
                                }
                                else
                                {
                                    char failure_msg[] = "[系统消息] 群聊创建失败!";
                                    encryptmsg(failure_msg, enbuf);
                                    send(sockArray[i], enbuf, strlen(enbuf), 0);
                                    memset(enbuf, 0, sizeof(enbuf));
                                    memset(key, 0, sizeof(key));
                                }
                            }
                            else
                            {
                                char error_msg[] = "[系统消息] 创建群聊格式错误!";
                                encryptmsg(error_msg, enbuf);
                                send(sockArray[i], enbuf, strlen(enbuf), 0);
                                memset(enbuf, 0, sizeof(enbuf));
                                memset(key, 0, sizeof(key));
                            }
                        }

                        if (strncmp(szText, list_groups_cmd, strlen(list_groups_cmd)) == 0)
                        {
                            list_groups(sockArray[i]);
                            memset(key, 0, sizeof(key));
                        }

                        if (strncmp(szText, delete_group_cmd, strlen(delete_group_cmd)) == 0)
                        {
                            char group_id[256];
                            char username[256];
                            if (sscanf(szText, "DELETE_GROUP %s %s", group_id, username) == 2)
                            {
                                delete_group(sockArray[i], group_id, username);
                                memset(key, 0, sizeof(key));
                            }
                            else
                            {
                                char error_msg[] = "[系统消息] 删除群聊格式错误!";
                                encryptmsg(error_msg, enbuf);
                                send(sockArray[i], enbuf, strlen(enbuf), 0);
                                memset(enbuf, 0, sizeof(enbuf));
                                memset(key, 0, sizeof(key));
                            }
                        }

                        if (strncmp(szText, enter_group_cmd, strlen(enter_group_cmd)) == 0)
                        {
                            char group_id[256];
                            char username[256];
                            if (sscanf(szText, "ENTER_GROUP %s %s", group_id, username) == 2)
                            {
                                int enterstatus = enter_group(sockArray[i], group_id, username);
                                if (enterstatus == 0)
                                {
                                    char success_msg[256];
                                    sprintf(success_msg, "[系统消息] 用户 %s 进入群聊ID: %s成功\n", username, group_id);
                                    send_to_all_clients(success_msg);
                                    printf("%s", success_msg);
                                    memset(success_msg, 0, sizeof(success_msg));
                                    memset(enbuf, 0, sizeof(enbuf));
                                    memset(key, 0, sizeof(key));
                                }
                                else
                                {
                                    char failure_msg[] = "[系统消息] 进入群聊失败!";
                                    encryptmsg(failure_msg, enbuf);
                                    send(sockArray[i], enbuf, strlen(enbuf), 0);
                                    memset(enbuf, 0, sizeof(enbuf));
                                    memset(key, 0, sizeof(key));
                                }
                            }
                            else
                            {
                                char error_msg[] = "[系统消息] 进入群聊格式错误!";
                                encryptmsg(error_msg, enbuf);
                                send(sockArray[i], enbuf, strlen(enbuf), 0);
                                memset(enbuf, 0, sizeof(enbuf));
                                memset(key, 0, sizeof(key));
                            }
                        }

                        const char *sensitive_words = "fuck";
                        if (strstr(szText, sensitive_words) != NULL)
                        {
                            char sensitive_msg[] = "[系统消息] 您发送的消息包含敏感词!";
                            encryptmsg(sensitive_msg, enbuf);
                            send(sockArray[i], enbuf, strlen(enbuf), 0);
                            memset(enbuf, 0, sizeof(enbuf));
                            memset(key, 0, sizeof(key));
                        }
                        else
                        {
                            sockaddr_in clientAddr;
                            int addrLen = sizeof(clientAddr);
                            getpeername(sockArray[i], (sockaddr *)&clientAddr, &addrLen);
                            printf("[客户端IP:%s PORT:%d]:%s\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port), szText);
                            int group_id;
                            char Gusername[256];
                            char message[BUFFER_SIZE];
                            if (sscanf(szText, "GROUP_MSG %d %s %[^\n]", &group_id, Gusername, message) == 3)
                            {
                                char query[256];
                                sprintf(query, "SELECT username FROM `group_users` WHERE group_id=%d", group_id);
                                if (mysql_query(conn, query))
                                {
                                    printf("查询群聊用户失败: %s\n", mysql_error(conn));
                                    continue;
                                }
                                MYSQL_RES *result = mysql_store_result(conn);
                                if (result == NULL)
                                {
                                    printf("mysql_store_result failed: %s\n", mysql_error(conn));
                                    continue;
                                }
                                MYSQL_ROW row;

                                if (strstr(message, "exit") != NULL)
                                {
                                    for (auto it = clients.begin(); it != clients.end(); ++it)
                                    {
                                        if (it->first == sockArray[i])
                                        {
                                            sprintf(query, "DELETE FROM `group_users` WHERE username='%s'", Gusername);
                                            printf("删除了用户：%s\n", Gusername);
                                            if (mysql_query(conn, query))
                                            {
                                                printf("删除群聊用户失败: %s\n", mysql_error(conn));
                                                continue;
                                            }
                                        }
                                    }
                                }

                                while ((row = mysql_fetch_row(result)))
                                {
                                    if (strcmp(row[0], Gusername) == 0)
                                    {
                                        for (auto it = clients.begin(); it != clients.end(); ++it) // 遍历所有客户端
                                        {
                                            if (it->first != sockArray[i]) // 不发送给自己
                                            {
                                                memset(key, 0, sizeof(key));
                                                // 修改为对应套接字密钥
                                                for (int index = 0; index < 64; index++)
                                                {
                                                    key[index] = it->second.des_key[index];
                                                }
                                                printf("客户端套接字: %llu, DES密钥: ", it->first);
                                                for (int j = 0; j < 64; j++)
                                                {
                                                    printf("%d", key[j]);
                                                }
                                                GenerateSubKeys(); // 生成子密钥
                                                char group_msg[BUFFER_SIZE];
                                                sprintf(group_msg, "[群聊][用户：%s]：%s", inet_ntoa(clientAddr.sin_addr), message);
                                                printf("群聊[%d]消息：%s\n", group_id, group_msg);
                                                encryptmsg(group_msg, enbuf);
                                                send(it->first, enbuf, strlen(enbuf), 0);
                                                memset(enbuf, 0, sizeof(enbuf));
                                                memset(key, 0, sizeof(key));
                                            }
                                        }
                                    }
                                }

                                mysql_free_result(result);
                            }
                            else
                            {
                                if (strncmp(szText, "LOGIN", 5) != 0 && strncmp(szText, "REGISTER", 8) != 0 && strncmp(szText, "CREATE_GROUP", 12) != 0 && strncmp(szText, "LIST_GROUPS", 11) != 0 && strncmp(szText, "ENTER_GROUP", 11) != 0 && strncmp(szText, "DELETE_GROUP", 12) != 0 && strncmp(szText, "GROUP_MSG", 9) != 0 && strncmp(szText, "RSA_KEY", 7) != 0 && strstr(szText, "DES_KEY") == NULL)
                                {
                                    printf("客户端套接字: %llu 未触发任何CMD|消息: %s\n", sockArray[i], szText);
                                    encryptmsg(szText, enbuf);
                                    send(sockArray[i], enbuf, strlen(enbuf), 0);
                                    memset(enbuf, 0, sizeof(enbuf));
                                    memset(key, 0, sizeof(key));
                                }
                            }
                        }
                    }
                    memset(key, 0, sizeof(key));
                }
                else if (event1.lNetworkEvents & FD_CLOSE && event1.iErrorCode[FD_CLOSE_BIT] == 0)
                {
                    closesocket(sockArray[i]);
                    remove_client_socket(sockArray[i]);
                    for (int j = i; j < nEventTotal - 1; j++)
                    {
                        sockArray[j] = sockArray[j + 1];
                        eventArray[j] = eventArray[j + 1];
                    }
                    nEventTotal--;
                }
                else if (event1.lNetworkEvents & FD_WRITE && event1.iErrorCode[FD_WRITE_BIT] == 0)
                {
                    char szText[BUFFER_SIZE] = "消息已被接收";
                    send(sockArray[i], szText, strlen(szText), 0);
                }
            }
        }
    }
}

void cleanup(WSAEVENT eventArray[], SOCKET sockArray[], int nEventTotal)
{
    for (int i = 0; i < nEventTotal; i++)
    {
        closesocket(sockArray[i]);
        WSACloseEvent(eventArray[i]);
    }
    WSACleanup();
}

int main(int argc, CHAR *argv[])
{
    WSAEVENT eventArray[WSA_MAXIMUM_WAIT_EVENTS];
    SOCKET sockArray[WSA_MAXIMUM_WAIT_EVENTS];
    int nEventTotal = 0;
    USHORT nPort = 8080;
    SOCKET sListen;
    WSAEVENT myevent;

    initialize_database();

    initialize_socket(sListen, myevent, nPort, eventArray, sockArray, nEventTotal);

    printf("服务器已启动,等待客户端连接...\n");

    handle_events(eventArray, sockArray, nEventTotal);
    cleanup(eventArray, sockArray, nEventTotal);

    return 0;
}