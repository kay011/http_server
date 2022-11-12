#ifndef EPOLL_SOCKET_H
#define EPOLL_SOCKET_H
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "easylogging++.h"
#include "global.h"
#include "http_parser.h"
#include "sys/epoll.h"

class EventLoop;
struct EpollContext {
  std::shared_ptr<void> ptr;
  int fd;
  std::string client_ip;

  std::vector<char> read_buffer;
  int read_size;
  EventLoop *loop;
  long nearest_active_time;
};

class EventHandlerIface {
 public:
  virtual int on_accept(std::shared_ptr<EpollContext> epoll_context) = 0;
  virtual int on_readable(std::shared_ptr<EpollContext> epoll_context, char *read_buffer, int buffer_size,
                          int read_size) = 0;
  virtual int on_writeable(std::shared_ptr<EpollContext> epoll_context) = 0;
  virtual int on_close(std::shared_ptr<EpollContext> epoll_context) = 0;
};

// impl
class HttpEventHandler : public EventHandlerIface {
 public:
  void add_mapping(const std::string &path, method_handler_callback handler, HttpMethod method);
  void add_mapping(const std::string &path, json_handler_callback handler, HttpMethod method);

 private:
  int handle_request(std::shared_ptr<Request> request, std::shared_ptr<Response> response);

 public:
  virtual ~HttpEventHandler() = default;
  int on_accept(std::shared_ptr<EpollContext> epoll_context) override;
  int on_readable(std::shared_ptr<EpollContext> epoll_context, char *read_buffer, int buffer_size,
                  int read_size) override;
  int on_writeable(std::shared_ptr<EpollContext> epoll_context) override;
  int on_close(std::shared_ptr<EpollContext> epoll_context) override;

 private:
  std::map<std::string, Resource> resource_map_;  // 维护 uri和业务处理函数的映射
};

#endif