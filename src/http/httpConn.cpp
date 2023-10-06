#include "../../include/http/httpConn.h"

#include <mysql/mysql.h>

#include <fstream>

// 定义http响应的一些状态信息
const char* ok200Title = "OK";
const char* error400Title = "错误请求\n";
const char* error400From = "请求语法错误\n";
const char* error403Title = "禁止请求\n";
const char* error403From = "您没有请求权限\n";
const char* error404Title = "未找到\n";
const char* error404From = "未找到请求文件\n";
const char* error500Title = "服务器内部错误\n";
const char* error500From = "服务器文件请求异常\n";

Locker lock;
map<string, string> users;

void HttpConn::initMysqlResult(ConnectionPool* connPool) {
    // 先从连接池中取一个连接
    MYSQL* mysql = NULL;
    ConnectionRaii mysqlcon(&mysql, connPool);

    const char* loginSql = "SELECT username,passwd FROM user;";
    // 在user表中检索username passwd数据 浏览器端输入
    if (mysql_query(mysql, loginSql)) {
        LOG_ERROR("查询失败:%s\n", mysql_error(mysql));
    }

    // 从表中检索完整的结果集
    MYSQL_RES* result = mysql_store_result(mysql);

    // 返回结果集中的列数
    int numFields = mysql_num_fields(result);
    // 返回所有字段结构的数组
    MYSQL_FIELD* fields = mysql_fetch_fields(result);

    // 从结果集中获取下一行 将对应的用户名和密码 存入map中
    while (MYSQL_ROW row = mysql_fetch_row(result)) {
        string temp1(row[0]);
        string temp2(row[1]);
        users[temp1] = temp2;
    }
}

// 对文件描述符设置非阻塞
int setnonblocking(int fd) {
    int oldOption = fcntl(fd, F_GETFL);
    int newOption = oldOption | O_NONBLOCK;
    fcntl(fd, F_SETFL, newOption);
    return oldOption;
}

// 将内核事件表注册读事件 ET模式 选择开启epoll one shot
void addfd(int epollfd, int fd, bool oneShot, int trigMode) {
    epoll_event event;
    event.data.fd = fd;

    if (1 == trigMode) {
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    } else {
        event.events = EPOLLIN | EPOLLRDHUP;
    }

    if (oneShot) event.events |= EPOLLONESHOT;

    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);

    setnonblocking(fd);
}

