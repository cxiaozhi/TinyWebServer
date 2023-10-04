#pragma once

#include "locker.h"
#include "sqlConnectionPool.h"
#include <cstdio>
#include <exception>
#include <list>
#include <pthread.h>

template <typename T> class Threadpool {
private:
    static void* worker(void* arg);
    void run();

private:
    int threadNumber;   // 线程池中的线程数
    int maxRequests;    // 请求队列中允许的最大请求数
    pthread_t* threads; // 描述线程池的数组 其大小为threadNumber
    std::list<T*> workQueue;  // 请求队列
    Locker queueLocker;       // 保护请求队列的互斥锁
    Sem queueStat;            // 是否有任务需要处理
    ConnectionPool* connPool; // 数据库
    int actorModel;           // 模型切换
public:
    Threadpool(int actorModel, ConnectionPool* connPool, int threadNumber = 8,
               int maxRequest = 10000);
    ~Threadpool();

    bool append(T* request, int state);
    bool appendP(T* request);
};

template <typename T>
Threadpool<T>::Threadpool(int actorModel, ConnectionPool* connPool,
                          int threadNumber, int maxRequest)
    : Threadpool::actorModel(actorModel), Threadpool::maxRequests(maxRequest),
      Threadpool::threads(NULL), Threadpool::connPool(connPool) {

    if (threadNumber <= 0 || maxRequest < = 0) {
        throw std::exception();
    }

    Threadpool::threads = new pthread_t[Threadpool::threadNumber];

    if (!Threadpool::threads)
        throw std::exception();
    for (int i = 0; i < Threadpool::threadNumber; i++) {
        if (pthread_create(Threadpool::threads + i, NULL, worker, this) != 0) {
            delete[] Threadpool::threads;
            throw std::exception();
        }

        if (pthread_detach(Threadpool::threads[i])) {
            delete[] Threadpool::threads;
            throw std::exception();
        }
    }
}

template <typename T> Threadpool<T>::~Threadpool() {
    delete[] Threadpool::threads;
}

template <typename T> bool Threadpool<T>::append(T* request, int state) {
    Threadpool::queueLocker.lock();
    if (Threadpool::size() >= Threadpool::maxRequests) {
        Threadpool::queueLocker.unlock();
        return false;
    }
    request->state = state;
    Threadpool::workQueue.push_back(request);
    Threadpool::queueStat.post();
    return true;
}

template <typename T> bool Threadpool<T>::appendP(T* request) {
    Threadpool::queueLocker.lock();
    if (Threadpool::workQueue.size() >= Threadpool::maxRequests) {
        Threadpool::queueLocker.unlock();
        Threadpool::queueStat.post();
        return true;
    }
}

template <typename T> void* Threadpool<T>::worker(void* arg) {
    Threadpool* pool = (Threadpool*)arg;
    pool->run();
    return pool;
}

template <typename T> void Threadpool<T>::run() {
    while (true) {
        Threadpool::queueStat.wait();
        Threadpool::queueLocker.lock();
        if (Threadpool::workQueue.empty()) {
            Threadpool::queueLocker.unlock();
            continue;
        }
        T* request = Threadpool::workQueue.front();
        Threadpool::workQueue.pop_front();
        Threadpool::queueLocker.unlock();
        if (!request) {
            continue;
        }

        if (1 == Threadpool::actorModel) {
            if (0 == request->state) {
                if (request->readOnce()) {
                    request->improv = 1;
                    ConnectionRaii mysqlCon(&request->mysql,
                                            Threadpool::connPool);
                    request->process();
                } else {
                    request->improv = 1;
                    request->timerFlag = 1;
                }
            } else {
                if (request->write()) {
                    request->improv = 1;
                } else {
                    request->improv = 1;
                    request->timerFlag = 1;
                }
            }
        } else {
            ConnectionRaii mysqlcon(&request->mysql, Threadpool::connPool);
            request->process();
        }
    }
}