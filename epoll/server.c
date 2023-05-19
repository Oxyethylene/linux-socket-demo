#include <stdio.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>

#define MAX_LINE 1024
#define SERV_ADDR "127.0.0.1"
#define SERV_PORT 8001
// 处于 完全连接状态的socket 的上限
#define BACKLOG 10
#define MAX_CLIENT 1024

void handle_connection(int sockfd)
{
    /**
     * n_read <= rec_buf.size()
     * 如果发送的数据大于这个rec_buf，则recv函数多次进入接受缓冲区分批放入该数组
     * 返回读到的数据长度（可能小于期望长度，因为可能Buf太小，一次读不完）
     */
    char buf[MAX_LINE];
    int n_read = recv(sockfd, buf, sizeof(buf), 0);
    if (n_read < MAX_LINE)
    {
        buf[n_read] = '\0';
    }
    if (n_read > 1)
    {
        // 从连接套接字指向的文件中 读出客户端发过来的消息
        printf("len:%d   client's msg:%s\n", n_read, buf);
        send(sockfd, buf, n_read, 0);
    }
    else if (n_read == 0) // ctrl+c 断开客户端
    {
        printf("client disconnect\n");
        close(sockfd);
    }
    else
    {
        perror("read error");
        close(sockfd);
    }
}

int main(int argc, char *argv[])
{
    int listenfd;
    int epollfd;
    int ret;
    int connfd;
    struct sockaddr_in s_addr = {0};
    struct sockaddr_in c_addr = {0};
    socklen_t c_addrlen = 0;
    struct epoll_event ev, ep[MAX_CLIENT];

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0)
    {
        perror("socket error");
        return -1;
    }
    printf("listenfd = %d\n", listenfd);

    s_addr.sin_family = AF_INET;
    s_addr.sin_addr.s_addr = inet_addr(SERV_ADDR);
    s_addr.sin_port = htons(SERV_PORT);

    if (bind(listenfd, (struct sockaddr *)&s_addr, sizeof(struct sockaddr)) == -1)
    {
        perror("bind error");
        return -1;
    }

    if (listen(listenfd, BACKLOG) < 0)
    {
        perror("listen error");
        return -1;
    }
    printf("server open at %s:%d, with max client=%d(backlog=%d)\n", SERV_ADDR, SERV_PORT, MAX_CLIENT, BACKLOG);

    // 创建一个epoll fd
    epollfd = epoll_create(MAX_CLIENT);
    ev.events = EPOLLIN;
    ev.data.fd = listenfd;
    // 把监听socket 先添加到efd中
    ret = epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &ev);
    // 循环等待
    while (1)
    {
        // 返回已就绪的epoll_event,-1表示阻塞,没有就绪的epoll_event,将一直等待
        size_t nready = epoll_wait(epollfd, ep, MAX_CLIENT, -1);
        for (int i = 0; i < nready; ++i)
        {
            // 如果是新的连接,需要把新的socket添加到efd中
            if (ep[i].data.fd == listenfd)
            {
                connfd = accept(listenfd, (struct sockaddr *)&c_addr, &c_addrlen);
                ev.events = EPOLLIN;
                ev.data.fd = connfd;
                ret = epoll_ctl(epollfd, EPOLL_CTL_ADD, connfd, &ev);
            }
            // 否则,读取数据
            else
            {
                handle_connection(ep[i].data.fd);
            }
        }
    }
    return 0;
}