#ifndef EVENTLOOP_H
#define EVENTLOOP_H
#include "event_handler.h"
class EventLoop{
public:
    void init(EventHandlerIface* socket_watcher);
    void loop(int listen_fd);
    int get_epoll_fd() const { return epoll_fd_; }
private:
	int close_and_release(int epollfd, epoll_event &event, EventHandlerIface* socket_watcher);
	int handle_accept_event(int epollfd, epoll_event &event, EventHandlerIface* socket_watcher);
	int handle_readable_event(int epollfd, epoll_event &event, EventHandlerIface* socket_watcher);
	int handle_writeable_event(int epollfd, epoll_event &event, EventHandlerIface* socket_watcher);
private: 
    int epoll_fd_;
    EventHandlerIface* socket_watcher_;
};
#endif 