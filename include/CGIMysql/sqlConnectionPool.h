#pragma once

#include <error.h>
#include <iostream>
#include <list>
#include <mysql/mysql.h>
#include <stdio.h>
#include <string>

#include "locker.h"

using namespace std;

class ConnectionPool {

private:
    ConnectionPool(/* args */);
    ~ConnectionPool();

    int m_MaxConn;  // 最大连接数
    int m_CurConn;  // 当前已使用的连接数
    int m_FreeConn; //当前空闲的连接数

    Locker lock;
    list<MYSQL*> connList; // 连接池
    Sem reserve;

public:
    string mUrl;          //主机地址
    string mPort;         // 数据库端口号
    string mUser;         // 登录数据库用户名
    string mPassWord;     // 登录数据库密码
    string mDatabaseName; // 使用数据库名
    int mCloseLog;        // 日志开关

    MYSQL* GetConnection();              // 获取数据库连接
    bool ReleaseConnection(MYSQL* conn); // 释放连接
    int GetFreeConn();                   // 获取连接
    void DestroyPool();                  // 销毁所有连接

    // 单例模式
    static ConnectionPool* GetInstance();

    void init(string url, string User, string PassWord, string DataBaseNmae,
              int Port, int MaxConn, int close_log);
};
