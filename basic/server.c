#include <sys/types.h>
#include <sys/socket.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/*
 * sockaddr_in 定义在<netinet/in.h>或<arpa/inet.h>中，
 * 该结构体解决了sockaddr（定义在<sys/socket.h> 里）的缺陷，
 * 把port和addr分开存储在两个变量中
 */
#include <netinet/in.h>
#include <arpa/inet.h> // inet_addr
#include <unistd.h>    //socklen_t

#include <pthread.h>

#include <errno.h>

#define MAX_LINE 1024
#define SERV_ADDR "127.0.0.1"
#define SERV_PORT 7070
// 处于 完全连接状态的socket 的上限
#define BACKLOG 10

typedef struct
{
    int listenfd;
    int connected_fd;
    struct sockaddr_in c_addr;
} conn_handler_param;

void print_addr_info(int listenfd)
{
    struct sockaddr_in listenfd_local_addr;
    socklen_t listenfd_local_addr_len = sizeof(struct sockaddr_in);
    int r1 = getsockname(listenfd, (struct sockaddr *)&listenfd_local_addr, &listenfd_local_addr_len);
    struct sockaddr_in listenfd_remote_addr;
    socklen_t listenfd_remote_addr_len = sizeof(struct sockaddr_in);
    int r2 = getpeername(listenfd, (struct sockaddr *)&listenfd_remote_addr, &listenfd_remote_addr_len);
    printf("address is:%s:%d    remote address is %s:%d\n",
           inet_ntoa(listenfd_local_addr.sin_addr), ntohs(listenfd_local_addr.sin_port),
           inet_ntoa(listenfd_remote_addr.sin_addr), ntohs(listenfd_remote_addr.sin_port));
}

void *handle_connection(void *args)
{
    conn_handler_param *param = (conn_handler_param *)args;
    struct sockaddr_in c_addr = param->c_addr;
    int connected_fd = param->connected_fd;
    int listenfd = param->listenfd;
    char buf[MAX_LINE];
    // [这里的client ip输出是0.0.0.0表示，被同意连接的客户端可以是本机上的任意一个进程(port)]
    printf("client_addr:%s\n", inet_ntoa(c_addr.sin_addr));
    // 用于和当前客户端通信的文件，是该进程打开的第几个文件
    printf("connectedfd:%d\n", connected_fd);
    // 读取listenfd的本地地址和远端地址(这一块与socket通信无关，只是为了测试各个套接字的本地地址和远端地址)
    printf("for listenfd:\n");
    print_addr_info(listenfd);
    // 读取connectedfd的本地地址和远端地址
    printf("for connectedfd:\n");
    print_addr_info(connected_fd);

    memset(buf, 0, sizeof(buf)); // 初始化 接受缓冲区
    while (1)
    {
        // n_read <= rec_buf.size()
        // 如果发送的数据大于这个rec_buf，则recv函数多次进入接受缓冲区分批放入该数组
        // 返回读到的数据长度（可能小于期望长度，因为可能Buf太小，一次读不完）
        int n_read = recv(connected_fd, buf, sizeof(buf), 0);
        if (n_read < MAX_LINE)
        {
            buf[n_read] = '\0';
        }
        if (n_read > 1)
        {
            // 从连接套接字指向的文件中 读出客户端发过来的消息
            printf("len:%d   client's msg:%s\n", n_read, buf);
            send(connected_fd, buf, n_read, 0);
        }
        else
        {
            printf("%d\n", n_read);
            printf("client disconnect\n");
            close(connected_fd);
            break; // ctrl+c 断开客户端
        }
    }
}

int main(int argc, char **argv)
{
    int listenfd;                    // server's listening socket
    struct sockaddr_in s_addr = {0}; // server's socket address
    struct sockaddr_in c_addr = {0}; // client's socket address
    socklen_t c_addrlen = 0;         // client's sockaddr's length

    pthread_t thread;

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0)
    {
        perror("socket error");
        return -1;
    }
    printf("listenfd = %d\n", listenfd);

    // 地址族（要和socket定义时一样）
    s_addr.sin_family = AF_INET;
    // 将 点分十进制字符串表示的ipv4地址 转化为 网络字节序证书表示的ipv4地址
    s_addr.sin_addr.s_addr = inet_addr(SERV_ADDR);
    // 将 无符号短整形的主机字节序  转化为  短整形的网络字节序
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
    printf("server open at %s:%d, with max client(backlog) %d\n", SERV_ADDR, SERV_PORT, BACKLOG);

    while (1)
    {
        int connected_fd = accept(listenfd, (struct sockaddr *)&c_addr, &c_addrlen);
        if (connected_fd)
        {
            conn_handler_param args = {
                connected_fd : connected_fd,
                listenfd : listenfd,
                c_addr : c_addr
            };
            pthread_create(&thread, NULL, handle_connection, (void *)&args);
        }
    }
    return 0;
}