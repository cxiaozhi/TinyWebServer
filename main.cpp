#include "include/config/default.config.h"

using namespace std;

int main(int argc, char* argv[]) {
    string user = "root";
    string passwd = "root";
    string dataBaseName = "test";

    Config config;  // 命令行解析
    config.parse_arg(argc, argv);

    WebServer server;

    server.init(config.port, user, passwd, dataBaseName, config.logWrite,
                config.optLinger, config.trigMode, config.sqlNum,
                config.threadNum, config.closeLogData,
                config.actorModel);  // 初始化

    server.logWrite();     // 日志
    server.sqlPool();      // 数据库
    server.threadPool();   // 线程池
    server.trigMode();     // 触发模式
    server.eventListen();  // 监听
    server.eventLoop();    // 事件循环

    return 0;
}