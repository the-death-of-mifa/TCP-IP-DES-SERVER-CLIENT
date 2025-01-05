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
#include <vector>
#include <algorithm>

#pragma comment(lib, "ws2_32.lib")

MYSQL *conn;

typedef struct
{
    int id;
    char name[100];
    SOCKET creator_socket;
    SOCKET members[10];
    int member_count;
} ChatGroup;

ChatGroup chat_groups[10];
int chat_group_count = 0;

std::vector<int> client_sockets; // 存储所有客户端套接字

void add_client_socket(int client_sock)
{
    client_sockets.push_back(client_sock);
}

void remove_client_socket(int client_sock)
{
    client_sockets.erase(std::remove(client_sockets.begin(), client_sockets.end(), client_sock), client_sockets.end()); // 删除指定元素
}

void send_to_all_clients(const char *message)
{
    for (int sock : client_sockets)
    {
        send(sock, message, strlen(message), 0);
    }
}

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

int create_group(const char *group_name, const char *creator)
{
    char query[256];
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
        fprintf(stderr, "列出群聊失败: %s\n", mysql_error(conn));
        return;
    }
    MYSQL_RES *result = mysql_store_result(conn);
    if (result == NULL)
    {
        fprintf(stderr, "mysql_store_result failed: %s\n", mysql_error(conn));
        return;
    }

    MYSQL_ROW row;
    char buffer[1024];
    while ((row = mysql_fetch_row(result))) // 遍历结果集
    {
        sprintf(buffer, "群聊ID: %s, 群聊名称: %s\n", row[0], row[1]);
        send(client_sock, buffer, strlen(buffer), 0);
        printf("%s", buffer);
    }
    mysql_free_result(result);
}

int delete_group(const char *group_id, const char *username)
{
    char query[256];
    sprintf(query, "DELETE FROM `groups` WHERE id=%s AND creator='%s'", group_id, username);
    if (mysql_query(conn, query))
    {
        fprintf(stderr, "删除群聊失败: %s\n", mysql_error(conn));
        return 1;
    }
    else
    {
        printf("群聊删除成功!\n");
        return 0;
    }
}

