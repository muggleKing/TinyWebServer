//
// Created by Eddie on 2020/12/6.
//

#include "threadpool.h"

template <typename T>
pool<T>::pool(int num, int max_tasknum) : m_threadnum(num), max_task_num(max_tasknum) {
    if(num<=0 || max_tasknum<=0){
        throw std::exception();
    }

    m_pthread = new pthread_t[num];
    assert(m_pthread!= nullptr);

    for(int i=0;i<num;i++){
        /*这样写不大好
        int ret = pthread_create(pthread + i , nullptr, worker, this);
        assert(ret == 0);
        */
        if(pthread_create(m_pthread + i , nullptr, worker, this)!=0){
            delete [] m_pthread;
            throw std::exception();
        }

        if(pthread_detach(m_pthread+i)==0){
            delete [] m_pthread;
            throw std::exception();
        }
    }
}

template <typename T>
void pool<T>::run() {
    while(!m_stop){
        m_lock.lock();
        m_sem.wait();
        if(m_queue.empty()){
            m_lock.unlock();
            continue;
        }
        auto task = m_queue.front();
        m_queue.pop_front();
        m_lock.unlock();
        if(!task){
            continue;
        }
        task->process();
    }
}

template <typename T>
static void* pool<T>::worker(void *arg) {
    pool* threadpool = (pool*)arg;
    assert(threadpool!= nullptr);
    threadpool->run();
    return threadpool;
}

template <typename T>
bool pool<T>::append(T* task) {
    m_lock.lock();
    if(m_queue.size() >= max_task_num){
        m_lock.unlock();
        return false;
    }
    m_queue.push_back(task);
    //先unlock再post啊
    m_lock.unlock();
    m_sem.post();
    return true;
}


