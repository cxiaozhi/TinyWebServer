#pragma once
#include "blockQueue.h"
#include <iostream>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <string>

using namespace std;

class Log {
private:
    Log(/* args */);
    ~Log();

public:
    static Log* get_instance() {
        static Log instance;
        return &instance;
    }
};
