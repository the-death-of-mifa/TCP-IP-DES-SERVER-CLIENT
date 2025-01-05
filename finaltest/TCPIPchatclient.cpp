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

int is_logged_in = 0;
char username[50];
int exit_group_chat = 0; // 新的标志变量

void register_user()
{
    char username[50], password[50], buffer[BUFFER_SIZE];
    fd_set read_fds;
    struct timeval timeout;
    int bytes_received;
    printf("请输入用户名: ");
    scanf("%s", username);
    printf("请输入密码: ");
    scanf("%s", password);

    sprintf(buffer, "REGISTER %s %s", username, password);
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
                    buffer[bytes_received] = '\0'; // 确保缓冲区以 '\0' 结尾
                    printf("服务器响应: %s\n", buffer);
                    break; // 成功接收到数据后退出循环
                }
                else if (bytes_received == 0)
                {
                    printf("服务器关闭连接。\n");
                    break;
                }
                else
                {
                    int error_code = WSAGetLastError();
                    if (error_code == WSAEWOULDBLOCK)
                    {
                        printf("recv would block\n");
                        continue;
                    }
                    else
                    {
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
    return;
}

int login_user()
{
    char password[50], buffer[BUFFER_SIZE];

    printf("请输入用户名: ");
    scanf("%s", username);
    printf("请输入密码: ");
    scanf("%s", password);

    sprintf(buffer, "LOGIN %s %s", username, password);
    send(client_socket, buffer, strlen(buffer), 0);

    Sleep(1000);
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
    fd_set readfds;
    struct timeval tv;
    int bytes_received;

    while (true)
    {
        printf("请输入消息: ");
        scanf("%s", message);

        if (strncmp(message, "exit", 4) == 0)
        {
            printf("退出聊天。\n");
            break;
        }

        send(client_socket, message, strlen(message), 0);

        while (true)
        {
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
                            printf("recv failed: %d\n", error_code);
                            break;
                        }
                    }
                    else if (bytes_received == 0)
                    {
                        printf("服务器关闭连接。\n");
                        break;
                    }
                    else
                    {
                        message[bytes_received] = '\0';
                        printf("服务器响应: %s\n", message);
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
                printf("select failed: %d\n", WSAGetLastError());
                break;
            }
        }
    }
}

