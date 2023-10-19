#include "../../include/log/log.h"

#include <pthread.h>
#include <stdarg.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

using namespace std;

Log::Log() {
    logLineCount = 0;
    isAsync = false;
}

Log::~Log() {
    if (logFileFp != NULL) {
        fclose(logFileFp);
    }
}

// 异步需要设置阻塞队列的长度 同步不需要设置
bool Log::init(const char* fileName, int closeLog, int logBufSize,
               int splitLines, int maxQueueSize) {
    if (maxQueueSize >= 1) {
        isAsync = true;
        logQueue = new BlockQueue<string>(maxQueueSize);
        pthread_t tid;

        pthread_create(&tid, NULL, flush_log_thread, NULL);
    }

    Log::closeLogData = closeLog;
    Log::logBufSize = logBufSize;
    Log::buf = new char[logBufSize];
    memset(Log::buf, '\0', Log::logBufSize);
    Log::splitLines = splitLines;

    time_t _time = time(NULL);

    struct tm* sysTime = localtime(&_time);
    struct tm myTime = *sysTime;

    const char* p = strrchr(fileName, '/');
    char logFullName[256] = {0};

    if (p == NULL) {
        snprintf(logFullName, 255, "%d_%02d_%02d_%s", myTime.tm_year + 1900,
                 myTime.tm_mon + 1, myTime.tm_mday, fileName);
    } else {
        strcpy(Log::log_name, p + 1);
        strncpy(Log::dir_name, fileName, p - fileName + 1);
        snprintf(logFullName, 255, "%s%d_%02d_%02d_%s", Log::dir_name,
                 myTime.tm_year + 1900, myTime.tm_mon + 1, myTime.tm_mday,
                 Log::log_name);
    }

    Log::todayDate = myTime.tm_mday;

    Log::logFileFp = fopen(logFullName, "a");
    if (Log::logFileFp == NULL) {
        return false;
    }

    return true;
}

void Log::writeLog(int level, const char* format, ...) {
    struct timeval nowTime = {0, 0};
    gettimeofday(&nowTime, NULL);
    time_t timeSec = nowTime.tv_sec;
    struct tm* sysTime = localtime(&timeSec);
    struct tm mytime = *sysTime;
    char str[16] = {0};

    switch (level) {
        case 0:
            strcpy(str, "[debug]:");
            break;
        case 1:
            strcpy(str, "[info]:");
            break;
        case 2:
            strcpy(str, "[warn]:");
            break;
        case 3:
            strcpy(str, "[error]:");
            break;
        default:
            strcpy(str, "[info]:");
            break;
    }

    // 写一个log 对m_count++ m_split_lines 最大行数
    Log::mutexLock.lock();
    Log::logLineCount++;

    if (todayDate != mytime.tm_mday ||
        Log::logLineCount % Log::splitLines == 0) {
        char new_log[256] = {0};
        fflush(Log::logFileFp);
        fclose(Log::logFileFp);
        char tail[16] = {0};

        snprintf(tail, 16, "%d_%02d_%02d_", mytime.tm_year + 1900,
                 mytime.tm_mon + 1, mytime.tm_mday);
        if (Log::todayDate != mytime.tm_mday) {
            snprintf(new_log, 255, "%s%s%s", dir_name, tail, log_name);
            Log::todayDate = mytime.tm_mday;
            Log::logLineCount = 0;
        } else {
            snprintf(new_log, 255, "%s%s%s.%lld", dir_name, tail, log_name,
                     Log::logLineCount / Log::splitLines);
        }
        Log::logFileFp = fopen(new_log, "a");
    }

    Log::mutexLock.unlock();

    va_list valst;
    va_start(valst, format);

    string log_str;
    Log::mutexLock.lock();

    // 写入的具体时间内容格式
    int n = snprintf(Log::buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s",
                     mytime.tm_year + 1900, mytime.tm_mon + 1, mytime.tm_mday,
                     mytime.tm_hour, mytime.tm_min, mytime.tm_sec,
                     nowTime.tv_usec, str);
    int m = vsnprintf(Log::buf + n, Log::logBufSize - n - 1, format, valst);
    Log::buf[n + m] = '\n';

    Log::buf[n + m + 1] = '\0';
    log_str = Log::buf;

    Log::mutexLock.unlock();
    if (Log::isAsync && !Log::logQueue->full()) {
        Log::logQueue->push(log_str);
    } else {
        Log::mutexLock.lock();
        fputs(log_str.c_str(), Log::logFileFp);
        Log::mutexLock.unlock();
    }
    va_end(valst);
}

void Log::flush(void) {
    Log::mutexLock.lock();

    // 手动强制刷新写入流缓冲区
    fflush(Log::logFileFp);
    Log::mutexLock.unlock();
}