//
// Created by Eddie on 2020/12/19.
//

#ifndef HTTP_SERVER_MREDIS_H
#define HTTP_SERVER_MREDIS_H

#include <hiredis/hiredis.h>
#include <iostream>
#include <string>
#include "locker.h"

using namespace std;

class redis_clt
{
private:
    locker m_redis_lock;
    static redis_clt *m_redis_instance;
    struct timeval timeout;
    redisContext *m_redisContext{nullptr};
    redisReply *m_redisReply{nullptr};
private:
    string getReply(string m_command);
    redis_clt();
public:
    string setUserpasswd(string username, string passwd){
        return getReply("set " + username + " " + passwd);
    }
    string getUserpasswd(string username)
    {
        return getReply("get " + username);
    }
    void vote(string votename)
    {
        if (votename.empty())
            return;
        string temp = getReply("ZINCRBY GOT 1 " + votename);
    }
    string getvoteboard();
    void board_init();
    static redis_clt *getInstance();
};


#endif //HTTP_SERVER_MREDIS_H
