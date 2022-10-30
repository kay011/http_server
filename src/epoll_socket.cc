#include <unistd.h>
#include <cstdlib>
#include <cstdio>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "epoll_socket.h"
#include <sys/fcntl.h>
#include "utils.h"
#include "global.h"
#include <pthread.h>
struct HttpThreadArgs{
    EpollSocketWatcher* socket_handler;
};

int EpollSocket::handle_accept_event(int &epollfd, epoll_event &event, EpollSocketWatcher &socket_handler)
{
    int listenfd = event.data.fd;

    std::string client_ip;
    int conn_sock = SocketUtils::accept_socket(listenfd, client_ip);
    if (conn_sock == -1)
    {
        return -1;
    }
    SocketUtils::set_nonblocking(conn_sock);
    LOG(INFO) << "get accept socket which listen fd:" << listenfd << ",conn_sock_fd: " << conn_sock;

    EpollContext *epoll_context = new EpollContext();
    epoll_context->fd = conn_sock;
    epoll_context->client_ip = client_ip;

    socket_handler.on_accept(*epoll_context);

    struct epoll_event conn_sock_ev;
    conn_sock_ev.events = EPOLLIN | EPOLLET;
    conn_sock_ev.data.ptr = epoll_context;

    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, conn_sock, &conn_sock_ev) == -1)
    {
        LOG(FATAL) << "epoll_ctl err";
    }

    return 0;
}
void * async_bussiness(void* arg){


}

int EpollSocket::handle_readable_event(int &epollfd, epoll_event &event, EpollSocketWatcher &socket_handler)
{
    EpollContext *epoll_context = (EpollContext *)event.data.ptr;
    int fd = epoll_context->fd;

    int buffer_size = SS_READ_BUFFER_SIZE;
    char read_buffer[buffer_size];
    memset(read_buffer, 0, buffer_size);

    // int read_size = recv(fd, read_buffer, buffer_size, 0);
    int read_size = SocketUtils::readn(fd, read_buffer, buffer_size);

    // 异步


    int handle_ret = 0;
    if (read_size > 0)
    {
        LOG(INFO) << "read success which read size: " << read_size;
        handle_ret = socket_handler.on_readable(*epoll_context, read_buffer, buffer_size, read_size);
    }

    if (read_size <= 0 || handle_ret < 0)
    {
        close_and_release(epollfd, event, socket_handler);
        return 0;
    }

    if (handle_ret == READ_CONTINUE)
    {
        event.events = EPOLLIN | EPOLLET;
    }
    else
    {
        event.events = EPOLLOUT | EPOLLET;
    }
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
    return 0;
}

int EpollSocket::handle_writeable_event(int &epollfd, epoll_event &event, EpollSocketWatcher &socket_handler)
{
    EpollContext *epoll_context = (EpollContext *)event.data.ptr;
    int fd = epoll_context->fd;
    LOG(INFO) << "start write data";

    int ret = socket_handler.on_writeable(*epoll_context);
    if (ret == WRITE_CONN_CLOSE)
    {
        close_and_release(epollfd, event, socket_handler);  // 断开
        return 0;
    }

    if (ret == WRITE_CONN_CONTINUE)
    {
        event.events = EPOLLOUT | EPOLLET;
    }
    else
    {
        event.events = EPOLLIN | EPOLLET; // 长连接
    }
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
    return 0;
}

int EpollSocket::start_epoll(int port, EpollSocketWatcher &socket_handler, int backlog, int max_events)
{
    int listenfd = SocketUtils::listen_on(port, backlog);

    int epollfd = epoll_create(1024);
    if (epollfd == -1)
    {
        LOG(FATAL) << "epoll_create err";
    }

    struct epoll_event ev; // 注册可读事件
    ev.events = EPOLLIN;
    ev.data.fd = listenfd;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &ev) == -1)
    {
        LOG(FATAL) << "epoll_ctl listen err";
    }

    epoll_event *events = new epoll_event[max_events];

    while (1)
    {
        int fds_num = epoll_wait(epollfd, events, max_events, -1);
        if (fds_num == -1)
        {
            LOG(ERROR) << "epoll_wait err";
        }

        for (int i = 0; i < fds_num; i++)
        {
            if (events[i].data.fd == listenfd)
            {
                // accept connection
                this->handle_accept_event(epollfd, events[i], socket_handler);
            }
            else if (events[i].events & EPOLLIN)
            {
                // readable
                this->handle_readable_event(epollfd, events[i], socket_handler);
            }
            else if (events[i].events & EPOLLOUT)
            {
                // writeable
                this->handle_writeable_event(epollfd, events[i], socket_handler);
            }
            else
            {
                LOG(WARNING) << "unkonw events :" << events[i].events;
            }
        }
    }

    if (events != NULL)
    {
        delete[] events;
        events = NULL;
    }
}

int EpollSocket::close_and_release(int &epollfd, epoll_event &epoll_event, EpollSocketWatcher &socket_handler)
{
    if (epoll_event.data.ptr == NULL)
    {
        return 0;
    }

    LOG(INFO) << "connect close";

    EpollContext *hc = (EpollContext *)epoll_event.data.ptr;

    socket_handler.on_close(*hc);

    int fd = hc->fd;
    epoll_event.events = EPOLLIN | EPOLLOUT | EPOLLET;
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, &epoll_event);

    delete (EpollContext *)epoll_event.data.ptr;
    epoll_event.data.ptr = NULL;

    int ret = close(fd);
    LOG(INFO) << "connect close complete which fd: " << fd << ", ret: " << ret;
    return ret;
}