void enter_group(SOCKET client_sock, const char *group_id, const char *username)
{
    char buffer[1024];
    char query[256];
    sprintf(query, "SELECT * FROM `groups` WHERE id=%s", group_id);
    if (mysql_query(conn, query))
    {
        fprintf(stderr, "进入群聊失败: %s\n", mysql_error(conn));
        return;
    }
    MYSQL_RES *result = mysql_store_result(conn);
    if (result == NULL)
    {
        fprintf(stderr, "mysql_store_result failed: %s\n", mysql_error(conn));
        return;
    }

    sprintf(buffer, "[系统消息] 用户 %s 进入群聊ID: %s成功\n", username, group_id);
    send_to_all_clients(buffer);
    printf("%s", buffer);
    memset(buffer, 0, sizeof(buffer));
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
                    char szText[3096];
                    int nRecv = recv(sockArray[i], szText, sizeof(szText) - 1, 0);
                    if (nRecv > 0)
                    {
                        szText[nRecv] = '\0';
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
                                const char *success_msg = "[系统消息] 登录成功!";
                                send(sockArray[i], success_msg, strlen(success_msg), 0);
                                printf("用户 %s 已登录\n", username);
                                printf("服务器套接字: %d\n", sockArray[i]);
                            }
                            else
                            {
                                const char *failure_msg = "[系统消息] 登录失败,用户名或密码错误!";
                                send(sockArray[i], failure_msg, strlen(failure_msg), 0);
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
                                    const char *success_msg = "[系统消息] 群聊创建成功!";
                                    send(sockArray[i], success_msg, strlen(success_msg), 0);
                                    chat_groups[chat_group_count].id = chat_group_count + 1;
                                    strcpy(chat_groups[chat_group_count].name, group_name);
                                    chat_groups[chat_group_count].creator_socket = sockArray[i];
                                    chat_groups[chat_group_count].members[0] = sockArray[i];
                                    chat_groups[chat_group_count].member_count = 1;
                                    chat_group_count++;
                                    printf("发送的消息为1: %s\n", success_msg);
                                }
                                else
                                {
                                    const char *failure_msg = "[系统消息] 群聊创建失败!";
                                    send(sockArray[i], failure_msg, strlen(failure_msg), 0);
                                    printf("发送的消息为2: %s\n", failure_msg);
                                }
                            }
                            else
                            {
                                const char *error_msg = "[系统消息] 创建群聊格式错误!";
                                send(sockArray[i], error_msg, strlen(error_msg), 0);
                                printf("发送的消息为3: %s\n", error_msg);
                            }
                        }

                        if (strncmp(szText, list_groups_cmd, strlen(list_groups_cmd)) == 0)
                        {
                            list_groups(sockArray[i]);
                        }

                        if (strncmp(szText, delete_group_cmd, strlen(delete_group_cmd)) == 0)
                        {
                            char group_id[256];
                            char username[256];
                            if (sscanf(szText, "DELETE_GROUP %s %s", group_id, username) == 2)
                            {
                                if (delete_group(group_id, username) == 0)
                                {
                                    const char *success_msg = "[系统消息] 群聊删除成功!";
                                    send(sockArray[i], success_msg, strlen(success_msg), 0);
                                    printf("发送的消息为4: %s\n", success_msg);
                                }
                                else
                                {
                                    const char *failure_msg = "[系统消息] 群聊删除失败!";
                                    send(sockArray[i], failure_msg, strlen(failure_msg), 0);
                                    printf("发送的消息为5: %s\n", failure_msg);
                                }
                            }
                            else
                            {
                                const char *error_msg = "[系统消息] 删除群聊格式错误!";
                                send(sockArray[i], error_msg, strlen(error_msg), 0);
                                printf("发送的消息为6: %s\n", error_msg);
                            }
                        }

                        if (strncmp(szText, enter_group_cmd, strlen(enter_group_cmd)) == 0)
                        {
                            char group_id[256];
                            char username[256];
                            if (sscanf(szText, "ENTER_GROUP %s %s", group_id, username) == 2)
                            {
                                enter_group(sockArray[i], group_id, username);
                            }
                            else
                            {
                                const char *error_msg = "[系统消息] 进入群聊格式错误!";
                                send(sockArray[i], error_msg, strlen(error_msg), 0);
                                printf("发送的消息为7: %s\n", error_msg);
                            }
                        }

                        const char *sensitive_words = "fuck";
                        if (strstr(szText, sensitive_words) != NULL)
                        {
                            const char *sensitive_msg = "[系统消息] 您发送的消息包含敏感词!";
                            send(sockArray[i], sensitive_msg, strlen(sensitive_msg), 0);
                        }
                        else
                        {
                            sockaddr_in clientAddr;
                            int addrLen = sizeof(clientAddr);
                            getpeername(sockArray[i], (sockaddr *)&clientAddr, &addrLen);
                            printf("[客户端IP:%s PORT:%d]:%s\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port), szText);
                            int group_id;
                            if (sscanf(szText, "GROUP_MSG %d %s", &group_id, szText) == 2)
                            {
                                for (int j = 0; j < chat_groups[group_id - 1].member_count; j++)
                                {
                                    if (chat_groups[group_id - 1].members[j] != sockArray[i])
                                    {
                                        char group_msg[3096];
                                        sprintf(group_msg, "[群聊][用户：%s]：%s", inet_ntoa(clientAddr.sin_addr), szText);
                                        send(chat_groups[group_id - 1].members[j], group_msg, strlen(group_msg), 0);
                                    }
                                }
                            }
                            else
                            {
                                if (strncmp(szText, "LOGIN", 5) != 0 && strncmp(szText, "REGISTER", 8) != 0 && strncmp(szText, "CREATE_GROUP", 12) != 0 && strncmp(szText, "LIST_GROUPS", 11) != 0 && strncmp(szText, "ENTER_GROUP", 11) != 0 && strncmp(szText, "DELETE_GROUP", 12) != 0 && strncmp(szText, "GROUP_MSG", 9) != 0)
                                {
                                    send(sockArray[i], szText, strlen(szText), 0);
                                    printf("发送的消息为8: %s\n", szText);
                                }
                            }
                        }
                    }
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
                    char szText[3096] = "消息已被接收";
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