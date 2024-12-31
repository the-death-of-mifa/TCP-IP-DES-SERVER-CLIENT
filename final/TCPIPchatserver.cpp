#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>
#include <time.h>
#include <ws2tcpip.h>
#include "mysql/mysql.h"

#pragma comment(lib, "ws2_32.lib")

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

// 初始化Socket
void InitializeSocket(SOCKET &sListen, WSAEVENT &myevent, USHORT nPort, WSAEVENT eventArray[], SOCKET sockArray[], int &nEventTotal)
{
    // 创建并绑定Socket
    WSADATA ws;
    WSAStartup(MAKEWORD(2, 2), &ws);                     // 初始化Winsock库
    sListen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); // 创建监听Socket
    sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(nPort);                                     // 设置端口号
    sin.sin_addr.S_un.S_addr = INADDR_ANY;                           // 监听所有IP地址
    bind(sListen, (struct sockaddr *)&sin, sizeof(struct sockaddr)); // 绑定Socket
    listen(sListen, 5);                                              // 开始监听

    // 创建事件并注册网络事件
    myevent = WSACreateEvent();                             // 创建一个新的事件对象
    WSAEventSelect(sListen, myevent, FD_ACCEPT | FD_CLOSE); // 将事件与Socket关联，并指定感兴趣的网络事件
    eventArray[nEventTotal] = myevent;                      // 将事件对象存储到事件数组中
    sockArray[nEventTotal++] = sListen;                     // 将Socket存储到Socket数组中
}

