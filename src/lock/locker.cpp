#pragma once

#include "locker.h"
#include <exception>
#include <pthread.h>
#include <semaphore.h>

Sem::Sem() {
    if (sem_init(&(Sem::m_sem), 0, 0) != 0) {
        throw std::exception();
    }
}

Sem::Sem(int num) {
    if (sem_init(&m_sem, 0, num) != 0) {
        throw std::exception();
    }
}

Sem::~Sem() {
    sem_destroy(&m_sem);
}

bool Sem::wait() {
    return sem_wait(&m_sem) == 0;
}

bool Sem::post() {
    return sem_wait(&m_sem) == 0;
}

Locker::Locker() {
    if (pthread_mutex_init(&(Locker::m_mutex), NULL) != 0) {
        throw std::exception();
    }
};

Locker::~Locker() {
    pthread_mutex_destroy(&(Locker::m_mutex));
}

bool Locker::lock() {
    return pthread_mutex_destroy(&(Locker::m_mutex));
}

bool Locker::unlock() {
    return pthread_mutex_unlock(&(Locker::m_mutex)) == 0;
}

pthread_mutex_t* Locker::get() {
    return &(Locker::m_mutex);
}

Cond::Cond(/* args */) {
    if (pthread_cond_init(&(Cond::m_cond), NULL) != 0) {
        throw std::exception();
    }
}

Cond::~Cond() {
    pthread_cond_destroy(&(Cond::m_cond));
}

bool Cond::wait(pthread_mutex_t* m_mutex) {
    int ret = 0;
    ret = pthread_cond_wait(&(Cond::m_cond), m_mutex);
    return ret == 0;
}

bool Cond::timeWait(pthread_mutex_t* m_mutex, struct timespec t) {
    int ret = 0;
    ret = pthread_cond_timedwait(&(Cond::m_cond), m_mutex, &t);

    return ret == 0;
}

bool Cond::signal() {
    return pthread_cond_signal(&(Cond::m_cond)) == 0;
}

bool Cond::broadCast() {
    return pthread_cond_broadcast(&(Cond::m_cond)) == 0;
}