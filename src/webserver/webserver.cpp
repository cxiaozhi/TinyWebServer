#include "webserver/webserver.h"

WebServer::WebServer() {

    // HttpConn 类对象
    WebServer::users = new HttpConn[MAX_FD];
    char serverPath[200];
    getcwd(serverPath, 200);

    // root文件夹路径
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

void WebServer::logWrite() {
    if (0 == WebServer::closeLogData) {
        if (1 == WebServer::logWriteData) {
            Log::get_instance()->init("./ServerLog", WebServer::closeLogData,
                                      2000, 800000, 800);
        } else {
            Log::get_instance()->init("./ServerLog", WebServer::closeLogData,
                                      2000, 800000, 0);
        };
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
    assert(WebServer::listenFd >= 0);

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
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    usersTimer[connfd].timer = timer;
    WebServer::utils.timerLst.addTimer(timer);
}

// 若有数据传输 则将定时器往后延迟三个单位 并对新的定时器在链表上的位置进行调整
void WebServer::adjustTimer(UtilTimer* timer) {
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    WebServer::utils.timerLst.adjustTimer(timer);
    LOG_INFO("%s", "增加时间一次");
}

void WebServer::dealTimer(UtilTimer* timer, int sockfd) {
    timer->cbFunc(&usersTimer[sockfd]);
    if (timer) {
        WebServer::utils.timerLst.delTimer(timer);
    }
    LOG_INFO("关闭文件 %d", usersTimer[sockfd].sockfd);
}

bool WebServer::dealClenetData() {
    struct sockaddr_in clientAddress;
    socklen_t clientAddrLength = sizeof(clientAddress);
    if (0 == WebServer::ListenTrigMode) {
        int connfd =
            accept(WebServer::listenFd, (struct sockaddr*)&clientAddress,
                   &clientAddrLength);
        if (connfd < 0) {
            LOG_ERROR("%s:错误是:%d", "接受错误", errno);
        }
        if (HttpConn::userCount >= MAX_FD) {
            WebServer::utils.showError(connfd, "网络服务繁忙");
            LOG_ERROR("%s", "网络服务繁忙");
            return false;
        }
        timer(connfd, clientAddress);
    } else {
        while (1) {
            int connfd =
                accept(WebServer::listenFd, (struct sockaddr*)&clientAddress,
                       &clientAddrLength);
            if (connfd < 0) {
                LOG_ERROR("%s:错误是:%d", "接受错误", errno);
                break;
            }
            if (HttpConn::userCount >= MAX_FD) {
                WebServer::utils.showError(connfd, "网络服务繁忙");
                LOG_ERROR("%s", "网络服务繁忙");
                break;
            }
            timer(connfd, clientAddress);
        }

        return false;
    }
    return true;
}

bool WebServer::dealWithSignal(bool& timeout, bool& stopServer) {
    int ret = 0;
    int sig;
    char signals[1024];
    ret = recv(WebServer::pipeFd[0], signals, sizeof(signals), 0);
    if (ret == -1) {
        return false;
    } else if (ret == 0) {
        return false;
    } else {
        for (int i = 0; i < ret; i++) {
            switch (signals[i]) {
            case SIGALRM:
                timeout = true;
                break;

            case SIGTERM:
                stopServer = true;
                break;
            }
        }
    }
    return true;
};

void WebServer::dealWithRead(int sockfd) {
    UtilTimer* timer = usersTimer[sockfd].timer;
    if (1 == WebServer::actorModel) {
        if (timer) {
            WebServer::adjustTimer(timer);
        }
        WebServer::pool->append(WebServer::users + sockfd, 0);
        while (true) {
            if (1 == users[sockfd].improv) {
                if (1 == users[sockfd].timerFlag) {
                    dealTimer(timer, sockfd);
                    WebServer::users[sockfd].timerFlag = 0;
                }
                WebServer::users[sockfd].improv = 0;
                break;
            }
        }
    } else {
        if ((WebServer::users[sockfd].readOnce())) {
            LOG_INFO(
                "客户端连接(%s)",
                inet_ntoa(WebServer::users[sockfd].getAddress()->sin_addr));
            WebServer::pool->appendP(WebServer::users + sockfd);
            if (timer) {
                adjustTimer(timer);
            }
        } else {
            dealTimer(timer, sockfd);
        }
    }
}

void WebServer::dealWithWrite(int sockfd) {
    UtilTimer* timer = WebServer::usersTimer[sockfd].timer;
    if (1 == WebServer::actorModel) {
        if (timer) {
            WebServer::adjustTimer(timer);
        }
        WebServer::pool->append(WebServer::users + sockfd, 1);
        while (true) {
            if (1 == WebServer::users[sockfd].improv) {
                if (1 == users[sockfd].timerFlag) {
                    dealTimer(timer, sockfd);
                    users[sockfd].timerFlag = 0;
                }
                WebServer::users[sockfd].improv = 0;
                break;
            }
        }
    } else {
        if (WebServer::users[sockfd].write()) {
            LOG_INFO("发送数据到客户端(%s)",
                     inet_ntoa(users[sockfd].getAddress()->sin_addr));
            if (timer) {
                adjustTimer(timer);
            }
        } else {
            dealTimer(timer, sockfd);
        }
    }
}

void WebServer::eventLoop() {
    bool timeout = false;
    bool stopServer = false;
    while (!stopServer) {
        int number = epoll_wait(WebServer::epollfd, WebServer::events,
                                MAX_EVENT_NUMBER, -1);
        if (number < 0 && errno != EINTR) {
            LOG_ERROR("%s", "池化失败");
            break;
        }
        for (int i = 0; i < number; i++) {
            int sockfd = events[i].data.fd;

            // 处理新到的客户连接
            if (sockfd == WebServer::listenFd) {
                bool flag = dealClenetData();
                if (false == flag) {
                    continue;
                }
            } else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                UtilTimer* timer = usersTimer[sockfd].timer;
                dealTimer(timer, sockfd);
            } else if ((sockfd == WebServer::pipeFd[0]) &&
                       (events[i].events & EPOLLIN)) {
                bool flag = dealWithSignal(timeout, stopServer);
                if (false == flag)
                    LOG_ERROR("%s", "处理客户数据失败");
            } else if (events[i].events & EPOLLIN) {
                dealWithRead(sockfd);
            } else if (events[i].events & EPOLLOUT) {
                dealWithWrite(sockfd);
            }
        }
        if (timeout) {
            utils.timerHandler();
            LOG_INFO("%s", "心跳");
            timeout = false;
        }
    }
}