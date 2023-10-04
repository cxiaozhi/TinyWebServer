#pragma once

#include "webserver/webserver.h"
#include <arpa/inet.h>
#include <cassert>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

WebServer::WebServer() {
    WebServer::users = new HttpConn[MAX_FD];
    char serverPath[200];
    getcwd(serverPath, 200);
    char root[6] = "/root";
    WebServer::root =
        (char*)malloc(strlen(serverPath) + strlen(WebServer::root) + 1);

    strcpy(WebServer::root, serverPath);
    strcpy(WebServer::root, root);

    // 定时器
    WebServer::usersTimer = new ClientData[MAX_FD];
}

WebServer::~WebServer() {
    close(WebServer::epollfd);
    close(WebServer::listenFd);
    close(WebServer::pipeFd[1]);
    close(WebServer::pipeFd[0]);
    delete[] WebServer::users;
    delete[] WebServer::usersTimer;
    delete WebServer::pool;
}

void WebServer::init(int port, string user, string password,
                     string dataBaseName, int logWriteData, int optLinger,
                     int trigMode, int sqlNum, int threadNum, int closeLog,
                     int actorModel) {

    WebServer::port = port;
    WebServer::user = user;
    WebServer::password = password;
    WebServer::databaseName = dataBaseName;
    WebServer::sqlNum = sqlNum;
    WebServer::threadNum = threadNum;
    WebServer::logWriteData = logWriteData;
    WebServer::optLinger = optLinger;
    WebServer::TrigMode = trigMode;
    WebServer::closeLogData = closeLog;
    WebServer::actorModel = actorModel;
}

void WebServer::trigMode() {
    if (0 == WebServer::TrigMode) {
        WebServer::ListenTrigMode = 0;
        WebServer::ConnTrigMode = 0;
    } else if (1 == WebServer::TrigMode) {
        WebServer::ListenTrigMode = 0;
        WebServer::ConnTrigMode = 1;
    } else if (2 == WebServer::TrigMode) {
        WebServer::ListenTrigMode = 1;
        WebServer::ConnTrigMode = 0;
    } else if (3 == WebServer::TrigMode) {
        WebServer::ListenTrigMode = 1;
        WebServer::ConnTrigMode = 1;
    }
}

void WebServer::sqlPool() {
    WebServer::connPool = ConnectionPool::GetInstance();
    WebServer::connPool->init("localhost", WebServer::user, WebServer::password,
                              WebServer::databaseName, 3306, WebServer::sqlNum,
                              WebServer::closeLogData);
    WebServer::users->initMysqlResult(WebServer::connPool);
}

void WebServer::threadPool() {
    // 线程池
    WebServer::pool = new Threadpool<HttpConn>(
        WebServer::actorModel, WebServer::connPool, WebServer::threadNum);
}

void WebServer::eventListen() {
    // 网络编程基础步骤
    WebServer::listenFd = socket(PF_INET, SOCK_STREAM, 0);

    // 优雅的关闭连接
    if (0 == WebServer::optLinger) {
        struct linger tmp = {0, 1};
        setsockopt(WebServer::listenFd, SOL_SOCKET, SO_LINGER, &tmp,
                   sizeof(tmp));
    } else if (1 == WebServer::optLinger) {
        struct linger tmp = {1, 1};
        setsockopt(WebServer::listenFd, SOL_SOCKET, SO_LINGER, &tmp,
                   sizeof(tmp));
    }

    int ret = 0;
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(WebServer::port);

    int flag = 1;
    setsockopt(WebServer::listenFd, SOL_SOCKET, SO_REUSEADDR, &flag,
               sizeof(flag));
    ret =
        bind(WebServer::listenFd, (struct sockaddr*)&address, sizeof(address));
    assert(ret >= 0);
    ret = listen(WebServer::listenFd, 5);
    assert(ret >= 0);
    utils.init(TIMESLOT);

    // epoll创建内核事件表
    epoll_event event[MAX_EVENT_NUMBER];

    WebServer::epollfd = epoll_create(5);

    assert(WebServer::epollfd != -1);

    utils.setNonBlocking(WebServer::pipeFd[1]);
    utils.addfd(WebServer::epollfd, WebServer::pipeFd[0], false, 0);
    utils.addsig(SIGPIPE, SIG_IGN);
    utils.addsig(SIGALRM, utils.sigHandler, false);
    utils.addsig(SIGPIPE, utils.sigHandler, false);

    alarm(TIMESLOT);

    // 工具类信号和描述符基础操作
    Utils::pipeFd = WebServer::pipeFd;
    Utils::epollFd = WebServer::epollfd;
}

void WebServer::timer(int connfd, sockaddr_in clientAddress) {
    WebServer::users[connfd].init(connfd, clientAddress, WebServer::root,
                                  WebServer::ConnTrigMode,
                                  WebServer::closeLogData, WebServer::user,
                                  WebServer::password, WebServer::databaseName);

    // 初始化client data数据
    WebServer::usersTimer[connfd].address = clientAddress;
    WebServer::usersTimer[connfd].sockfd = connfd;

    UtilTimer* timer = new UtilTimer;

    timer->userData = &WebServer::usersTimer[connfd];
    timer->cbFunc = cbFunc;
}