#ifndef HTTP_SERVER_H_
#define HTTP_SERVER_H_

#include <string>
#include <map>
#include <functional>
#include "sys/epoll.h"
#include "json/json.h"
#include "event_handler.h"
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

typedef std::function<void(Request &request, Response &response)> method_handler_callback;
typedef std::function<void(Request &request, Json::Value &response)> json_handler_callback;

struct Resource
{
	HttpMethod method;
	method_handler_callback method_handler_cb;
	json_handler_callback json_handler_cb;
};

class HttpEventHandler : public EventHandlerIface
{
public:
	

	void add_mapping(std::string path, method_handler_callback handler, HttpMethod method = GET_METHOD);

	void add_mapping(std::string path, json_handler_callback handler, HttpMethod method = GET_METHOD);

	int handle_request(Request &request, Response &response);
public:
	virtual ~HttpEventHandler() {}
	int on_accept(EpollContext &epoll_context) override;
	int on_readable(EpollContext &epoll_context, char *read_buffer, int buffer_size, int read_size) override;
	int on_writeable(EpollContext &epoll_context) override;
	int on_close(EpollContext &epoll_context) override;
private:
	std::map<std::string, Resource> resource_map_;
};

class HttpServer
{
public:
	HttpServer(int port);
	~HttpServer();
	void post(std::string path, method_handler_callback handler);
	void post(std::string path, json_handler_callback handler);
	void get(std::string path, method_handler_callback handler);
	void get(std::string path, json_handler_callback handler);

	int start();

private:
	int port_;
	int listen_fd_;
	std::shared_ptr<HttpEventHandler> http_handler_;
	EventLoop loop_;
};

#endif /* HTTP_SERVER_H_ */
