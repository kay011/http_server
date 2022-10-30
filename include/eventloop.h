#ifndef EVENTLOOP_H
#define EVENTLOOP_H
#include "event_handler.h"
#include "event_epoll.h"
#include <memory>

class EventLoop{
public:
    EventLoop();
    ~EventLoop();
    void init(EventHandlerIface* socket_watcher);
    void loop(int listen_fd);
    int get_epoll_fd() const { return poller_->get_epoll_fd(); }

    void add_to_poller(int fd, epoll_event &event);
    void update_to_poller(int fd, epoll_event &event);
    void remove_from_poller(int fd, epoll_event &event);
private:
	int close_and_release(epoll_event &event, EventHandlerIface* socket_watcher);
	int handle_accept_event(epoll_event &event, EventHandlerIface* socket_watcher);
	int handle_readable_event(epoll_event &event, EventHandlerIface* socket_watcher);
	int handle_writeable_event(epoll_event &event, EventHandlerIface* socket_watcher);
private: 
    EventHandlerIface* socket_watcher_;
    std::shared_ptr<Epoll> poller_;

};
#endif 