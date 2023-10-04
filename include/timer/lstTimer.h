#include "log.h"
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

class UtilTimer {
public:
    UtilTimer* prev;
    UtilTimer* next;
    time_t expire;
    void (*cbFunc)(ClientData*);
    ClientData* userData;

public:
    UtilTimer() : prev(NULL), next(NULL){};
    ~UtilTimer();
};

struct ClientData {
    sockaddr_in address;
    int sockfd;
    UtilTimer* timer;
};

class SortTimerLst {
private:
    void addTimer(UtilTimer* timer, UtilTimer* lstHead);
    UtilTimer* head;
    UtilTimer* tail;

public:
    SortTimerLst();
    ~SortTimerLst();

    void addTimer(UtilTimer* timer);
    void adjustTimer(UtilTimer* timer);
    void delTimer(UtilTimer* timer);
    void tick();
};

class Utils {
private:
    /* data 把数据定义在后面 无法直接看出来是成员变量 还是传递的数据*/
public:
    static int* pipeFd;
    SortTimerLst timerLst;
    static int epollFd;
    int TIMESLOT;

public:
    Utils();
    ~Utils();

    void init(int timeslot);
    int setNonBlocking(int fd); // 对文件描述符设置非阻塞

    void
    addfd(int epollfd, int fd, bool oneShot,
          int trigMode); // 将内核事件表注册读事件 ET模式 选择开启epoll one shot
    static void sigHandler(int sig); // 信号处理函数
    void addsig(int sig, void(handler)(int),
                bool restart = true); // 设置信号函数
    void timerHandler(); // 定时处理任务 重新定时以不断触发sigalrm 信号
    void showError(int connfd, const char* info);
};

void cbFunc(ClientData* userData);