DWORD WINAPI send_group_messages(LPVOID lpParam)
{
    int group_id = (intptr_t)lpParam;
    char input[BUFFER_SIZE];
    int input_len = 0;

    while (1) // 修改循环条件
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
                sprintf(group_msg, "GROUP_MSG %d %s", group_id, input);

                if (strncmp(input, "exit", 4) == 0)
                {
                    send(client_socket, group_msg, strlen(group_msg), 0);
                    printf("退出群聊\n");
                    break;
                }
                else
                {
                    send(client_socket, group_msg, strlen(group_msg), 0);
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
    char buffer[BUFFER_SIZE];
    int bytes_received;

    while (!exit_group_chat) // 修改循环条件
    {
        bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received < 0)
        {
            int error_code = WSAGetLastError();
            if (error_code == WSAETIMEDOUT) // 超时错误
            {
                printf("接收超时,未收到服务器响应。\n");
            }
            else if (error_code == WSAEWOULDBLOCK) // 非阻塞模式下没有数据可接收
            {
                Sleep(100); // 等待0.1秒
                continue;
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
                else if (strstr(buffer, "群聊") != NULL)
                {
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

void create_group()
{
    char group_name[50], buffer[BUFFER_SIZE];

    fd_set read_fds;
    struct timeval timeout;
    int bytes_received;
    printf("请输入群聊名称: ");
    scanf("%s", group_name);

    sprintf(buffer, "CREATE_GROUP %s %s", group_name, username);
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
                    buffer[bytes_received] = '\0'; // 确保缓冲区以 '\0' 结尾
                    printf("服务器响应: %s\n", buffer);
                    break; // 成功接收到数据后退出循环
                }
                else if (bytes_received == 0)
                {
                    printf("服务器关闭连接。\n");
                    break;
                }
                else
                {
                    int error_code = WSAGetLastError();
                    if (error_code == WSAEWOULDBLOCK)
                    {
                        printf("recv would block\n");
                        continue;
                    }
                    else
                    {
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

void list_groups()
{
    char buffer[BUFFER_SIZE];
    fd_set read_fds;
    struct timeval timeout;
    int bytes_received;

    // 发送请求
    sprintf(buffer, "LIST_GROUPS");
    send(client_socket, buffer, strlen(buffer), 0);

    printf("等待服务器响应...\n");

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
                    buffer[bytes_received] = '\0'; // 确保缓冲区以 '\0' 结尾
                    printf("服务器响应: %s\n", buffer);
                    break; // 成功接收到数据后退出循环
                }
                else if (bytes_received == 0)
                {
                    printf("服务器关闭连接。\n");
                    break;
                }
                else
                {
                    int error_code = WSAGetLastError();
                    if (error_code == WSAEWOULDBLOCK)
                    {
                        printf("recv would block\n");
                        continue;
                    }
                    else
                    {
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

void enter_group()
{
    int group_id;
    char buffer[BUFFER_SIZE];
    fd_set read_fds;
    struct timeval timeout;
    int bytes_received;

    printf("请输入群聊ID: ");
    scanf("%d", &group_id);

    sprintf(buffer, "ENTER_GROUP %d %s", group_id, username);
    send(client_socket, buffer, strlen(buffer), 0);

    while (1)
    {
        FD_ZERO(&read_fds);
        FD_SET(client_socket, &read_fds);
        timeout.tv_sec = 5; // 设置超时时间为 5 秒
        timeout.tv_usec = 0;

        int activity = select(client_socket + 1, &read_fds, NULL, NULL, &timeout);

        if (activity < 0)
        {
            printf("select 出错\n");
            break;
        }
        else if (activity == 0)
        {
            // 超时
            continue;
        }

        if (FD_ISSET(client_socket, &read_fds))
        {
            bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
            if (bytes_received <= 0)
            {
                printf("接收消息出错或连接已关闭\n");
                break;
            }

            buffer[bytes_received] = '\0';
            printf("服务端消息: %s\n", buffer);

            if (strstr(buffer, "成功") != NULL)
            {
                printf("进入群聊成功\n");
                break;
            }
            else
            {
                printf("进入群聊失败\n");
                return;
            }
        }
    }

    // 进入群聊后的消息处理

    HANDLE send_thread = CreateThread(NULL, 0, send_group_messages, (LPVOID)(intptr_t)group_id, 0, NULL);
    while (1) // 修改循环条件
    {
        FD_ZERO(&read_fds);
        FD_SET(client_socket, &read_fds);
        timeout.tv_sec = 5; // 设置超时时间为 5 秒
        timeout.tv_usec = 0;

        int activity = select(0, &read_fds, NULL, NULL, &timeout);

        if (activity < 0)
        {
            printf("select 出错\n");
            break;
        }

        if (FD_ISSET(client_socket, &read_fds))
        {
            bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
            if (bytes_received <= 0)
            {
                printf("接收消息出错或连接已关闭\n");
                break;
            }

            buffer[bytes_received] = '\0';
            printf("群聊[%d]: %s\n", group_id, buffer);
        }
    }

    WaitForSingleObject(send_thread, INFINITE);
    CloseHandle(send_thread);
}

void delete_group()
{
    int group_id;
    char buffer[BUFFER_SIZE];

    printf("请输入要删除的群聊ID: ");
    scanf("%d", &group_id);

    sprintf(buffer, "DELETE_GROUP %d %s", group_id, username);
    send(client_socket, buffer, strlen(buffer), 0);

    recv(client_socket, buffer, BUFFER_SIZE, 0);
    printf("服务器响应: %s\n", buffer);
}

void display_group_menu()
{
    int choice;
    while (1)
    {
        printf("群聊菜单:\n");
        printf("1. 创建群聊\n");
        printf("2. 查看群聊列表\n");
        printf("3. 加入群聊\n");
        printf("4. 删除群聊\n");
        printf("5. 返回主菜单\n");
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
            printf("无效选择,请重新选择。\n");
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
        printf("主菜单:\n");
        printf("1. 注册\n");
        printf("2. 登录\n");
        printf("3. 发送消息\n");
        printf("4. 群聊功能\n");
        printf("5. 退出\n");
        printf("请选择: ");
        scanf("%d", &choice);

        switch (choice)
        {
        case 1:
            register_user();
            break;
        case 2:
            if (is_logged_in == 0)
            {
                login_user();
            }
            else
            {
                printf("您已登录,无需重复登录。\n");
            }
            break;
        case 3:
            if (is_logged_in == 1)
            {
                printf("您已登录,可以发送消息\n");
                send_message();
            }
            else
            {
                printf("请先登录。\n");
            }
            break;
        case 4:
            if (is_logged_in == 1)
            {
                printf("您已登录,进入群聊功能\n");
                display_group_menu();
            }
            else
            {
                printf("请先登录。\n");
            }
            break;
        case 5:
            printf("退出程序,请稍等...\n");
            Sleep(1000);
            printf("程序已退出。\n");
            return;
        default:
            printf("无效选择,请重新选择。\n");
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