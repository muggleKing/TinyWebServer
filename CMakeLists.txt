CMAKE_MINIMUM_REQUIRED(VERSION 3.8)
PROJECT(HTTP_SERVER)
INCLUDE_DIRECTORIES(src/http_conn/include)
INCLUDE_DIRECTORIES(src/locker/include/)
INCLUDE_DIRECTORIES(src/threadPool/include/)
INCLUDE_DIRECTORIES(src/timer/include/)

SET(SRC_LIST main.cpp src/threadPool/threadpool.cpp src/http_conn/http_conn.cpp)
ADD_EXECUTABLE(http ${SRC_LIST})
