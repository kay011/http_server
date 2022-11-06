#ifndef HTTP_SERVER_H_
#define HTTP_SERVER_H_

#include <map>
#include <string>

#include "easylogging++.h"
#include "event_handler.h"
#include "eventloop.h"
#include "http_parser.h"
#include "json/json.h"
#include "sys/epoll.h"
class HttpServer {
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
