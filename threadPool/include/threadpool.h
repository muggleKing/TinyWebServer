//
// Created by Eddie on 2020/12/6.
//

#ifndef HTTP_SERVER_THREADPOOL_H
#define HTTP_SERVER_THREADPOOL_H

#include "locker.h"
#include <iostream>
#include <list>
#include <pthread.h>
#include <vector>
using namespace std;

template <typename T>
class pool{
public:
    pool(int num = 10, int max_tasknum = 100);
    bool append(T* task);

public:
    void run();
    static void* worker(void* arg);

private:
    int m_threadnum;
    int max_task_num;
    list<T*> m_queue;
    locker m_lock;
    sem_t m_sem;
    pthread_t* m_pthread{nullptr};
    //这个忘了
    bool m_stop{false};
};
#endif //HTTP_SERVER_THREADPOOL_H
