//
// Created by Eddie on 2020/11/27.
//

#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <locker.h>
#include <http_conn.h>
#include <threadPool.h>
#include <util_timer.h>
#include <mredis.h>
#include <vector>
#define MAX_FD 65536
#define MAX_EVENT_NUMBER 10000
#define TIMESLOT 5
static int pipefd[2];
static sort_timer_list timer_lst;
static int epollfd = -1;
template <typename T>
void destroy_user_list(T user_list){
    int count = 0;
    for(auto i : user_list){
        if(i){
            count++;
            delete i;
        }
    }
    cout<< "delete" <<count<< " items"<<endl;
}
void addsig(int sig, void(handler)(int), bool restart = true){
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if(restart)
        sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    sigaction(sig, &sa, nullptr);
}
void sig_handler(int sig){
    int old_err = errno;
    int msg = sig;
    send(pipefd[1], (char*)&msg, 1, 0);
    errno = old_err;
}

void timer_handler(){
    timer_lst.tick();
    alarm(TIMESLOT);
}
void cb_func(client_data* user_data){
    epoll_ctl(epollfd, EPOLL_CTL_DEL, user_data->socketfd, 0);
    close(user_data->socketfd);
}

int main(int argc, char** argv) {
    if(argc <= 2){
        printf("usage: %s ip port\n", basename(argv[0]));
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);
    addsig(SIGPIPE, SIG_IGN);
    threadpool<http_conn>* pool = new threadpool<http_conn>();
    assert(pool);
    /*最多的连接数是65536*/
    http_conn* users= new http_conn[MAX_FD];
    assert(users);
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(listenfd>0);
    /*inet_pton htons 转换成网络字节序*/
    struct sockaddr_in sockaddrIn;
    bzero(&sockaddrIn, sizeof(sockaddrIn));
    sockaddrIn.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &sockaddrIn.sin_addr);
    sockaddrIn.sin_port = htons(port);
    socklen_t socklen = sizeof(sockaddrIn);
    int ret = bind(listenfd, (sockaddr*)&sockaddrIn, socklen);
    assert(ret>0);
    ret = listen(listenfd, 8);
    assert(ret>=0);
    epoll_event event[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(8);
    http_conn::m_epollfd = epollfd;
    addfd(epollfd, listenfd, true);
    socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    setnonblocking(pipefd[1]);
    addfd(epollfd, pipefd[0], true);
    addsig(SIGALRM, sig_handler, false);
    addsig(SIGTERM, sig_handler, false);
    addsig(SIGINT, sig_handler, false);
    vector<client_data*> client_data_list(MAX_FD, nullptr);
    alarm(TIMESLOT);
    epoll_event event_list[MAX_EVENT_NUMBER];
    bool timeout = false;
    bool stop_server = false;
    while(!stop_server){
        int num = epoll_wait(epollfd, event_list, MAX_EVENT_NUMBER, -1);
        for(int i=0;i<num;i++){
            int socketfd = event_list[i].data.fd;
            //new conn is coming
            if(socketfd == listenfd){
                struct sockaddr_in client_addr;
                socklen_t addr_len = sizeof(client_addr);
                int connfd = accept(listenfd, (sockaddr*)&client_addr, &addr_len);
                if(connfd<0){
                    continue;
                }
                if(http_conn::m_user_count >= MAX_FD){
                    continue;
                }
                users[connfd].init(connfd, client_addr);
                if(client_data_list[connfd] == nullptr){
                    client_data_list[connfd] = new client_data(client_addr, socketfd);
                    util_timer* timer = new util_timer(time(nullptr)+TIMESLOT, client_data_list[connfd], cb_func);
                    client_data_list[connfd]->timer = timer;
                    timer_lst.add_timer(timer);
                }
            }
                //sth wrong with client or server
            else if(event_list[i].events & (EPOLLHUP | EPOLLRDHUP | EPOLLERR)){
                users[socketfd].close_conn();
                auto timer = client_data_list[socketfd]->timer;
                timer_lst.del_timer(timer);
            }
                //get a signal from pipe
            else if(socketfd == pipefd[0] && (event_list[i].events & EPOLLIN)){
                int sig;
                char signals[1024];
                int retmsg = recv(socketfd, signals, sizeof(signals), 0);
                if(retmsg <= 0){
                    continue;
                }
                else{
                    for(int i=0;i<retmsg;i++){
                        if(signals[i] == SIGALRM){
                            timeout = true;
                            break;
                        }
                        else if(signals[i] == SIGALRM || signals[i] == SIGINT){
                            stop_server = true;
                        }
                    }
                }
            }
                //get sth from socket read buffer
            else if(event_list[i].events & EPOLLIN){
                auto timer = client_data_list[socketfd]->timer;
                //main thread is reading
                if(users[socketfd].read()){
                    pool->append(&users[socketfd]);
                    if(timer){
                        timer->expire = time(nullptr) + TIMESLOT;
                        timer_lst.adjust_timer(timer);
                    }
                    else{
                        cout<< "did not create timer for socket" <<socketfd << endl;
                    }
                }
                else{
                    users[socketfd].close_conn();
                    if(timer)
                        timer_lst.del_timer(timer);
                }
            }
                //socket writing buffer is ready for writing
            else if(event_list[i].events & EPOLLOUT){
                auto timer = client_data_list[socketfd]->timer;
                //main thread is writing
                if(users[socketfd].write()){
                    if(timer){
                        timer->expire = time(nullptr) +TIMESLOT;
                        timer_lst.adjust_timer(timer);
                    }else{
                        cout<<"did not create timer for socket" <<socketfd << endl;
                    }
                }
                else{
                    users[socketfd].close_conn();
                    if(timer)
                        timer_lst.del_timer(timer);
                }
            }
            if(timeout){
                timer_handler();
                timeout = false;
            }
        }
    }
    close(epollfd);
    close(listenfd);
    delete[] users;
    destroy_user_list<vector<client_data*>>(client_data_list);
    delete pool;
    return 0;
}
