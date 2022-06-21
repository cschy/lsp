#include <stdio.h>  
#include <unistd.h>  
#include <stdlib.h>  
#include <string.h>  
#include <sys/types.h>
#include <sys/socket.h>  
#include <errno.h>  
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/resource.h>/*设置最大的连接数需要setrlimit*/
#include <iostream>
#include <string>
#include "ThreadPool.h"
#include "utils.h"
#include "main.h"
#include "recvip.h"

#define PORT        6666
#define MAXEPOLL    4096
#define MAXEVENTS   64
#define MAXLINE     1024

int cur_fds = 1;
int epoll_fd;

MyData* recvData(MyData* data)
{
    char buf[MAXLINE];
    while (true) {
        bzero(&buf, sizeof(buf));
        ssize_t read_bytes = read(data->fd, buf, sizeof(buf));
        if (read_bytes > 0) {
            data->readBuf.append(buf, read_bytes);
        }
        else if (read_bytes == -1 && errno == EINTR) {
            std::cout << "continue reading" << std::endl;
            continue;
        }
        else if (read_bytes == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            printf("[recv from %s:%d]: %s\n", inet_ntoa(data->userAddr.sin_addr), data->userAddr.sin_port, data->readBuf.c_str());
            /*errif(write(data->fd, data->readBuf.c_str(), data->readBuf.size()) == -1, "socket write error: ");
            data->readBuf.clear();*/
            break;
        }
        else if (read_bytes == 0) {
            printf("[leave] %s:%d\n", inet_ntoa(data->userAddr.sin_addr), data->userAddr.sin_port);
            //epoll_ctl(epfd, EPOLL_CTL_DEL, data->fd, NULL);
            close(data->fd);
            --cur_fds;
            break;
        }
    }
    return data;
}

void sendData(MyData* data)
{
    std::size_t data_size = data->sendBuf.size();
    char buf[data_size + 1];
    buf[data_size] = '\0';
    memcpy(buf, data->sendBuf.c_str(), data_size);

    char* pbuf = buf, * endbuf = &buf[data_size];

    while (pbuf < endbuf) {
        ssize_t write_bytes = write(data->fd, pbuf, endbuf - pbuf);
        if (write_bytes == data_size) {
            printf("[send to %s:%d]: %s\n", inet_ntoa(data->userAddr.sin_addr), data->userAddr.sin_port, buf);
            data->sendBuf.clear();
        }
        if (write_bytes == -1 && errno == EINTR) {
            std::cout << "continue writing" << std::endl;
            continue;
        }
        if (write_bytes == -1 && errno == EAGAIN) {
            std::cout << "send buffer overflow" << std::endl;

            data->sendBuf = pbuf;
            epoll_event ev;
            ev.events = EPOLLIN | EPOLLET | EPOLLOUT;
            ev.data.ptr = (void*)data;
            errif(epoll_ctl(epoll_fd, EPOLL_CTL_MOD, data->fd, &ev) == -1, "epoll_mod +EPOLLOUT error: ");
            
            break;
        }
        if (write_bytes == -1) {
            printf("other error on client fd %d\n", data->fd);
            break;
        }
        pbuf += write_bytes;
    }

}

void startServer(Business* business)
{
    int listen_fd;

    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(PORT);

    struct epoll_event ev, evs[MAXEPOLL];

    do {
        struct rlimit rlt = { MAXEPOLL, MAXEPOLL };
        errif(setrlimit(RLIMIT_NOFILE, &rlt) == -1, "setrlimit error: ");

        errif((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1, "create listen_fd error: ");
        errif(bind(listen_fd, (struct sockaddr*)&servaddr, sizeof(struct sockaddr)) == -1, "bin listen_fd error: ");
        errif(listen(listen_fd, SOMAXCONN) == -1, "listen listen_fd error: ");
        errif((epoll_fd = epoll_create(MAXEPOLL)) == -1, "epoll_create error: ");
        //errif(fcntl(listen_fd, F_SETFL, fcntl(listen_fd, F_GETFD, 0) | O_NONBLOCK) == -1, "setnonblocking listen_fd error: ");
        ev.events = EPOLLIN /*| EPOLLET*/;
        ev.data.fd = listen_fd;
        errif(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev) == -1, "epoll_add listen_fd error: ");
    } while (0);


    struct sockaddr_in cliaddr;
    socklen_t len = sizeof(struct sockaddr);
    int wait_fds, i, conn_fd;
    ThreadPool pool;

    while (true)
    {
        errif((wait_fds = epoll_wait(epoll_fd, evs, MAXEVENTS, -1)) == -1, "epoll_wait error: ");

        for (i = 0; i < wait_fds; i++)
        {
            //accept
            if (evs[i].data.fd == listen_fd && cur_fds < MAXEPOLL)
            {
                ++cur_fds;
                errif((conn_fd = accept(listen_fd, (struct sockaddr*)&cliaddr, &len)) == -1, "accept error: ");
                printf("[connect] %s:%d]\n", inet_ntoa(cliaddr.sin_addr), cliaddr.sin_port);
                errif(fcntl(conn_fd, F_SETFL, fcntl(listen_fd, F_GETFD, 0) | O_NONBLOCK) == -1, "setnonblocking conn_fd error: ");

                MyData* data = new MyData{ conn_fd, cliaddr, "", "" };
                business->accept_callback(data);
                
                ev.events = EPOLLIN | EPOLLET;
                ev.data.ptr = (void*)data;
                errif(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn_fd, &ev) == -1, "epoll_add conn_fd error: ");

                continue;
            }

            //发送剩余数据
            MyData* data = (MyData*)evs[i].data.ptr;
            if ((evs[i].events & EPOLLOUT) == EPOLLOUT && !data->sendBuf.empty())
            {
                evs[i].events |= ~EPOLLOUT;
                evs[i].data.ptr = (void*)data;
                errif(epoll_ctl(epoll_fd, EPOLL_CTL_MOD, data->fd, &evs[i]) == -1, "epoll_mod -EPOLLOUT error: ");
                sendData(data);
                //continue;
            }

            //读数据
            pool.add([&]() {
                business->read_callback(recvData(data));
                //模拟高并发收数据
                //while (1);
                /*for (int i = 0; i < INT32_MAX; i++)
                {
                    i * 33.33;
                }*/
                
            });

        }
    }

    close(listen_fd);
}

int main(int argc, char* argv[])
{    
    std::setlocale(LC_ALL, "en_US.UTF-8");
    startServer(new RecvIp);
    return 0;
}
