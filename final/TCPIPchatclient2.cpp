#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")

void menu()
{
    printf("==========================\n");
    printf("1. 注册\n");
    printf("2. 登录\n");
    printf("3. 发送消息\n");
    printf("4. 退出客户端\n");
    printf("==========================\n");
}

// 连接到服务器
SOCKET connect_to_server(const char *ip, int port)
{
    WSADATA ws;
    WSAStartup(MAKEWORD(2, 2), &ws); // 初始化Winsock库

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET)
    {
        printf("[错误] 无法创建Socket\n");
        exit(EXIT_FAILURE);
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.S_un.S_addr = inet_addr(ip);

    if (connect(sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        printf("[错误] 无法连接到服务器\n");
        closesocket(sock);
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    printf("[系统消息] 已成功连接到服务器\n");
    return sock;
}

// 注册
void register_user(SOCKET sock)
{
    char username[256], password[256], buffer[512];
    printf("请输入用户名: ");
    scanf("%s", username);
    printf("请输入密码: ");
    scanf("%s", password);

    snprintf(buffer, sizeof(buffer), "REGISTER %s %s", username, password);
    send(sock, buffer, strlen(buffer), 0);

    int nRecv = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (nRecv > 0)
    {
        buffer[nRecv] = '\0';
        printf("%s\n", buffer);
    }
    else
    {
        printf("[错误] 注册失败\n");
    }
}

// 登录
int login_user(SOCKET sock)
{
    char username[256], password[256], buffer[512];
    printf("请输入用户名: ");
    scanf("%s", username);
    printf("请输入密码: ");
    scanf("%s", password);

    snprintf(buffer, sizeof(buffer), "LOGIN %s %s", username, password);
    send(sock, buffer, strlen(buffer), 0);

    int nRecv = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (nRecv > 0)
    {
        buffer[nRecv] = '\0';
        printf("%s\n", buffer);
        if (strstr(buffer, "[系统消息] 登录成功！") != NULL)
        {
            printf("[系统消息] 登录成功\n");
            return 1; // 登录成功
        }
    }
    else
    {
        printf("[错误] 登录失败\n");
    }
    return 0; // 登录失败
}

// 发送消息
void send_message(SOCKET sock)
{
    char message[256], buffer[512];
    printf("请输入要发送的消息: ");
    getchar(); // 清空输入缓冲区
    fgets(message, sizeof(message), stdin);
    message[strcspn(message, "\n")] = '\0'; // 移除换行符

    send(sock, message, strlen(message), 0);

    int nRecv = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (nRecv > 0)
    {
        buffer[nRecv] = '\0';
        printf("服务器响应: %s\n", buffer);
    }
    else
    {
        printf("[错误] 消息发送失败\n");
    }
}

int main()
{
    const char *server_ip = "127.0.0.1"; // 服务器IP地址
    int server_port = 8080;              // 服务器端口号

    SOCKET sock = connect_to_server(server_ip, server_port);

    int logged_in = 0; // 登录状态
    while (1)
    {
        menu();
        int choice;
        printf("请选择操作: ");
        scanf("%d", &choice);

        switch (choice)
        {
        case 1:
            register_user(sock);
            break;
        case 2:
            logged_in = login_user(sock);
            break;
        case 3:
            if (logged_in)
            {
                send_message(sock);
            }
            else
            {
                printf("[系统消息] 请先登录\n");
            }
            break;
        case 4:
            printf("[系统消息] 退出客户端\n");
            closesocket(sock);
            WSACleanup();
            exit(EXIT_SUCCESS);
        default:
            printf("[系统消息] 无效的选项，请重新选择\n");
        }
    }

    return 0;
}
