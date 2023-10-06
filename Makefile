CXX ?= g++

DEBUG ?= 1

# PWDPATH = $(shell pwd)
ifeq ($(DEBUG),1)
	CXXFLAGS += -g
else
	CXXFLAGS += -O2

endif

server:main.cpp ./src/timer/lstTimer.cpp ./src/CGIMysql/sqlConnectionPool.cpp ./src/config/default.config.cpp ./src/http/httpConn.cpp ./src/lock/locker.cpp ./src/log/log.cpp ./src/webserver/webserver.cpp 
	$(CXX) -o server $^ $(CXXFLAGS) -lpthread -lmysqlclient

clean:
	rm -r server