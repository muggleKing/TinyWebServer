//
// Created by Eddie on 2020/12/6.
//

#include <http_conn.h>
#include <cstdarg>
#include <mredis.h>

void http_conn::init(int socketfd, const sockaddr_in &addr){
    m_socket = socketfd;
    m_addr = addr;
    addfd(m_epollfd, socketfd, true);
    init();
}
void http_conn::init() {
    filename = "";
    memset(read_buff, '\0', BUFF_READ_SIZE);
    memset(write_buff, '\0', BUFF_WRITE_SIZE);
    read_for_now = 0;
    write_for_now = 0;
}
void http_conn::process() {
    HTTP_CODE ret = process_read();
    if(ret == NO_REQUEST){
        modfd(m_epollfd, m_socket, EPOLLIN);
        return;
    }
    bool result = process_write(ret);
    modfd(m_epollfd, m_socket, EPOLLOUT);
}
/*
 * functions for processing http request message
 */
void http_conn::parser_requestline(const string &text) {
    string m_method = text.substr(0, text.find(" "));
    string m_url = text.substr(text.find_first_of(" ") + 1, text.find_last_of(" ") - text.find_first_of(" ") - 1);
    string m_protocol = text.substr(text.find_last_of(" ") + 1);
    m_map["method"] = m_method;
    m_map["url"] = m_url;
    m_map["protocol"] = m_protocol;
}
void http_conn::parser_header(const string &text) {
    if(text.empty())
        return;
    if(text.find(": ") <= text.size()){
        string m_type = text.substr(0, text.find(": "));
        string m_content = text.substr(text.find(": ") + 2);
        m_map[m_type] = m_content;
        return;
    }
    if(text.find('=') <= text.size()){
        string m_type = text.substr(0, text.find('='));
        string m_content = text.substr(text.find('=') + 2);
        m_map[m_type] = m_content;
        return;
    }
}
void http_conn::parser_postinfo(const string &text) {
    string processd;
    string strleft = text;
    while(true){
        processd = strleft.substr(0, strleft.find('&'));
        m_map[processd.substr(0, processd.find('='))] = processd.substr(processd.find('=') + 1);
        strleft = strleft.substr(strleft.find('&') + 1);
        if(strleft.empty() || strleft==processd)
            break;
    }
}
/*
 * functions for reading http request from read buffer to redis
 */
http_conn::HTTP_CODE http_conn::process_read() {
    string m_head;
    string m_left = read_buff;
    int flag = 0;
    int do_post_flag = 0;
    while(true){
        m_head = m_left.substr(0, m_left.find("\r\n"));
        m_left = m_left.substr(m_left.find("\r\n") + 2);
        if(flag == 0){
            flag = 1;
            parser_requestline(m_head);
        }
        else if(do_post_flag){
            parser_postinfo(m_head);
            break;
        }
        else{
            parser_header(m_head);
        }
        if(m_head.empty())
            do_post_flag = 1;
        if(m_left.empty())
            break;
    }
    if(m_map["method"] == "POST"){
        return POST_REQUEST;
    }else if(m_map["method"] == "GET"){
        return GET_REQUEST;
    }else{
        return NO_REQUEST;
    }
}
bool http_conn::read() {
    if(read_for_now > BUFF_READ_SIZE){
        cout << "read error" <<endl;
        return false;
    }
    int bytes_read = 0;
    while(true){
        bytes_read = recv(m_socket, read_buff + read_for_now, BUFF_READ_SIZE - read_for_now, 0);
        if(bytes_read == -1){
            if(errno == EAGAIN ){
                break;
            }
            cout<< " bytes_read == -1" << endl;
            return false;
        }
        else if(bytes_read == 0){
            cout<< " bytes_read == 0" << endl;
            return false;
        }
        read_for_now += bytes_read;
    }
    return true;
}
/*
 *functions for deciding which page should we return
 */
bool http_conn::do_request(){
    if(m_map["method"] == "POST"){
        redis_clt* m_redis = redis_clt::getInstance();
        if(m_map["url"] == "/base.html" || m_map["url"] == "/"){
            if(m_redis->getUserpasswd(m_map["username"]) == m_map["passwd"]){
                if(m_redis->getUserpasswd(m_map["username"]) == "root")
                    filename = "./root/welcome.html";
                else
                    filename = "./root/welcome.html";
            }
            else{
                filename = "./root/error.html";
            }
        }
        else if (m_map["url"] == "/welcome.html") //如果来自登录后界面
        {
            m_redis->vote(m_map["votename"]);
            postmsg = "";
            return false;
        }
        else if(m_map["url"] == "/register.html"){
            m_redis->setUserpasswd(m_map["username"], m_map["passwd"]);
            filename = "./root/register.html";
        }
        else if(m_map["url"] == "/getvote"){
            postmsg = m_redis->getvoteboard();\
            return false;
        }
        else{
            filename = "./root/base.html"; //进入初始登录界面
        }
    }
    else if(m_map["method"] == "GET"){
        if(m_map["url"] == "/"){
            m_map["url"] = "/base.html";
        }
        filename = "./root" + m_map["url"];
    }
    else{
        filename = "./root/error.html";
    }
    return true;
}
/*
 * functions preparing for the msg about to write into read-buf
 */
bool http_conn::process_write(http_conn::HTTP_CODE ret) {
    if(do_request()){
        int fd = open(filename.c_str(), O_RDONLY);
        stat(filename.c_str(), &m_file_stat);
        file_addr = (char*)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        m_iovec[1].iov_base = file_addr;
        m_iovec[1].iov_len = m_file_stat.st_size;
        m_iovec_length = 2;
        close(fd);
    }
    else{
        if(postmsg!=""){
            m_iovec[1].iov_base = post_temp;
            m_iovec[1].iov_len = (postmsg.size() * sizeof(char));
            m_iovec_length = 2;
        }
        else{
            m_iovec_length = 1;
        }
    }
    return true;
}

/*
 * functions for processing buffer writing
 */
bool http_conn::write() {
    int bytes_write = 0;
    //every request is ok
    string response_head = "HTTP/1.1 200 OK\r\n\r\n";
    char head_temp[response_head.size()];
    strcpy(head_temp, response_head.c_str());
    m_iovec[0].iov_base = head_temp;
    m_iovec[0].iov_len = response_head.size() * sizeof(char);
    bytes_write = writev(m_socket, m_iovec, m_iovec_length);
    if(bytes_write <= 0){
        return false;
    }
    unmap();
    if (m_map["Connection"] == "keep-alive")
    {
        return true;
    }
    else
    {
        return false;
    }
}
void http_conn::unmap()
{
    if (file_addr)
    {
        munmap(file_addr, m_file_stat.st_size);
        file_addr = 0;
    }
}
