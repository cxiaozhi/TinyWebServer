#include "../../include/config/default.config.h"

Config::Config() {
    // 端口号
    port = 9006;
    // 日志写入方式 默认同步
    logWrite = 0;

    // 触发组合模式 默认listenfd
    trigMode = 0;

    // listenfd触发模式
    listenTrigMode = 0;

    // connfd触发模式
    connTrigMode = 0;

    // 优雅关闭连接 默认不使用
    optLinger = 0;

    // 数据库连接池数量 默认8
    sqlNum = 8;

    // 线程池内的线程数量 默认8
    threadNum = 8;

    // 关闭日志 默认不关闭
    closeLogData = 0;

    // 并发模型 默认是proactor
    actorModel = 0;
}

void Config::parse_arg(int argc, char* argv[]) {
    int opt;
    const char* str = "p:l:m:o:s:t:c:a:";
    while ((opt = getopt(argc, argv, str)) != -1) {
        switch (opt) {
            case 'p':
                port = atoi(optarg);
                break;
            case 'l':
                logWrite = atoi(optarg);
                break;
            case 'm':
                trigMode = atoi(optarg);
                break;
            case 'o':
                optLinger = atoi(optarg);
                break;
            case 's':
                sqlNum = atoi(optarg);
                break;
            case 't':
                threadNum = atoi(optarg);
                break;
            case 'c':
                closeLogData = atoi(optarg);
                break;
            case 'a':
                actorModel = atoi(optarg);
                break;
            default:
                break;
        }
    }
}