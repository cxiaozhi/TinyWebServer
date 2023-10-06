#pragma once
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>

#include <iostream>
#include <string>

#include "blockQueue.h"
using namespace std;

class Log {
private:
    Log();
    virtual ~Log();
    char dir_name[128];      // 路径名
    char log_name[128];      // logn文件名
    int splitLines;          // 日志最大行数
    int logBufSize;          // 日志缓冲区大小
    long long logLineCount;  // 日志行数记录
    int todayDate;           // 记录当前时间是那一天
    FILE* logFileFp;         // 打开log的文件指针
    char* buf;
    BlockQueue<string>* logQueue;  // 阻塞队列
    bool isAsync;                  // 是否同步标志位
    Locker mutexLock;              // 互斥锁
    int closeLog;                  // 关闭日志

private:
    void* asyncWriteLog() {
        string singleLog;
        while (logQueue->pop(singleLog)) {
            mutexLock.lock();
            fputs(singleLog.c_str(), logFileFp);
            mutexLock.unlock();
        }
    }

public:
    static Log* get_instance() {
        static Log instance;
        return &instance;
    }

    static void* flush_log_thread(void* args) {
        Log::get_instance()->asyncWriteLog();
    }

    bool init(const char* fileName, int closeLog, int logBufSize = 8192,
              int splitLines = 5000000, int maxQueueSize = 0);
    void writeLog(int level, const char* format, ...);
    void flush(void);
};

#define LOG_DEBUG(format, ...)                                   \
    if (0 == closeLogData) {                                     \
        Log::get_instance()->writeLog(0, format, ##__VA_ARGS__); \
        Log::get_instance()->flush();                            \
    }

#define LOG_INFO(format, ...)                                    \
    if (0 == closeLogData) {                                     \
        Log::get_instance()->writeLog(1, format, ##__VA_ARGS__); \
        Log::get_instance()->flush();                            \
    }

#define LOG_WARN(format, ...)                                    \
    if (0 == closeLogData) {                                     \
        Log::get_instance()->writeLog(2, format, ##__VA_ARGS__); \
        Log::get_instance()->flush();                            \
    }

#define LOG_ERROR(format, ...)                                   \
    if (0 == closeLogData) {                                     \
        Log::get_instance()->writeLog(3, format, ##__VA_ARGS__); \
        Log::get_instance()->flush();                            \
    }
