#pragma once

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cassert>

#include "../http/httpConn.h"
#include "../threadpool/threadpool.h"
#include "../timer/lstTimer.h"

const int MAX_FD = 65536;            // 最大文件描述符
const int MAX_EVENT_NUMBER = 65536;  // 最大事件数
const int TIMESLOT = 5;              // 最小超时单位

class WebServer {
private:
    /* data */
public:
    // 基础
    int port;
    char* root;
    int logWriteData;
    int closeLogData;
    int actorModel;
    int pipeFd[2];
    int epollfd;
    HttpConn* users;

    ConnectionPool* connPool;
    string user;
    string password;
    string databaseName;
    int sqlNum;

    Threadpool<HttpConn>* pool;
    int threadNum;

    // 事件池
    epoll_event events[MAX_EVENT_NUMBER];

    int listenFd;
    int optLinger;
    int TrigMode;
    int ListenTrigMode;
    int ConnTrigMode;

    // 定时器相关
    ClientData* usersTimer;
    Utils utils;

public:
    WebServer();
    ~WebServer();

    void init(int port, string user, string password, string dataBaseName,
              int logWriteData, int optLinger, int trigMode, int sqlNum,
              int threadNum, int closeLog, int actorModel);
    void threadPool();
    void sqlPool();
    void logWrite();
    void trigMode();
    void eventListen();
    void eventLoop();
    void timer(int connfd, struct sockaddr_in clientAddress);
    void adjustTimer(UtilTimer* timer);
    void dealTimer(UtilTimer* timer, int sockfd);
    bool dealClenetData();
    bool dealWithSignal(bool& timeout, bool& stopServer);
    void dealWithRead(int sockfd);   // 读
    void dealWithWrite(int sockfd);  // 写
};
