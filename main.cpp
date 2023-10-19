#include "include/config/default.config.h"

using namespace std;

int main(int argc, char* argv[]) {
    std::cout << "1. 主程序启动..." << std::endl;
    string user = "ccz";
    string passwd = "ccz";
    string dataBaseName = "test";

    Config config;  // 命令行解析
    config.parse_arg(argc, argv);

    WebServer server;

    server.init(config.port, user, passwd, dataBaseName, config.logWrite,
                config.optLinger, config.trigMode, config.sqlNum,
                config.threadNum, config.closeLogData,
                config.actorModel);  // 初始化
    std::cout << "2. 服务器初始化完成..." << std::endl;

    server.logWrite();  // 日志
    std::cout << "3. 日志系统启动..." << std::endl;

    server.sqlPool();  // 数据库
    std::cout << "3. sql池化完成..." << std::endl;
    server.threadPool();  // 线程池
    std::cout << "4. 线程池启动..." << std::endl;
    server.trigMode();  // 触发模式
    std::cout << "5. 触发模式初始化..." << std::endl;
    server.eventListen();  // 监听
    std::cout << "6. socket监听启动..." << std::endl;
    std::cout << "7. 事件循环开始..." << std::endl;
    server.eventLoop();  // 事件循环

    return 0;
}