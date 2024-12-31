// 基于SOCKET的聊天软件设计，基于C语言
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>
#include <time.h>
#include "des.h"         // DES相关函数的头文件
#include "mysql/mysql.h" // 需要安装MySQL开发库

#pragma comment(lib, "ws2_32.lib")

#define MAX_CLIENTS 100
#define BUFFER_SIZE 1024

typedef struct
{
    SOCKET socket;
    char username[50];
    char id[10];
    int logged_in; // 是否已登录 (0: 未登录, 1: 已登录)
    struct sockaddr_in address;
} Client;

Client clients[MAX_CLIENTS];
int client_count = 0;

int is_logged_in(int socket)
{
    for (int i = 0; i < client_count; ++i)
    {
        if (clients[i].socket == socket)
        {
            return clients[i].logged_in; // 返回登录状态
        }
    }
    return 0; // 未找到对应的客户端，默认为未登录
}

// 数据库连接
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

int register_user(const char *username, const char *password)
{
    char query[256];
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

void broadcast_message(const char *message, int exclude_index)
{

    for (int i = 0; i < client_count; ++i)
    {
        if (i != exclude_index)
        {
            if (!is_logged_in(clients[i].socket))
            {
                const char *error_message = "[系统消息] 您尚未登录，请先登录。\n";
                send(clients[i].socket, error_message, strlen(error_message), 0);
            }
            else
            {
                send(clients[i].socket, message, strlen(message), 0);
            }
        }
    }
}

void handle_client(int index)
{
    const char *register_cmd = "REGISTER";
    const char *login_cmd = "LOGIN";
    char buffer[BUFFER_SIZE];
    int bytes_received;

    while ((bytes_received = recv(clients[index].socket, buffer, BUFFER_SIZE, 0)) > 0)
    {
        buffer[bytes_received] = '\0';
        // 检查消息是否以 REGISTER 开头
        if (strncmp(buffer, register_cmd, strlen(register_cmd)) == 0)
        {
            char username[128], password[128];
            if (sscanf(buffer + strlen(register_cmd), " %127s %127s", username, password) == 2)
            {
                int reval = register_user(username, password);
                const char *response = reval == 0 ? "[系统消息] 注册成功！\n" : "[系统消息] 注册失败！\n";
                send(clients[index].socket, response, strlen(response), 0);
            }
            else
            {
                const char *error_message = "[系统消息] 注册格式错误。\n";
                send(clients[index].socket, error_message, strlen(error_message), 0);
            }
            continue; // 结束本次循环，跳过后续广播
        }

        char username[128], password[128];
        if (sscanf(buffer, "LOGIN %127s %127s", username, password) == 2)
        {
            const char *response;
            if (authenticate_user(username, password))
            {
                for (int i = 0; i < MAX_CLIENTS; i++)
                {
                    if (clients[i].socket == clients[index].socket)
                    {
                        clients[i].logged_in = 1;
                        strncpy(clients[i].username, username, sizeof(clients[i].username) - 1);
                        break;
                    }
                }
                response = "[系统消息] 登录成功！\n";
            }
            else
            {
                response = "[系统消息] 登录失败，用户名或密码错误。\n";
            }
            send(clients[index].socket, response, strlen(response), 0);
            continue; // 登录处理完毕后，跳过后续广播逻辑
        }

        // 敏感词过滤
        if (strstr(buffer, "FUCK") != NULL)
        {
            strcpy(buffer, "[系统消息] 消息包含敏感词，已被拦截！\n");
        }
        else
        {
            printf("客户端[%s:%d]: %s\n", inet_ntoa(clients[index].address.sin_addr), ntohs(clients[index].address.sin_port), buffer);
        }

        // 加密处理（DES）

        if (strncmp(buffer, register_cmd, strlen(register_cmd)) != 0 &&
            strncmp(buffer, "LOGIN", 5) != 0)
        {
            broadcast_message(buffer, index);
        }
    }

    closesocket(clients[index].socket);
    clients[index] = clients[--client_count];
}

void server_start(int port)
{
    WSADATA wsaData;
    SOCKET server_socket;
    struct sockaddr_in server_address, client_address;
    int client_address_size = sizeof(client_address);

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        printf("WSAStartup failed\n");
        return;
    }

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == INVALID_SOCKET)
    {
        printf("Socket creation failed\n");
        WSACleanup();
        return;
    }

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == SOCKET_ERROR)
    {
        printf("Bind failed\n");
        closesocket(server_socket);
        WSACleanup();
        return;
    }

    if (listen(server_socket, MAX_CLIENTS) == SOCKET_ERROR)
    {
        printf("Listen failed\n");
        closesocket(server_socket);
        WSACleanup();
        return;
    }

    printf("服务器已启动，等待客户端连接...\n");

    while (1)
    {
        SOCKET client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_address_size);
        if (client_socket == INVALID_SOCKET)
        {
            printf("Accept failed\n");
            continue;
        }

        printf("新客户端已连接: %s:%d\n", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));

        clients[client_count].socket = client_socket;
        clients[client_count].address = client_address;
        sprintf(clients[client_count].id, "C%d", client_count);
        client_count++;

        // 为每个客户端创建线程处理
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)handle_client, reinterpret_cast<LPVOID>(client_count - 1), 0, NULL);
    }

    closesocket(server_socket);
    WSACleanup();
}

// des加密分组处理函数
char *msg_en(char *a, u_char key_final[16][6], char *cipher)
{
    int len = strlen(a);           // 获取明文字符串的长度
    u_char temp[8], tempcipher[8]; // 定义临时存储空间，用于处理8字节的块
    int index = 0;                 // 初始化输出密文字符串的索引

    for (int i = 0; i < len; i += 8)
    {                                              // 以8字节（64位）块为单位处理
        memset(temp, 0, sizeof(temp));             // 清零临时存储空间
        memset(tempcipher, 0, sizeof(tempcipher)); // 清零临时密文存储空间

        for (int j = 0; j < 8; j++)
        {
            if (i + j < len)
            {                               // 确保不越界访问
                temp[j] = (u_char)a[i + j]; // 将明文字节复制到临时数组
            }
            else
            {
                temp[j] = 0; // 如果不是完整的块，则用零填充剩余的字节
            }
        }

        // 将8字节的块转换为位集，用于DES处理
        bool plainBits[64], cipherBits[64];
        BytesToBits((char *)temp, (bool *)plainBits); // 将字节转换为位

        // 对该块执行DES加密
        encrypt((bool *)plainBits, (bool *)cipherBits); // 加密过程

        // 将加密后的位集转换回字节
        BitsToBytes((bool *)cipherBits, (char *)tempcipher); // 将位转换为字节

        for (int j = 0; j < 8; j++)
        {
            cipher[index++] = tempcipher[j]; // 将加密后的字节复制到密文字符串
        }
    }

    cipher[index] = '\0'; // 在输出密文字符串末尾添加空字符，进行字符串结束符的标记
    return cipher;        // 返回加密后的密文字符串
}

int main()
{
    initialize_database();
    server_start(8080);
    close_database();
    return 0;
}
