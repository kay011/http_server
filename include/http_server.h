#ifndef HTTP_SERVER_H_
#define HTTP_SERVER_H_

#include <string>
#include <map>
#include "sys/epoll.h"
#include "json/json.h"
#include "epoll_socket.h"
#include "http_parser.h"
#include "easylogging++.h"
#include "eventloop.h"

struct HttpMethod
{
	int code;
	std::string name;
};

const static HttpMethod GET_METHOD = {1, "GET"};
const static HttpMethod POST_METHOD = {2, "POST"};

typedef void (*method_handler_ptr)(Request &request, Response &response);
typedef void (*json_handler_ptr)(Request &request, Json::Value &response);

struct Resource
{
	HttpMethod method;
	method_handler_ptr handler_ptr;
	json_handler_ptr json_ptr;
};

class HttpEpollWatcher : public EpollSocketWatcher
{
private:
	std::map<std::string, Resource> resource_map_;

public:
	virtual ~HttpEpollWatcher() {}

	void add_mapping(std::string path, method_handler_ptr handler, HttpMethod method = GET_METHOD);

	void add_mapping(std::string path, json_handler_ptr handler, HttpMethod method = GET_METHOD);

	int handle_request(Request &request, Response &response);

	virtual int on_accept(EpollContext &epoll_context);

	virtual int on_readable(EpollContext &epoll_context, char *read_buffer, int buffer_size, int read_size);

	virtual int on_writeable(EpollContext &epoll_context);

	virtual int on_close(EpollContext &epoll_context);
};

class HttpServer
{
public:
	HttpServer(int port);
	~HttpServer();
	void post(std::string path, method_handler_ptr handler);
	void post(std::string path, json_handler_ptr handler);
	void get(std::string path, method_handler_ptr handler);
	void get(std::string path, json_handler_ptr handler);

	int start();

private:
	int port_;
	int listen_fd_;
	HttpEpollWatcher* http_handler_;
	EventLoop loop_;
	// EpollSocket* epoll_socket_;
};

#endif /* HTTP_SERVER_H_ */
