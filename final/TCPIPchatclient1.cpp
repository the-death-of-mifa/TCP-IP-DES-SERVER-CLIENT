#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>
#include <conio.h>
#include <cerrno>

#pragma comment(lib, "ws2_32.lib")

#define BUFFER_SIZE 1024

SOCKET client_socket;

int is_logged_in = 0; // 全局变量，表示登录状态

void register_user()
{
    char username[50], password[50], buffer[BUFFER_SIZE];
    printf("请输入用户名: ");
    scanf("%s", username);
    printf("请输入密码: ");
    scanf("%s", password);

    sprintf(buffer, "REGISTER %s %s", username, password);
    send(client_socket, buffer, strlen(buffer), 0);

    recv(client_socket, buffer, BUFFER_SIZE, 0);
    printf("服务器响应: %s\n", buffer);
    return;
}

int login_user()
{
    char username[50], password[50], buffer[BUFFER_SIZE];

    printf("请输入用户名: ");
    scanf("%s", username);
    printf("请输入密码: ");
    scanf("%s", password);

    sprintf(buffer, "LOGIN %s %s", username, password);
    send(client_socket, buffer, strlen(buffer), 0);

    Sleep(1000); // 等待服务器响应
    if (is_logged_in == 1)
    {
        printf("登录成功！\n");
        return 1;
    }

    return 0;
}

void send_message()
{

    char message[BUFFER_SIZE];
    while (true)
    {
        printf("请输入消息: ");
        scanf("%s", message);
        /*fgets(message, BUFFER_SIZE, stdin);
        message[strcspn(message, "\n")] = '\0'; // 移除换行符*/

        if (strncmp(message, "exit", 4) == 0)
        {
            printf("退出聊天。\n");
            break;
        }

        send(client_socket, message, strlen(message), 0);

        int bytes_received = recv(client_socket, message, BUFFER_SIZE - 1, 0);
        if (bytes_received < 0)
        {
            int error_code = WSAGetLastError();
            printf("recv failed: %d\n", error_code);
            break;
        }
        else
        {
            message[bytes_received] = '\0';
            printf("服务器响应3: %s\n", message);
        }
    }
}

DWORD WINAPI receive_messages(LPVOID lpParam)
{
    char buffer[BUFFER_SIZE];
    int bytes_received;

    while (1)
    {
        bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received < 0)
        {
            int error_code = WSAGetLastError();
            if (error_code == WSAETIMEDOUT) // 超时错误
            {
                printf("接收超时，未收到服务器响应。\n");
            }
            else
            {
                printf("recv failed: %d\n", error_code);
                break; // 跳出循环
            }
        }
        else if (bytes_received == 0)
        {
            printf("服务器关闭连接。\n");
            break; // 跳出循环
        }
        else // 接收到数据
        {
            buffer[bytes_received] = '\0'; // 确保缓冲区以 '\0' 结尾
            if (strlen(buffer) > 0 && is_logged_in == 0)
            {

                const char *status = "登录成功";
                const char *status2 = "登录失败";
                if (strstr(buffer, status) != NULL)
                {
                    is_logged_in = 1;
                    printf("%d\n", is_logged_in);
                    printf("服务器响应2: %s\n", buffer);
                }
                else if (strstr(buffer, status2) != NULL)
                {
                    printf("登录失败请重试。\n");
                }
                else
                {
                    printf("服务器响应1: %s\n", buffer);
                }
            }
        }
    }

    return 0;
}

void client_menu()
{
    printf("\n=========== 聊天客户端 ===========\n");
    printf("1. 注册\n");
    printf("2. 登录\n");
    printf("3. 发送消息\n");
    printf("4. 退出\n");
    printf("================================\n");
}

void start_client(const char *server_ip, int port)
{
    WSADATA wsaData;
    struct sockaddr_in server_address;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        printf("WSAStartup failed\n");
        return;
    }

    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == INVALID_SOCKET)
    {
        printf("Socket creation failed\n");
        WSACleanup();
        return;
    }

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = inet_addr(server_ip);
    server_address.sin_port = htons(port);

    if (connect(client_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
    {
        printf("连接服务器失败！\n");
        closesocket(client_socket);
        WSACleanup();
        return;
    }

    printf("成功连接到服务器！\n");

    int running = 1;
    int choice = 0;
    HANDLE recv_thread = CreateThread(NULL, 0, receive_messages, NULL, 0, NULL);

    while (running)
    {
        client_menu();
        printf("请选择操作: ");
        scanf("%d", &choice);

        switch (choice)
        {
        case 1:
            register_user();
            break;
        case 2:
            login_user();
            break;
        case 3:
            if (is_logged_in == 0)
            {
                printf("请先登录。\n");
                break;
            }
            else
            {
                printf("已登录，可以发送消息。\n");
            }
            send_message();
            break;
        case 4:
            running = 0;
            printf("正在退出客户端...\n");
            break;
        default:
            printf("无效的选择，请重新输入！\n");
        }
    }
    WaitForSingleObject(recv_thread, INFINITE);
    CloseHandle(recv_thread);
    closesocket(client_socket);
    WSACleanup();
}

int main()
{
    const char *server_ip = "127.0.0.1"; // 根据需要修改为服务器IP
    int port = 8080;
    start_client(server_ip, port);
    return 0;
}