// 从内核时间表删除描述符
void removefd(int epollfd, int fd) {
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

// 将事件重置为EPOLLONESHOT
void modfd(int epollfd, int fd, int ev, int trigmode) {
    epoll_event event;
    event.data.fd = fd;
    if (1 == trigmode)
        event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    else
        event.events = ev | EPOLLONESHOT | EPOLLRDHUP;

    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

int HttpConn::userCount = 0;
int HttpConn::epollfd = -1;

sockaddr_in* HttpConn::getAddress() {
    return &sockAddress;
};
// 关闭连接 关闭一个连接客户总量减一
void HttpConn::closeConn(bool realClose) {
    if (realClose && (sockFd != -1)) {
        printf("关闭 %d\n", sockFd);
        removefd(HttpConn::epollfd, HttpConn::sockFd);
        HttpConn::sockFd = -1;
        HttpConn::userCount--;
    }
}

// 初始化连接 外部调用初始化套接字地址
void HttpConn::init(int sockfd, const sockaddr_in& addr, char* root,
                    int trigmode, int closeLog, string user, string passwd,
                    string sqlName) {
    HttpConn::sockFd = sockfd;
    HttpConn::sockAddress = addr;
    addfd(HttpConn::epollfd, sockfd, true, HttpConn::TrigMode);
    HttpConn::userCount++;

    // 当浏览器出现连接重置时,可能是网站根目录或http响应格式出错或者访问的文件中内容完全为空
    HttpConn::docRoot = root;
    HttpConn::TrigMode = trigmode;
    HttpConn::closeLogData = closeLog;

    strcpy(HttpConn::sqlUser, user.c_str());
    strcpy(HttpConn::sqlPasswd, passwd.c_str());
    strcpy(HttpConn::sqlName, sqlName.c_str());

    init();
}

// 初始化新接受的连接
// checkState 默认为分析请求状态
void HttpConn::init() {
    HttpConn::mysql = NULL;
    HttpConn::bytesHaveSend = 0;
    HttpConn::bytesHaveSend = 0;
    HttpConn::checkState = CHECK_STATE_REQUEST_LINE;
    HttpConn::linger = false;
    HttpConn::method = GET;
    HttpConn::url = 0;
    HttpConn::version = 0;
    HttpConn::contentLength = 0;
    HttpConn::host = 0;
    HttpConn::startLine = 0;
    HttpConn::checkedIdx = 0;
    HttpConn::readIdx = 0;
    HttpConn::writeIdx = 0;
    HttpConn::cgi = 0;
    HttpConn::rwState = 0;
    HttpConn::timerFlag = 0;
    HttpConn::improv = 0;

    memset(HttpConn::readBuf, '\0', READ_BUFFER_SIZE);
    memset(HttpConn::writeBuf, '\0', WRITE_BUFFER_SIZE);
    memset(HttpConn::realFile, '\0', FILE_NAME_LEN);
}

// 从状态机 用于分析出一行内容
// 返回值为行的读取状态 有LINE_OK LINE_BAD LINE_OPEN
HttpConn::LINE_STATUS HttpConn::parseLine() {
    char temp;
    for (; HttpConn::checkedIdx < HttpConn::readIdx; HttpConn::checkedIdx++) {
        temp = HttpConn::readBuf[HttpConn::checkedIdx];
        if (temp == '\r') {
            if ((HttpConn::checkedIdx + 1) == HttpConn::readIdx) {
                return LINE_OPEN;
            } else if (HttpConn::readBuf[HttpConn::checkedIdx + 1] == '\n') {
                HttpConn::readBuf[HttpConn::checkedIdx++] = '\0';
                HttpConn::readBuf[HttpConn::checkedIdx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        } else if (temp == '\n') {
            if (HttpConn::checkedIdx > 1 &&
                HttpConn::readBuf[HttpConn::checkedIdx - 1] == '\r') {
                HttpConn::readBuf[HttpConn::checkedIdx - 1] = '\0';
                HttpConn::readBuf[HttpConn::checkedIdx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }

    return LINE_OPEN;
}

// 循环读取客户数据 知道无数据可读或对方关闭连接
// 非阻塞ET工作模式下 需要一次性将数据读完
bool HttpConn::readOnce() {
    if (HttpConn::readIdx >= READ_BUFFER_SIZE) {
        return false;
    }
    int bytesRead = 0;
    if (0 == HttpConn::TrigMode) {
        bytesRead =
            recv(HttpConn::sockFd, HttpConn::readBuf + HttpConn::readIdx,
                 READ_BUFFER_SIZE - HttpConn::readIdx, 0);
        HttpConn::readIdx += bytesRead;
        if (bytesRead <= 0) {
            return false;
        }
        return true;
    } else {
        while (true) {
            bytesRead =
                recv(HttpConn::sockFd, HttpConn::readBuf + HttpConn::readIdx,
                     READ_BUFFER_SIZE - HttpConn::readIdx, 0);
            if (bytesRead == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                return false;
            } else if (bytesRead == 0) {
                return false;
            }
            HttpConn::readIdx += bytesRead;
        }
        return true;
    }
}

// 解析http请求行 获得请求方法 目标URL 以及http版本号
HttpConn::HTTP_CODE HttpConn::parseRequestLine(char* text) {
    HttpConn::url = strpbrk(text, " \t");
    printf(HttpConn::url);
    if (!HttpConn::url) {
        return BAD_REQUEST;
    }

    *HttpConn::url++ = '\0';
    char* method = text;
    if (strcasecmp(method, "GET") == 0) {
        HttpConn::method = HttpConn::GET;
    } else if (strcasecmp(method, "POST") == 0) {
        HttpConn::method = HttpConn::POST;
        HttpConn::cgi = 1;
    } else {
        return BAD_REQUEST;
    }
    HttpConn::url += strspn(HttpConn::url, " \t");
    HttpConn::version = strpbrk(HttpConn::url, " \t");
    if (!HttpConn::version) return BAD_REQUEST;
    *HttpConn::version++ = '\0';
    HttpConn::version += strspn(HttpConn::version, " \t");
    if (strcasecmp(HttpConn::url, "HTTP/1.1") != 0) {
        return BAD_REQUEST;
    }

    if (strncasecmp(HttpConn::url, "http://", 7) == 0) {
        HttpConn::url += 7;
        HttpConn::url = strchr(HttpConn::url, '/');
    }
    if (strncasecmp(HttpConn::url, "https://", 8) == 0) {
        HttpConn::url += 8;
        HttpConn::url = strchr(HttpConn::url, '/');
    }

    if (!HttpConn::url || HttpConn::url[0] != '/') {
        return BAD_REQUEST;
    }

    if (strlen(HttpConn::url) == 1) {
        strcat(HttpConn::url, "judge.html");
    }

    HttpConn::checkState = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

// 解析http请求的一个头部信息
HttpConn::HTTP_CODE HttpConn::parseHeaders(char* text) {
    if (text[0] == '\0') {
        if (HttpConn::contentLength != 0) {
            HttpConn::checkState = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    } else if (strncasecmp(text, "连接:", 11) == 0) {
        text += 11;
        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0) {
            HttpConn::linger = true;
        }
    } else if (strncasecmp(text, "Host:", 5) == 0) {
        text += 5;
        text += strspn(text, " \t");
        HttpConn::host = text;
    } else {
        LOG_INFO("没有头部信息: %s", text);
    }
    return NO_REQUEST;
}

// 判断http请求是否被完整读入
HttpConn::HTTP_CODE HttpConn::parseContent(char* text) {
    if (HttpConn::readIdx >= (HttpConn::contentLength + HttpConn::checkedIdx)) {
        text[HttpConn::contentLength] = '\0';
        // post 请求中最后为输入的用户名和密码
        HttpConn::headString = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

char* HttpConn::getLine() {
    return readBuf + startLine;
}

HttpConn::HTTP_CODE HttpConn::processRead() {
    LINE_STATUS lineStatus = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char* text = 0;
    while ((HttpConn::checkState == CHECK_STATE_CONTENT &&
            lineStatus == LINE_OK) ||
           ((lineStatus = parseLine()) == LINE_OK)) {
        text = HttpConn::getLine();
        HttpConn::startLine = HttpConn::checkedIdx;
        LOG_INFO("%s", text);
        switch (HttpConn::checkState) {
            case CHECK_STATE_REQUEST_LINE: {
                ret = parseRequestLine(text);
                if (ret == BAD_REQUEST) {
                    return BAD_REQUEST;
                }
                break;
            }

            case CHECK_STATE_HEADER: {
                ret = parseHeaders(text);
                if (ret == BAD_REQUEST)
                    return BAD_REQUEST;
                else if (ret == GET_REQUEST) {
                    return doRequest();
                }
                break;
            }
            case CHECK_STATE_CONTENT: {
                ret = parseContent(text);
                if (ret == GET_REQUEST) {
                    return doRequest();
                }
                lineStatus = LINE_OPEN;
                break;
            }
            default:
                return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;
}

HttpConn::HTTP_CODE HttpConn::doRequest() {
    strcpy(HttpConn::realFile, docRoot);
    int len = strlen(docRoot);
    const char* p = strrchr(HttpConn::url, '/');

    // 处理CGI
    if (cgi == 1 && (*(p + 1) == '2') || *(p + 1) == '3') {
        // 根据标志判断登录检测还是注册检测
        char flag = HttpConn::url[1];
        char* urlReal = (char*)malloc(sizeof(char) * 200);
        strcpy(urlReal, "/");
        strcpy(urlReal, HttpConn::url + 2);
        strncpy(realFile + len, urlReal, FILE_NAME_LEN - len - 1);
        free(urlReal);

        // 将用户名和密码提取出来
        char name[100], password[100];
        int i;
        for (i = 5; HttpConn::headString[i] != '&'; i++) {
            name[i - 5] = HttpConn::headString[i];
        }
        name[i - 5] = '\0';
        int j = 0;
        for (i = i + 10; HttpConn::headString[i] != '\0'; i++, j++) {
            password[j] = HttpConn::headString[i];
        }
        password[j] = '\0';

        if (*(p + 1) == '\3') {
            // 如果是注册 先检测数据库中是否有重名的
            // 没有重名的 进行增加数据
            char* sqlInsert = (char*)malloc(sizeof(char) * 200);
            strcpy(sqlInsert, "INSERT INTO user(username,passwd) values(");
            strcat(sqlInsert, "");
            strcat(sqlInsert, name);
            strcat(sqlInsert, "', '");
            strcat(sqlInsert, "')");

            if (users.find(name) == users.end()) {
                lock.lock();
                int res = mysql_query(mysql, sqlInsert);
                users.insert(pair<string, string>(name, password));
                lock.unlock();
                if (!res)
                    strcpy(HttpConn::url, "/log.html");
                else
                    strcpy(HttpConn::url, "/registerError.html");
            } else {
                strcpy(HttpConn::url, "/registerError.html");
            }
        } else if (*(p + 1) == '2') {
            // 如果是登录 直接判断
            // 若浏览器端输入的用户名和密码在表中可以查找到 返回1 否则返回0
            if (users.find(name) != users.end() && users[name] == password) {
                strcpy(HttpConn::url, "/welcome.html");
            } else {
                strcpy(HttpConn::url, "/logError.html");
            }
        }
    }
    if (*(p + 1) == '0') {
        char* urlReal = (char*)malloc(sizeof(char) * 200);
        strcpy(urlReal, "/register.html");
        strncpy(HttpConn::realFile + len, urlReal, strlen(urlReal));
        free(urlReal);
    } else if (*(p + 1) == '1') {
        char* urlReal = (char*)malloc(sizeof(char) * 200);
        strcpy(urlReal, "/log.html");
        strncpy(realFile + len, urlReal, strlen(urlReal));
        free(urlReal);
    } else if (*(p + 1) == '5') {
        char* urlReal = (char*)malloc(sizeof(char) * 200);
        strcpy(urlReal, "/picture.html");
        strncpy(realFile + len, urlReal, strlen(urlReal));
        free(urlReal);
    } else if (*(p + 1) == '6') {
        char* urlReal = (char*)malloc(sizeof(char) * 200);
        strcpy(urlReal, "/video.html");
        strncpy(realFile + len, urlReal, strlen(urlReal));
        free(urlReal);
    } else if (*(p + 1) == '7') {
        char* urlReal = (char*)malloc(sizeof(char) * 200);
        strcpy(urlReal, "/fans.html");
        strncpy(realFile + len, urlReal, strlen(urlReal));
        free(urlReal);
    } else {
        strncpy(realFile + len, HttpConn::url, FILE_NAME_LEN - len - 1);
    }
    if (stat(HttpConn::realFile, &fileStat) < 0) {
        return NO_RESOURCE;
    }
    if (!(fileStat.st_mode & S_IROTH)) return FOR_BIDDEN_REQUEST;
    if (S_ISDIR(fileStat.st_mode)) return BAD_REQUEST;
    int fd = open(realFile, O_RDONLY);
    HttpConn::fileAddress =
        (char*)mmap(0, fileStat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return FILE_REQUEST;
}

void HttpConn::unmap() {
    if (HttpConn::fileAddress) {
        munmap(HttpConn::fileAddress, fileStat.st_size);
        HttpConn::fileAddress = 0;
    }
}

bool HttpConn::write() {
    int temp = 0;
    if (bytesToSend == 0) {
        modfd(epollfd, sockFd, EPOLLIN, TrigMode);
        init();
        return true;
    }
    while (1) {
        temp = writev(sockFd, iv, ivCount);
        if (temp < 0) {
            if (errno == EAGAIN) {
                modfd(epollfd, sockFd, EPOLLIN, TrigMode);
                return true;
            }
            unmap();
            return false;
        }

        bytesHaveSend += temp;
        bytesToSend -= temp;
        if (bytesHaveSend >= iv[0].iov_len) {
            iv[0].iov_len = 0;
            iv[1].iov_base = fileAddress + (bytesHaveSend - writeIdx);
            iv[1].iov_len = bytesToSend;
        } else {
            iv[0].iov_base = writeBuf + bytesHaveSend;
            iv[0].iov_len = iv[0].iov_len - bytesHaveSend;
        }

        if (bytesToSend <= 0) {
            unmap();
            modfd(epollfd, sockFd, EPOLLIN, TrigMode);
            if (linger) {
                init();
                return true;
            } else {
                return false;
            }
        }
    }
}

bool HttpConn::addResponse(const char* format, ...) {
    if (writeIdx >= WRITE_BUFFER_SIZE) return false;
    va_list argList;
    va_start(argList, format);
    int len = vsnprintf(writeBuf + writeIdx, WRITE_BUFFER_SIZE - 1 - writeIdx,
                        format, argList);
    if (len >= (WRITE_BUFFER_SIZE - 1 - writeIdx)) {
        va_end(argList);
        return false;
    }
    writeIdx += len;
    va_end(argList);
    LOG_INFO("请求:%s", writeBuf);
    return true;
}

bool HttpConn::addStatusLine(int status, const char* title) {
    return addResponse("%s %d %s\r\n", "HTTP/1.1", status, title);
}

bool HttpConn::addHeaders(int contentLen) {
    return addContentLength(contentLen) && addLinger() && addBlankLine();
}

bool HttpConn::addContentLength(int contentLen) {
    return addResponse("Ccontent-Length:%d\r\n", contentLen);
}

bool HttpConn::addContentType() {
    return addResponse("Connection-Type:%s\r\n", "text/html");
}

bool HttpConn::addLinger() {
    return addResponse("Connection:%s\r\n",
                       (linger == true) ? "keep-alive" : "close");
}

bool HttpConn::addBlankLine() {
    return addResponse("%s", "\r\n");
}

bool HttpConn::addContent(const char* content) {
    return addResponse("%s", content);
};

bool HttpConn::processWrite(HTTP_CODE ret) {
    switch (ret) {
        case INTERNAL_ERROR: {
            addStatusLine(404, error404Title);
            addHeaders(strlen(error404From));
            if (!addContent(error404From)) return false;
            break;
        }

        case FOR_BIDDEN_REQUEST: {
            addStatusLine(403, error403Title);
            addHeaders(strlen(error403From));
            if (!addContent(error403From)) {
                return false;
            }
            break;
        }
        case FILE_REQUEST: {
            addStatusLine(200, ok200Title);
            if (fileStat.st_size != 0) {
                addHeaders(fileStat.st_size);
                iv[0].iov_base = writeBuf;
                iv[0].iov_len = writeIdx;
                iv[1].iov_base = fileAddress;
                iv[1].iov_len = fileStat.st_size;
                ivCount = 2;
                bytesToSend = writeIdx + fileStat.st_size;
                return true;
            } else {
                const char* okString = "<html><body></body></html>";
                addHeaders(strlen(okString));
                if (!addContent(okString)) return false;
            }
        }
        default:
            return false;
    }
    iv[0].iov_base = writeBuf;
    iv[0].iov_len = writeIdx;
    ivCount = 1;
    bytesToSend = writeIdx;
    return true;
}

void HttpConn::process() {
    HTTP_CODE readRet = processRead();
    if (readRet == NO_REQUEST) {
        modfd(epollfd, HttpConn::sockFd, EPOLLIN, TrigMode);
        return;
    }
    bool writeRet = processWrite(readRet);
    if (!writeRet) {
        closeConn();
    }
    modfd(epollfd, sockFd, EPOLLOUT, TrigMode);
}