#ifndef LST_TIMER
#define LST_TIMER

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

#include <time.h>
#include "../log/log.h"
//前向声明
class util_timer;
//用户数据结构
struct client_data
{
    sockaddr_in address;//客户端address
    int sockfd;//socket文件描述符
    util_timer *timer;//定时器
};
//定时器类
class util_timer
{
public:
    util_timer() : prev(NULL), next(NULL) {}

public:
    time_t expire;//超时时间
    
    void (* cb_func)(client_data *);//回调函数
    client_data *user_data;//用户数据
    util_timer *prev;//指向前一个定时器
    util_timer *next;//指向后一个定时器
};
//定时器链表 升序双向链表
class sort_timer_lst
{
public:
    sort_timer_lst();
    ~sort_timer_lst();
    //将timer添加到链表中 O(n)
    void add_timer(util_timer *timer);
    //当某个定时任务发生变化时调用，调整到合适的位置 O(n)
    void adjust_timer(util_timer *timer);
    //删除指定定时器 O(1)
    void del_timer(util_timer *timer);
    //💓函数，每个一段时间执行一次，SIGALRM信号每次被触发就在其信号处理函数中执行一次tick，处理链表上到期的任务 O(k)
    void tick();

private:
    void add_timer(util_timer *timer, util_timer *lst_head);

    util_timer *head;//头节点
    util_timer *tail;//尾节点
};

class Utils
{
public:
    Utils() {}
    ~Utils() {}

    void init(int timeslot);

    //对文件描述符设置非阻塞
    int setnonblocking(int fd);

    //将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
    void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);

    //信号处理函数
    static void sig_handler(int sig);

    //设置信号函数
    void addsig(int sig, void(handler)(int), bool restart = true);

    //定时处理任务，重新定时以不断触发SIGALRM信号
    void timer_handler();
    
    void show_error(int connfd, const char *info);

public:
    static int *u_pipefd;
    sort_timer_lst m_timer_lst;
    static int u_epollfd;
    int m_TIMESLOT;
};

void cb_func(client_data *user_data);

#endif
