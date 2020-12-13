//
// Created by Eddie on 2020/12/6.
//

#include "http_conn.h"

//写的基本都对 除了modfd 这里读完后会在process_read里面统一操作
bool http_conn::read() {
    if(m_read_idx >= READ_BUFFER_SIZE){
        return false;
    }

    while(1){
        int temp = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        if(temp == 0){
            //连接可能关闭了
            return false;
        }else if(temp == -1){
            if(errno == EAGAIN){
//                modfd(m_epollfd, m_sockfd, EPOLLIN);
                return true;
            }
            return false;
        }
        m_read_idx += temp;
    }
}

//write和read不一样的地方是 这是socket的最后一步 涉及到是否关闭连接
bool http_conn::write() {
    int tmp = 0;
    int bytes_have_read = 0;
    int bytes_to_send = m_write_idx;

    if(m_write_idx == 0){
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        init();//forget
        return false;
    }

    while(1){
        tmp = writev(m_sockfd, m_iv, m_iv_count);

        if(tmp < 0){
            if(errno == EAGAIN){
                modfd(m_epollfd, m_sockfd, EPOLLOUT);
                return true;
            }
            unmap();//forget
            return false;
        }
        if(tmp == 0){

        }
        bytes_have_read += tmp;
        bytes_to_send -= tmp;

        if(bytes_to_send <= 0){
            unmap();
            modfd(m_epollfd, m_sockfd, EPOLLIN);
            //false导致删除定时器 关闭socketfd
            if(m_linger){
                init();
                return true;//forget
            }else{
                return false;//forget
            }
        }

    }
}