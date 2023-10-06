#pragma once
#include <iostream>

#include "../webserver/webserver.h"
using namespace std;

class Config {
public:
    Config();
    ~Config(){};

    void parse_arg(int argc, char* argv[]);
    int port;            // 端口号
    int logWrite;        // 日志写入方式
    int trigMode;        // 触发组合模式
    int listenTrigMode;  // listenfd触发模式
    int connTrigMode;    // connfd触发模式
    int optLinger;       // 优雅关闭链接
    int sqlNum;          // 数据库连接池数量
    int threadNum;       // 线程池内的线程数量
    int closeLogData;    // 关闭日志
    int actorModel;      // 并发模型选择
};