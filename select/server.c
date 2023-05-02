#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h> // struct timeval

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * sockaddr_in 定义在<netinet/in.h>或<arpa/inet.h>中，
 * 该结构体解决了sockaddr（定义在<sys/socket.h> 里）的缺陷，
 * 把port和addr分开存储在两个变量中
 */
#include <netinet/in.h>
#include <arpa/inet.h> // inet_addr
#include <unistd.h>    //socklen_t

#include <errno.h>

#define MAX_LINE 1024
#define SERV_ADDR "127.0.0.1"
#define SERV_PORT 7070
// 处于 完全连接状态的socket 的上限
#define BACKLOG 10

void print_addr_info(int listenfd)
{
    struct sockaddr_in listenfd_local_addr;
    socklen_t listenfd_local_addr_len = sizeof(struct sockaddr_in);
    int r1 = getsockname(listenfd, (struct sockaddr *)&listenfd_local_addr, &listenfd_local_addr_len);
    struct sockaddr_in listenfd_remote_addr;
    socklen_t listenfd_remote_addr_len = sizeof(struct sockaddr_in);
    int r2 = getpeername(listenfd, (struct sockaddr *)&listenfd_remote_addr, &listenfd_remote_addr_len);
    printf("fd is:%d\naddress is:%s:%d    remote address is %s:%d\n", listenfd,
           inet_ntoa(listenfd_local_addr.sin_addr), ntohs(listenfd_local_addr.sin_port),
           inet_ntoa(listenfd_remote_addr.sin_addr), ntohs(listenfd_remote_addr.sin_port));
}

void handle_connection(int *client, int max_fd, fd_set *read_set_p, fd_set *all_set_p)
{
    int n_read;
    char buf[MAX_LINE];
    memset(buf, 0, sizeof(buf)); // 初始化 接受缓冲区
    for (int i = 0; i < max_fd; i++)
    {
        int connected_fd = client[i];
        if (connected_fd != -1)
        {
            if (FD_ISSET(connected_fd, read_set_p))
            {
                /**
                 * n_read <= rec_buf.size()
                 * 如果发送的数据大于这个rec_buf，则recv函数多次进入接受缓冲区分批放入该数组
                 * 返回读到的数据长度（可能小于期望长度，因为可能Buf太小，一次读不完）
                 */
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
                else if (n_read == 0)
                {
                    printf("client disconnect\n");
                    print_addr_info(connected_fd);
                    close(connected_fd);
                    FD_CLR(connected_fd, all_set_p);
                    client[i] = -1;
                    continue; // ctrl+c 断开客户端
                }
                else
                {
                    perror("read error");
                    close(connected_fd);
                    FD_CLR(connected_fd, all_set_p);
                    client[i] = -1;
                    continue;
                }
            }
        }
    }
}

int main(int argc, char **argv)
{
    int listenfd;                    // server's listening socket
    struct sockaddr_in s_addr = {0}; // server's socket address
    struct sockaddr_in c_addr = {0}; // client's socket address
    socklen_t c_addrlen = 0;         // client's sockaddr's length
    fd_set read_set, all_set;
    int n_ready, client[FD_SETSIZE]; // 头文件<sys/selet.h>中定义的FD_SETSIZE常值是数据类型fd_set中描述符总数，其值通常是1024
    int max_fd;

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
    printf("server open at %s:%d, with max client=%d(backlo%d)\n", SERV_ADDR, SERV_PORT, FD_SETSIZE, BACKLOG);

    // initialize
    for (int i = 0; i < FD_SETSIZE; i++)
    {
        // -1 indicates available entry
        client[i] = -1;
    }
    /**
     * 需要检查的文件描述字个数（即检查到fd_set的第几位），
     * 数值应该比三组fd_set中所含的最大fd值更大，一般设为三组fd_set中所含的最大fd值加1
     * （如在readset,writeset,exceptset中所含最大的fd为5，则nfds=6，因为fd是从0开始的）。
     * 设这个值是为提高效率，使函数不必检查fd_set的所有1024位。
     */
    max_fd = listenfd;
    /**
     * 将指定的文件描述符集清空，
     * 在对文件描述符集合进行设置前，必须对其进行初始化，
     * 如果不清空，由于在系统分配内存空间后，通常并不作清空处理，所以结果是不可知的。
     */
    FD_ZERO(&all_set);
    // 用于在文件描述符集合中增加一个新的文件描述符。
    FD_SET(listenfd, &all_set);

    while (1)
    {
        read_set = all_set;
        n_ready = select(max_fd + 1, &read_set, NULL, NULL, NULL);
        if (n_ready < 0)
        {
            perror("select error");
            continue;
        }

        /**
         * 检查s是否在这个集合里面,
         * select将更新这个集合,把其中不可读的套节字去掉
         * 只保留符合条件的套节字在这个集合里面
         */
        if (FD_ISSET(listenfd, &read_set))
        {
            int connected_fd = accept(listenfd, (struct sockaddr *)&c_addr, &c_addrlen);
            if (connected_fd < 0)
            {
                perror("accept error");
                continue;
            }
            printf("new client:\n");
            print_addr_info(connected_fd);

            int i = 0;
            for (i = 0; i < FD_SETSIZE; i++)
            {
                if (client[i] == -1)
                {
                    client[i] = connected_fd;
                    break;
                }
            }
            if (i == FD_SETSIZE)
            {
                printf("too many clients");
                close(connected_fd);
                continue;
            }
            if (connected_fd > max_fd)
            {
                max_fd = connected_fd;
            }

            FD_SET(connected_fd, &all_set);
            if (--n_ready <= 0)
            {
                continue;
            }
        }
        handle_connection(client, max_fd, &read_set, &all_set);
    }
    return 0;
}