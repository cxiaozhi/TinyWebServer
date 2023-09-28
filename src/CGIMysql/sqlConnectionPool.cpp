#include "sqlConnectionPool.h"
#include "locker.h"
#include <error.h>
#include <iostream>
#include <list>
#include <mysql/mysql.h>
#include <stdio.h>
#include <string.h>
#include <string>

using namespace std;

ConnectionPool::ConnectionPool() {
    m_MaxConn = 0;
    m_FreeConn = 0;
}

ConnectionPool* ConnectionPool::GetInstance() {
    static ConnectionPool connPool;
    return &connPool;
}

// 构造i初始化
void ConnectionPool::init(string url, string User, string PassWord,
                          string DataBaseNmae, int Port, int MaxConn,
                          int close_log) {
    mUrl = url;
    mPort = Port;
    mUser = User;
    mPassWord = PassWord;
    mDatabaseName = DataBaseNmae;
    mCloseLog = close_log;

    for (int i = 0; i < MaxConn; i++) {
        MYSQL* con = NULL;
        con = mysql_init(con);
        if (con == NULL) {
        }
    }
}