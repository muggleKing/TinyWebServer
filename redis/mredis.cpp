//
// Created by Eddie on 2020/12/19.
//

#include "mredis.h"
#include <map>

redis_clt *redis_clt::m_redis_instance = new redis_clt();
redis_clt *redis_clt::getInstance() {
    return m_redis_instance;
}
redis_clt::redis_clt() {
    timeout = {2, 0};
    m_redisContext = (redisContext *)redisConnectWithTimeout("127.0.0.1", 6379, timeout);
    board_init();
}
void redis_clt::board_init() {
    string board;
    board = getReply("EXISTS GOT");
    //table not exist
    if(board!="1"){
        getReply("DEL GOT");
        getReply("zadd GOT 0 JohnSnow");
        getReply("zadd GOT 0 JaimeLannister");
        getReply("zadd GOT 0 NedStark");
        getReply("zadd GOT 0 TyrionLannister");
        getReply("zadd GOT 0 DaenerysTargaryen");
        getReply("zadd GOT 0 AryaStark");
        cout << "init ok" << endl;
    }
}
string redis_clt::getvoteboard() {
    string board;
    board = getReply("zrange GOT 0 - 1 withscores");
    if(board[0] == '{' && board[board.length()-1] == '}'){
        return board;
    }else{
        return "";
    }
}
string redis_clt::getReply(string m_command) {
    m_redis_lock.lock();
    m_redisReply = (redisReply *)redisCommand(m_redisContext, m_command.c_str());
    string temp = "";
    if(m_redisReply->element == 0 && m_redisReply->type == REDIS_REPLY_STRING ){
        temp = string(m_redisReply->str);
    }else if(m_redisReply->type == REDIS_REPLY_INTEGER){
        int tmpcode = m_redisReply->integer;
        temp = to_string(tmpcode);
    }else{
        //for post
        temp+="{";
        for(int i=0;i<m_redisReply->elements;++i){
            temp+="\"";
            temp+=string(m_redisReply->element[i]->str);
            temp+="\"";
            if(i%2 == 0)
                temp+=":";
            else
                temp+=",";
        }
        temp.pop_back();
        temp+="}";
    }
    freeReplyObject(m_redisReply);
    m_redis_lock.unlock();
    return temp;
}

