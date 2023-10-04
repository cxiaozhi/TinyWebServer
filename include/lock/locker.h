#pragma once

#include <exception>
#include <pthread.h>
#include <semaphore.h>

class Sem {
private:
    sem_t m_sem;

public:
    Sem();
    Sem(int num);
    ~Sem();

    bool wait();

    bool post();
};

class Locker {
private:
    pthread_mutex_t m_mutex;

public:
    Locker();
    ~Locker();

    bool lock();
    bool unlock();

    pthread_mutex_t* get();
};

class Cond {
private:
    pthread_cond_t m_cond;

public:
    Cond();
    ~Cond();

    bool wait(pthread_mutex_t* m_mutex);

    bool timeWait(pthread_mutex_t* m_mutex, struct timespec t);

    bool signal();

    bool broadCast();
};