// 处理网络事件
void HandleEvents(WSAEVENT eventArray[], SOCKET sockArray[], int &nEventTotal)
{
    while (TRUE)
    {
        // 等待网络事件发生
        int nIndex = WSAWaitForMultipleEvents(nEventTotal, eventArray, FALSE, WSA_INFINITE, FALSE) - WSA_WAIT_EVENT_0;
        for (int i = nIndex; i < nEventTotal; i++)
        {
            if (WSAWaitForMultipleEvents(1, &eventArray[i], TRUE, 1000, FALSE) != WSA_WAIT_TIMEOUT)
            {
                WSANETWORKEVENTS event1;
                WSAEnumNetworkEvents(sockArray[i], eventArray[i], &event1); // 获取网络事件

                if (event1.lNetworkEvents & FD_ACCEPT && event1.iErrorCode[FD_ACCEPT_BIT] == 0)
                {
                    // 处理新的连接请求
                    if (nEventTotal > WSA_MAXIMUM_WAIT_EVENTS)
                        continue;
                    SOCKET sNew = accept(sockArray[i], NULL, NULL);                // 接受新的连接
                    WSAEVENT newEvent = WSACreateEvent();                          // 创建新的事件对象
                    WSAEventSelect(sNew, newEvent, FD_READ | FD_CLOSE | FD_WRITE); // 将事件与新的Socket关联
                    eventArray[nEventTotal] = newEvent;                            // 将新的事件对象存储到事件数组中
                    sockArray[nEventTotal++] = sNew;                               // 将新的Socket存储到Socket数组中
                    sockaddr_in clientAddr;
                    int addrLen = sizeof(clientAddr);
                    getpeername(sNew, (sockaddr *)&clientAddr, &addrLen);
                    printf("新客户端已连接[IP:%s, PORT:%d]\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));
                }
                else if (event1.lNetworkEvents & FD_READ && event1.iErrorCode[FD_READ_BIT] == 0)
                {
                    const char *register_cmd = "REGISTER";
                    const char *login_cmd = "LOGIN";
                    // 处理读取事件
                    char szText[3096];
                    int nRecv = recv(sockArray[i], szText, strlen(szText), 0); // 接收数据
                    if (nRecv > 0)
                    {

                        szText[nRecv] = '\0'; // 添加字符串结束符
                        if (strncmp(szText, register_cmd, strlen(register_cmd)) == 0)
                        {
                            char username[256], password[256];
                            if (sscanf(szText, "REGISTER %s %s", username, password) == 2)
                            {
                                if (register_user(username, password) == 0)
                                {
                                    const char *success_msg = "[系统消息] 注册成功!";
                                    send(sockArray[i], success_msg, strlen(success_msg), 0);
                                }
                                else
                                {
                                    const char *failure_msg = "[系统消息] 注册失败!";
                                    send(sockArray[i], failure_msg, strlen(failure_msg), 0);
                                }
                            }
                            else
                            {
                                fprintf(stderr, "注册格式错误\n");
                                const char *error_msg = "[系统消息] 注册格式错误!";
                                send(sockArray[i], error_msg, strlen(error_msg), 0);
                            }
                        }

                        char username[256], password[256];
                        if (sscanf(szText, "LOGIN %s %s", username, password) == 2)
                        {
                            if (authenticate_user(username, password))
                            {
                                // 更新会话状态
                                for (int j = 0; j < nEventTotal; j++)
                                {
                                    if (sockArray[j] == sockArray[nIndex])
                                    {
                                        // clients[i].logged_in = 1;
                                        // strncpy(clients[i].username, username, sizeof(clients[i].username) - 1);
                                        break;
                                    }
                                }

                                const char *success_msg = "[系统消息] 登录成功!";
                                send(sockArray[nIndex], success_msg, strlen(success_msg), 0);
                                printf("用户 %s 已登录\n", username);
                                printf("服务器套接字: %d\n", sockArray[nIndex]);
                            }
                            else
                            {
                                const char *failure_msg = "[系统消息] 登录失败，用户名或密码错误!";
                                send(sockArray[i], failure_msg, strlen(failure_msg), 0);
                                printf("登录失败，用户名或密码错误\n");
                            }
                        }

                        // 敏感词过滤
                        const char *sensitive_words = "fuck";
                        if (strstr(szText, sensitive_words) != NULL)
                        {
                            const char *sensitive_msg = "[系统消息] 您发送的消息包含敏感词!";
                            send(sockArray[i], sensitive_msg, strlen(sensitive_msg), 0);
                        }
                        else
                        {
                            printf("接收到的消息为:%s", szText);
                            send(sockArray[i], szText, strlen(szText), 0); // 发送响应消息
                        }
                    }
                }
                else if (event1.lNetworkEvents & FD_CLOSE && event1.iErrorCode[FD_CLOSE_BIT] == 0)
                {
                    // 处理关闭事件
                    closesocket(sockArray[i]); // 关闭Socket
                    for (int j = i; j < nEventTotal - 1; j++)
                    {
                        sockArray[j] = sockArray[j + 1];   // 移动数组中的Socket
                        eventArray[j] = eventArray[j + 1]; // 移动数组中的事件对象
                    }
                    nEventTotal--; // 减少事件总数
                }
                else if (event1.lNetworkEvents & FD_WRITE && event1.iErrorCode[FD_WRITE_BIT] == 0)
                {
                    // 处理写入事件
                    char szText[3096] = "消息已被接收";
                    send(sockArray[i], szText, strlen(szText), 0); // 发送消息
                }
            }
        }
    }
}

// 清理资源
void Cleanup(WSAEVENT eventArray[], SOCKET sockArray[], int nEventTotal)
{
    for (int i = 0; i < nEventTotal; i++)
    {
        closesocket(sockArray[i]);    // 关闭Socket
        WSACloseEvent(eventArray[i]); // 关闭事件对象
    }
    WSACleanup(); // 清理Winsock库
}

int main(int argc, CHAR *argv[])
{
    // 变量定义
    WSAEVENT eventArray[WSA_MAXIMUM_WAIT_EVENTS]; // 事件句柄数组
    SOCKET sockArray[WSA_MAXIMUM_WAIT_EVENTS];    // Socket句柄数组
    int nEventTotal = 0;                          // 事件总数
    USHORT nPort = 8080;                          // 监听端口号
    SOCKET sListen;
    WSAEVENT myevent;

    initialize_database();

    InitializeSocket(sListen, myevent, nPort, eventArray, sockArray, nEventTotal); // 初始化Socket

    printf("服务器已启动，等待客户端连接...\n");

    HandleEvents(eventArray, sockArray, nEventTotal); // 处理网络事件
    Cleanup(eventArray, sockArray, nEventTotal);      // 清理资源

    return 0;
}