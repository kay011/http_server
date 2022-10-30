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

class EventHandlerIface
{
public:
	virtual int on_accept(EpollContext &epoll_context) = 0;

	virtual int on_readable(EpollContext &epoll_context, char *read_buffer, int buffer_size, int read_size) = 0;

	virtual int on_writeable(EpollContext &epoll_context) = 0;

	virtual int on_close(EpollContext &epoll_context) = 0;
};

#endif