#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H
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
#include <sys/wait.h>
#include <sys/uio.h>
#include <map>

#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"
#include "../timer/lst_timer.h"
#include "../log/log.h"

class http_conn
{
public:
    //文件名的最大长度
    static const int FILENAME_LEN = 200;
    //读缓冲区的大小
    static const int READ_BUFFER_SIZE = 2048;
    //写缓冲区的大小
    static const int WRITE_BUFFER_SIZE = 1024;
    //HTTP请求方法
    enum METHOD
    {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATH
    };
    //解析请求时的状态 请求行 请求头部 请求实体
    enum CHECK_STATE
    {
        CHECK_STATE_REQUESTLINE = 0,
        CHECK_STATE_HEADER,
        CHECK_STATE_CONTENT
    };
    // 服务器处理HTTP时可能的结果
    enum HTTP_CODE
    { 
        NO_REQUEST,//请求不完整，需要继续读取请求报文数据--跳转主线程继续监测读事件
        GET_REQUEST,//获得了完整的HTTP请求--调用do_request完成请求资源映射
        BAD_REQUEST,//HTTP请求报文有语法错误或请求资源为目录--跳转process_write完成响应报文
        NO_RESOURCE,//请求资源不存在--跳转process_write完成响应报文 
        FORBIDDEN_REQUEST,//请求资源禁止访问，没有读取权限--跳转process_write完成响应报文
        FILE_REQUEST,//请求资源可以正常访问--跳转process_write完成响应报文
        INTERNAL_ERROR,//服务器内部错误，该结果在主状态机逻辑switch的default下，一般不会触发
        CLOSED_CONNECTION
    };
    // 解析请求时一行一行读取时的状态
    enum LINE_STATUS
    {
        LINE_OK = 0,
        LINE_BAD,
        LINE_OPEN
    };

public:
    http_conn() {}
    ~http_conn() {}

public:
    //初始化新的连接
    void init(int sockfd, const sockaddr_in &addr, char *, int, int, string user, string passwd, string sqlname);
    //关闭连接
    void close_conn(bool real_close = true);
    //处理客户请求
    void process();
    //读取浏览器端发来的全部数据
    bool read_once();
    //响应报文写入函数
    bool write();
    sockaddr_in *get_address()
    {
        return &m_address;
    }
    //同步线程初始化数据库读取表
    void initmysql_result(connection_pool *connPool);
    int timer_flag;
    int improv;


private:
    void init();
    //解析HTTP请求报文
    HTTP_CODE process_read();
    //填充HTTP应答报文
    bool process_write(HTTP_CODE ret);
    // 被process_read()调用
    //处理请求行
    HTTP_CODE parse_request_line(char *text);
    //处理请求头部
    HTTP_CODE parse_headers(char *text);
    //处理请求实体
    HTTP_CODE parse_content(char *text);
    //生成相应报文
    HTTP_CODE do_request();

    //m_start_line是已经解析的字符
    //get_line用于将指针向后偏移，指向未处理的字符
    char *get_line() { return m_read_buf + m_start_line; };

    //从状态机读取一行，分析是请求报文的哪一部分
    LINE_STATUS parse_line();

    
    void unmap();
    //根据响应报文格式，生成对应8个部分，以下函数均由do_request调用
    bool add_response(const char *format, ...);
    bool add_content(const char *content);
    //添加响应状态行
    bool add_status_line(int status, const char *title);
    //添加响应头部
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    //添加空行
    bool add_blank_line();

public:

    static int m_epollfd;
    //用户数量
    static int m_user_count;
    MYSQL *mysql;
    int m_state;  //读为0, 写为1

private:
    //该HTTP连接的socekt和对应的客户端地址
    int m_sockfd;
    sockaddr_in m_address;
    //读缓冲区
    char m_read_buf[READ_BUFFER_SIZE];

    //已读入的客户数据的最后一个字节的下一个位置
    int m_read_idx;
    //当前解析字符的地址
    int m_checked_idx;
    //当前解析行起始地址
    int m_start_line;

    //写缓冲区
    char m_write_buf[WRITE_BUFFER_SIZE];
    //写缓存区中待发送的字节数
    int m_write_idx;

    //状态机的状态
    CHECK_STATE m_check_state;
    //请求方法
    METHOD m_method;

    //以下为解析请求报文中对应的6个变量
    //客户端请求目标文件的完整路径  = doc_root + m_url
    char m_real_file[FILENAME_LEN];
    //客户端请求的目标文件的文件名
    char *m_url;
    //请求行版本号
    char *m_version;
    //主机名
    char *m_host;
    //HTTP请求主体的长度
    int m_content_length;
    //是否要保持连接
    bool m_linger;


    //读取服务器上的文件地址
    char *m_file_address;
    struct stat m_file_stat;
    //io向量机制iovec
    struct iovec m_iv[2];
    int m_iv_count;
    //是否启用的POST
    int cgi;
    //存储请求头数据       
    char *m_string; 
    //剩余发送字节数
    int bytes_to_send;
    //已发送字节数
    int bytes_have_send;
    char *doc_root;

    map<string, string> m_users;
    //读取模式 LT=0 ET=1
    int m_TRIGMode;
    int m_close_log;

    char sql_user[100];
    char sql_passwd[100];
    char sql_name[100];
};

#endif
