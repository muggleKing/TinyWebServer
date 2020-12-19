//
// Created by Eddie on 2020/12/6.
//

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/uio.h>
#include "locker.h"
#include "threadPool.h"
#include <string>
#include <map>
#include <unordered_map>

using namespace std;

class http_conn //http 连接类
{
public:
    static const int BUFF_READ_SIZE = 2048;
    static const int BUFF_WRITE_SIZE = 2048;
    enum HTTP_CODE
    {
        NO_REQUEST,
        GET_REQUEST,
        POST_REQUEST,
        NO_RESOURCE,
        CLOSED_CONNECTION
    };
public:
    http_conn(){};
    ~http_conn(){};
public:
    void init(int socketfd, const sockaddr_in &addr); //初始化套接字
    void init();                                      //实现具体各个参数值的初始化
    void close_conn(string msg = "");                 //关闭连接
    void process();                                   //处理请求
    bool read();                                      //一次性调用recv读取所有数据，读取浏览器发来的全部数据，读到读缓冲区,返回调用是否称成功的信息
    bool write();
public:
    static int m_epollfd; //当前http连接的epoll描述符,这个是静态的
    static int m_user_count;
private:
    HTTP_CODE process_read();          //从读缓冲区读取出来数据进行解析
    bool process_write(HTTP_CODE ret); //写入响应到写缓冲区中
    void parser_header(const string &text);      //解析请求的内容
    void parser_requestline(const string &text); //解析请求的第一行
    void parser_postinfo(const string &text);    //解析post请求正文
    bool do_request(); //确定到底请求的是哪一个页面
    void unmap();
private:
    locker m_redis_lock;
    int m_socket; //当前属于这个http连接的套接字
    sockaddr_in m_addr;
    struct stat m_file_stat;
    struct iovec m_iovec[2];
    int m_iovec_length;
    string filename;
    string postmsg;
    char *file_addr;
    char post_temp[BUFSIZ];
    char read_buff[BUFF_READ_SIZE];   //每个http连接都有一个读缓冲区和写缓冲区
    char write_buff[BUFF_WRITE_SIZE]; //每个http连接都有一个读缓冲区和写缓冲区
    int read_for_now = 0;
    int write_for_now = 0;
    map<string, string> m_map; //http连接的各项属性
};

int http_conn::m_user_count = 0;
int http_conn::m_epollfd = -1;
static int setnonblocking(int fd){
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}
static void addfd(int epollfd, int fd, bool one_shot){
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    if(one_shot){
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}
static void removefd(int epollfd, int fd){
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}
static void modfd(int epollfd, int fd, int ev){
    epoll_event events;
    events.data.fd = fd;
    events.events = ev|EPOLLIN | EPOLLET | EPOLLRDHUP;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &events);
}

