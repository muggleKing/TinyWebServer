//
// Created by Eddie on 2020/12/8.
//

#ifndef HTTP_SERVER_UTIL_TIMER_H
#define HTTP_SERVER_UTIL_TIMER_H

#include <time.h>
#define BUFFER_SIZE 64

class util_timer;

struct client_data{
    sockaddr_in addr;
    int socketfd;
    util_timer* timer{nullptr};
    char buf[BUFFER_SIZE];
};

class util_timer{
public:
    util_timer(client_data* c, time_t e, void (*cb)(void*)) : user_data(c), cb_func(cb), expire(e){}
public:
    util_timer* pre{nullptr};
    util_timer* next{nullptr};
    client_data* user_data{nullptr};
    time_t expire;
    void (*cb_func)(void*);
};

class sort_timer_list{
public:
    sort_timer_list(){};

    ~sort_timer_list(){
        auto tmp = head;
        while(head){
            head = tmp->next;
            delete tmp;
            tmp = head;
        }
    }

    void add_timer(util_timer* timer){
        if(!timer){
            return;
        }
        if(!head){
            head = tail = timer;
            return;
        }
        if(timer->expire < head->expire){
            timer->next = head;
            head->pre = timer;
            head = timer;
            return;
        }
        add_timer(timer, head);
    }

    void adjust_timer(util_timer* timer){
        if(!timer){
            return;
        }
        util_timer* tmp = timer->next;
        if(!tmp || (timer->expire < tmp->expire)){
            return;
        }
        if(timer == head){
            head = head->next;
            head->pre = nullptr;
            timer->next = nullptr;
            add_timer(timer, head);
        }
        else{
            timer->pre->next = timer->next;
            timer->next->pre = timer->pre;
            add_timer(timer, timer->next);
        }
    }

    void del_timer(util_timer* timer){
        if(!timer){
            return;
        }
        if((head==timer)&&(tail==timer)){
            delete timer;
            head = nullptr;
            tail = nullptr;
            return;
        }
        if(timer == head){
            head = timer->next;
            head->pre = nullptr;
            delete timer;
            return;
        }
        if(timer == tail){
            tail = timer->pre;
            tail->next = nullptr;
            delete timer;
            return;
        }
        timer->pre->next = timer->next;
        timer->next->pre = timer->pre;
        delete timer;
        return;
    }

    void tick(){
        if(!head){
            return;
        }
        printf("time tick\n");
        time_t cur = time(nullptr);
        auto tmp = head;
        while(tmp){
            if(cur < tmp->expire){
                break;
            }
            tmp->cb_func(tmp->user_data);
            head = tmp->next;
            if(head){
                head->pre = nullptr;
            }
            delete tmp;
            tmp = head;
        }
    }


private:
    util_timer* head{nullptr};
    util_timer* tail{nullptr};

private:
    void add_timer(util_timer* timer, util_timer* head){
        auto prev = head;
        auto tmp = head->next;
        while(tmp){
            if(timer->expire < tmp->expire){
                prev->next = timer;
                timer->pre = prev;
                tmp->pre = timer;
                timer->pre = tmp;
                break;
            }
            prev = tmp;
            tmp = prev->next;
        }
        if(!tmp){
            tail->next = timer;
            timer->pre = tail;
            tail = timer;
        }
        return;
    }
};

#endif //HTTP_SERVER_UTIL_TIMER_H
