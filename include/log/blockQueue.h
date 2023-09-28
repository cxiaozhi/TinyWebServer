#pragma once

#include "locker.h"
#include <iostream>
#include <pthread.h>
#include <stdlib.h>
#include <sys/time.h>

using namespace std;

template <class T> class BlockQueue {
private:
    Locker m_mutex;
    Cond m_cond;
    T* m_array;
    int m_size;
    int m_max_size;
    int m_back;
    int m_front;

public:
    BlockQueue(int max_size = 1000) {
        if (max_size <= 0) {
            exit(-1);
        }
        m_max_size = max_size;
        m_array = new T[max_size];
        m_size = 0;
        m_front = -1;
        m_back = -1;
    }

    void clear() {
        m_mutex.lock();
        m_size = 0;
        m_front = -1;
        m_back = -1;
        m_mutex.unlock();
    }
    ~BlockQueue() {
        m_mutex.lock();
        if (m_array != NULL) {
            delete[] m_array;
        }

        m_mutex.unlock();
    }

    // 判断队列是否满了
    bool full() {
        m_mutex.lock();
        if (m_size >= m_max_size) {
            m_mutex.unlock();
            return true;
        }
        m_mutex.unlock();
        return false;
    }

    // 判断队列是否为空
    bool empty() {
        m_mutex.lock();
        if (0 == m_size) {
            m_mutex.unlock();
            return true;
        }
        m_mutex.unlock();
        return false;
    }

    // 返回队首元素
    bool front(T& value) {
        m_mutex.lock();
        if (0 == m_size) {
            m_mutex.unlock();
            return false;
        }

        value = m_array[m_front];
        m_mutex.unlock();
        return true;
    }

    // 返回队尾元素
    bool back(T& value) {
        m_mutex.lock();
        if (0 == m_size) {
            m_mutex.unlock();
            return false;
        }
        value = m_array[m_back];
        m_mutex.unlock();
        return true;
    }

    int size() {
        int tmp = 0;
        m_mutex.lock();
        tmp = m_size;
        m_mutex.unlock();
        return tmp;
    }

    int max_size() {
        int tmp = 0;
        m_mutex.lock();
        tmp = m_max_size;

        m_mutex.unlock();
        return tmp;
    }

    // 往队列添加元素 需要将所有使用队列队线程先唤醒
    // 若当前没有线程等待条件变量 则唤醒无意义
    bool push(const T& item) {
        m_mutex.lock();
        if (m_size >= m_max_size) {
            m_cond.broadCast();
            m_mutex.unlock();
            return false;
        }

        m_back = (m_back + 1) % m_max_size;
        m_array[m_back] = item;
        m_size++;
        m_cond.broadCast();
        m_mutex.unlock();
        return true;
    }

    // 弹出元素
    bool pop(T& item) {
        m_mutex.lock();
        while (m_size <= 0) {
            if (!m_cond.wait(m_mutex.get())) {
                m_mutex.unlock();
                return false;
            }
        }

        m_front = (m_front + 1) % m_max_size;
        item = m_array[m_front];
        m_size--;
        m_mutex.unlock();
        return true;
    }

    // 超时处理
    bool pop(T& item, int ms_timeout) {
        struct timespec timespec = {0, 0};
        struct timeval now = {0, 0};
        gettimeofday(&now, NULL);
        m_mutex.lock();
        if (m_size <= 0) {
            timespec.tv_sec = now.tv_sec + ms_timeout / 1000;
            timespec.tv_nsec = (ms_timeout % 1000) * 1000;
            if (!m_cond.timeWait(m_mutex.get(), timespec)) {
                m_mutex.unlock();
                return false;
            }
        }

        m_front = (m_front + 1) % m_max_size;
        item = m_array[m_front];
        m_size--;
        m_mutex.unlock();
        return true;
    }
};
