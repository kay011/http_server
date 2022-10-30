#ifndef EPOLL_SOCKET_H
#define EPOLL_SOCKET_H
#include "sys/epoll.h"
#include <string>
#include "easylogging++.h"
#include "global.h"
class EpollContext
{
public:
	void *ptr;
	int fd;
	std::string client_ip;
};

class EpollSocketWatcher
{
public:
	virtual int on_accept(EpollContext &epoll_context) = 0;

	virtual int on_readable(EpollContext &epoll_context, char *read_buffer, int buffer_size, int read_size) = 0;

	virtual int on_writeable(EpollContext &epoll_context) = 0;

	virtual int on_close(EpollContext &epoll_context) = 0;
};

class EpollSocket
{
private:

	int close_and_release(int &epollfd, epoll_event &event, EpollSocketWatcher &socket_watcher);

	int handle_accept_event(int &epollfd, epoll_event &event, EpollSocketWatcher &socket_watcher);

	int handle_readable_event(int &epollfd, epoll_event &event, EpollSocketWatcher &socket_watcher);

	int handle_writeable_event(int &epollfd, epoll_event &event, EpollSocketWatcher &socket_watcher);

public:
	int start_epoll(int port, EpollSocketWatcher &socket_watcher, int backlog, int max_events);
};

#endif