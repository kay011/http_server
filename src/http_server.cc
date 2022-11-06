#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/epoll.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <sstream>
#include "http_parser.h"
#include "http_server.h"
#include "utils.h"


HttpServer::HttpServer(int port): port_(port){
	listen_fd_ = SocketUtils::listen_on(port_, 10);
	http_handler_ = std::make_shared<HttpEventHandler>();
	loop_.init(http_handler_.get(), listen_fd_);
	loop_.add_to_poller(listen_fd_, EPOLLIN);
}
HttpServer::~HttpServer(){

}

int HttpServer::start()
{
	loop_.loop();
	return 0;
}

void HttpServer::post(std::string path, method_handler_callback handler)
{
	http_handler_->add_mapping(path, handler, POST_METHOD);
}
void HttpServer::post(std::string path, json_handler_callback handler)
{
	http_handler_->add_mapping(path, handler, POST_METHOD);
}

void HttpServer::get(std::string path, method_handler_callback handler)
{
	http_handler_->add_mapping(path, handler, GET_METHOD);
}

void HttpServer::get(std::string path, json_handler_callback handler)
{
	http_handler_->add_mapping(path, handler, GET_METHOD);
}
