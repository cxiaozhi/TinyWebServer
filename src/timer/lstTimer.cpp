
#include "../../include/timer/lstTimer.h"

#include "../../include/http/httpConn.h"

SortTimerLst::SortTimerLst() {
    head = NULL;
    tail = NULL;
}

SortTimerLst::~SortTimerLst() {
    UtilTimer* tmp = head;
    while (tmp) {
        head = tmp->next;
        delete tmp;
        tmp = head;
    }
}

void SortTimerLst::addTimer(UtilTimer* timer) {
    if (!timer) {
        return;
    }

    if (!head) {
        head = tail = timer;
        return;
    }

    if (timer->expire < head->expire) {
        timer->next = head;
        head->prev = timer;
        head = timer;
        return;
    }

    addTimer(timer, head);
}

void SortTimerLst::adjustTimer(UtilTimer* timer) {
    if (!timer) {
        return;
    }

    UtilTimer* tmp = timer->next;
    if (!tmp || (timer->expire < tmp->expire)) {
        return;
    }

    if (timer == head) {
        head = head->next;
        head->prev = NULL;
        timer->next = NULL;
        addTimer(timer, head);
    } else {
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        addTimer(timer, timer->next);
    }
}

void SortTimerLst::delTimer(UtilTimer* timer) {
    if (!timer) {
        return;
    }

    if ((timer == head) && (timer == tail)) {
        delete timer;
        head = NULL;
        tail = NULL;
        return;
    }

    if (timer == head) {
        head = head->next;
        head->prev = NULL;
        delete timer;
        return;
    }
    if (timer == tail) {
        tail = tail->next;
        tail->prev = NULL;
        delete timer;
        return;
    }

    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
}

void SortTimerLst::tick() {
    if (!head) {
        return;
    }
    time_t cur = time(NULL);
    UtilTimer* tmp = head;
    while (tmp) {
        if (cur < tmp->expire) {
            break;
        }
        tmp->cbFunc(tmp->userData);
        head = tmp->next;
        if (head) {
            head->prev = NULL;
        }
        delete tmp;
        tmp = head;
    }
}

void SortTimerLst::addTimer(UtilTimer* timer, UtilTimer* lstHead) {
    UtilTimer* prev = lstHead;
    UtilTimer* tmp = prev->next;
    while (tmp) {
        if (timer->expire < tmp->expire) {
            prev->next = timer;
            timer->next = tmp;
            tmp->prev = timer;
            timer->prev = prev;
            break;
        }
        prev = tmp;
        tmp = tmp->next;
    }

    if (!tmp) {
        prev->next = timer;
        timer->prev = prev;
        timer->next = NULL;
        tail = timer;
    }
}

void Utils::init(int timeslot) {
    Utils::TIMESLOT = timeslot;
}

// 对文件描述符设置非阻塞
int Utils::setNonBlocking(int fd) {
    int oldOption = fcntl(fd, F_GETFL);
    int newOption = oldOption | O_NONBLOCK;
    fcntl(fd, F_SETFL, newOption);
    return oldOption;
}

// 将内核时间表注册读事件 et模式 选择开启 epolloneshot
void Utils::addfd(int epollfd, int fd, bool oneShot, int trigMode) {
    epoll_event event;
    event.data.fd = fd;

    if (1 == trigMode)
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else
        event.events = EPOLLIN | EPOLLRDHUP;
    if (oneShot) event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setNonBlocking(fd);
}

// 信号处理函数
void Utils::sigHandler(int sig) {
    // 为保证函数的可重入性 保留原来的errno
    int svaeErrno = errno;
    int msg = sig;
    send(Utils::pipeFd[1], (char*)&msg, 1, 0);
    errno = svaeErrno;
}

// 设置信号函数
void Utils::addsig(int sig, void(handler)(int), bool restart) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart) sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

// 定适合处理任务 重新定时以不断触发sigalrm信号
void Utils::timerHandler() {
    Utils::timerLst.tick();
    alarm(Utils::TIMESLOT);
}

void Utils::showError(int connfd, const char* info) {
    send(connfd, info, strlen(info), 0);
    close(connfd);
}
int* Utils::pipeFd = 0;
int Utils::epollFd = 0;

class Utils;
void cbFunc(ClientData* userData) {
    epoll_ctl(Utils::epollFd, EPOLL_CTL_DEL, userData->sockfd, 0);
    assert(userData);
    close(userData->sockfd);
    HttpConn::userCount--;
};