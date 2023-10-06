#include "../../include/CGIMysql/sqlConnectionPool.h"

#include <error.h>
#include <mysql/mysql.h>
#include <stdio.h>
#include <string.h>

#include <iostream>
#include <list>
#include <string>

#include "../../include/lock/locker.h"
#include "../../include/log/log.h"
using namespace std;

ConnectionPool::ConnectionPool() {
    m_MaxConn = 0;
    m_FreeConn = 0;
}

ConnectionPool* ConnectionPool::GetInstance() {
    static ConnectionPool connPool;
    return &connPool;
}

// 构造初始化
void ConnectionPool::init(string url, string User, string PassWord,
                          string DataBaseNmae, int Port, int MaxConn,
                          int close_log) {
    mUrl = url;
    mPort = Port;
    mUser = User;
    mPassWord = PassWord;
    mDatabaseName = DataBaseNmae;
    closeLogData = close_log;

    for (int i = 0; i < MaxConn; i++) {
        MYSQL* con = NULL;
        con = mysql_init(con);
        if (con == NULL) {
            LOG_ERROR("数据库错误");
            exit(1);
        }
        con =
            mysql_real_connect(con, url.c_str(), User.c_str(), PassWord.c_str(),
                               DataBaseNmae.c_str(), Port, NULL, 0);
        if (con == NULL) {
            LOG_ERROR("数据库错误");
            exit(1);
        }
        connList.push_back(con);
        m_FreeConn++;
    }
    reserve = Sem(m_FreeConn);
    m_MaxConn = m_FreeConn;
}

// 当有请求时 从数据库连接池中返回一个可用连接 更新使用和空闲连接数
MYSQL* ConnectionPool::GetConnection() {
    MYSQL* con = NULL;
    if (0 == connList.size()) return NULL;
    reserve.wait();
    lock.lock();
    con = connList.front();
    connList.pop_front();
    m_FreeConn--;
    m_CurConn++;
    lock.unlock();
    return con;
}

// 释放当前使用的连接
bool ConnectionPool::ReleaseConnection(MYSQL* con) {
    if (NULL == con) {
        return false;
    }

    lock.lock();
    connList.push_back(con);
    m_FreeConn++;
    m_CurConn--;
    lock.unlock();
    reserve.post();
    return true;
}

// 销毁数据库连接池
void ConnectionPool::DestroyPool() {
    lock.lock();
    if (connList.size() > 0) {
        list<MYSQL*>::iterator it;
        for (it = connList.begin(); it != connList.end(); it++) {
            MYSQL* con = *it;
            mysql_close(con);
        }
        m_CurConn = 0;
        m_FreeConn = 0;
        connList.clear();
    }

    lock.unlock();
}

// 当前空闲连接数
int ConnectionPool::GetFreeConn() {
    return this->m_FreeConn;
}

ConnectionPool::~ConnectionPool() {
    DestroyPool();
}

ConnectionRaii::ConnectionRaii(MYSQL** sql, ConnectionPool* connPool) {
    *sql = connPool->GetConnection();
    conRaii = *sql;
    poolRaii = connPool;
}

ConnectionRaii::~ConnectionRaii() {
    poolRaii->ReleaseConnection(conRaii);
}