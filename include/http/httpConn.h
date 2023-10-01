#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <map>
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
#include <unistd.h>

#include "locker.h"
#include "log.h"
#include "lstTimer.h"
#include "sqlConnectionPool.h"

class HttpConn {

public:
    static const int FILE_NAME_LEN = 200;
    static const int READ_BUFFER_SIZE = 2048;
    static const int WRITE_BUFFER_SIZE = 1024;
    static int epollfd;
    static int userCount;
    MYSQL* mysql;
    int rwState; // 读为0 写为1

    enum METHOD {
        GET,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATH,
    };
    enum CHECK_STATE {
        CHECK_STATE_REQUEST_LINE,
        CHECK_STATE_HEADER,
        CHECK_STATE_CONTENT
    };
    enum HTTP_CODE {
        NO_REQUEST,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURCE,
        FOR_BIDDEN_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTTON
    };
    enum LINE_STATUS { LINE_OK, LINE_BAD, LINE_OPEN };

private:
    int sockFd;
    sockaddr_in address;
    char readBuf[HttpConn::READ_BUFFER_SIZE];
    long readIdx;
    long checkedIdx;
    int startLine;
    char writeBuf[HttpConn::WRITE_BUFFER_SIZE];
    int writeIdx;
    CHECK_STATE checkState;
    METHOD method;
    char realFile[HttpConn::FILE_NAME_LEN];
    char* url;
    char* version;
    char* host;
    long contentLength;
    bool linger;
    char* fileAddress;
    struct stat fileStat;
    struct iovec iv[2];
    int ivCount;
    int cgi;          // 是否启用的POST
    char* headString; // 存储请求头数据
    int bytesToSend;
    int bytesHaveSend;
    char* docRoot;
    map<string, string> users;
    int TrigMode;
    int closeLog;

    char sqlUser[100];
    char sqlPasswd[100];
    char sqlName[100];

public:
    HttpConn(/* args */);
    ~HttpConn();

public:
    void init(int sockfd, const sockaddr_in& addr, char*, int, int, string user,
              string passwd, string sqlname);
    void closeConn(bool realClose = true);
    void process();
    void readOnce();
    bool write();
    sockaddr_in* getAddress() {}
};
