#include "eventloop.h"
#include <sys/epoll.h>
#include "easylogging++.h"
#include "utils.h"
#include <unistd.h>
#include <cstdlib>
#include <cstdio>


void EventLoop::init(EpollSocketWatcher* socket_watcher){
    socket_watcher_ = socket_watcher;
    epoll_fd_ = epoll_create(1024);
    if (epoll_fd_ == -1)
    {
        LOG(FATAL) << "epoll_create err";
    }
}  

void EventLoop::loop(int listenfd){
    int max_events = 1000;
    epoll_event *events = new epoll_event[max_events];
    while (1)
    {
        int fds_num = epoll_wait(epoll_fd_, events, max_events, -1);
        if (fds_num == -1)
        {
            LOG(ERROR) << "epoll_wait err";
        }

        for (int i = 0; i < fds_num; i++)
        {
            if (events[i].data.fd == listenfd)
            {
                // accept connection
                this->handle_accept_event(epoll_fd_, events[i], socket_watcher_);
            }
            else if (events[i].events & EPOLLIN)
            {
                // readable
                this->handle_readable_event(epoll_fd_, events[i], socket_watcher_);
            }
            else if (events[i].events & EPOLLOUT)
            {
                // writeable
                this->handle_writeable_event(epoll_fd_, events[i], socket_watcher_);
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
int EventLoop::close_and_release(int &epollfd, epoll_event &event, EpollSocketWatcher* socket_watcher){
    if (event.data.ptr == NULL)
    {
        return 0;
    }

    LOG(INFO) << "connect close";

    EpollContext *hc = (EpollContext *)event.data.ptr;

    socket_watcher->on_close(*hc);

    int fd = hc->fd;
    event.events = EPOLLIN | EPOLLOUT | EPOLLET;
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, &event);

    delete (EpollContext *)event.data.ptr;
    event.data.ptr = NULL;

    int ret = close(fd);
    LOG(INFO) << "connect close complete which fd: " << fd << ", ret: " << ret;
    return ret;
}

int EventLoop::handle_accept_event(int &epollfd, epoll_event &event, EpollSocketWatcher* socket_watcher){
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

    socket_watcher->on_accept(*epoll_context);

    struct epoll_event conn_sock_ev;
    conn_sock_ev.events = EPOLLIN | EPOLLET;
    conn_sock_ev.data.ptr = epoll_context;

    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, conn_sock, &conn_sock_ev) == -1)
    {
        LOG(FATAL) << "epoll_ctl err";
    }

    return 0;
}

int EventLoop::handle_readable_event(int &epollfd, epoll_event &event, EpollSocketWatcher* socket_watcher){
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
        handle_ret = socket_watcher->on_readable(*epoll_context, read_buffer, buffer_size, read_size);
    }

    if (read_size <= 0 || handle_ret < 0)
    {
        close_and_release(epollfd, event, socket_watcher);
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

int EventLoop::handle_writeable_event(int &epollfd, epoll_event &event, EpollSocketWatcher* socket_watcher){
    EpollContext *epoll_context = (EpollContext *)event.data.ptr;
    int fd = epoll_context->fd;
    LOG(INFO) << "start write data";

    int ret = socket_watcher->on_writeable(*epoll_context);
    if (ret == WRITE_CONN_CLOSE)
    {
        close_and_release(epollfd, event, socket_watcher);  // 断开
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