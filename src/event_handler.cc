#include "event_handler.h"

#include <sys/socket.h>

#include "unistd.h"

void HttpEventHandler::add_mapping(std::string path,
                                   method_handler_callback handler,
                                   HttpMethod method) {
  Resource resource = {method, handler, nullptr};
  resource_map_[path] = resource;
}

void HttpEventHandler::add_mapping(std::string path,
                                   json_handler_callback handler,
                                   HttpMethod method) {
  Resource resource = {method, nullptr, handler};
  resource_map_[path] = resource;
}

int HttpEventHandler::handle_request(Request &req, Response &res) {
  std::string uri = req.get_request_uri();
  if (this->resource_map_.find(uri) ==
      this->resource_map_.end()) {  // not found
    res.code_msg = STATUS_NOT_FOUND;
    res.body = STATUS_NOT_FOUND.msg;
    LOG(INFO) << "page not found which uri: " << uri.c_str();
    return 0;
  }

  Resource resource = this->resource_map_[req.get_request_uri()];
  // check method
  HttpMethod method = resource.method;
  if (method.name != req.line.method) {
    res.code_msg = STATUS_METHOD_NOT_ALLOWED;
    res.set_head("Allow", method.name);
    res.body.clear();
    LOG(INFO) << "not allow method, allowed: " << method.name.c_str()
              << ", request method: " << req.line.method.c_str();
    return 0;
  }

  if (resource.json_handler_cb != nullptr) {
    Json::Value root;
    // 改为线程池
    resource.json_handler_cb(req, root);
    res.set_body(root);
  } else if (resource.method_handler_cb != nullptr) {
    resource.method_handler_cb(req, res);
  }
  LOG(INFO) << "handle response success which code" << res.code_msg.status_code
            << ", msg: " << res.code_msg.msg.c_str();
  return 0;
}

int HttpEventHandler::on_accept(EpollContext &epoll_context) {
  int conn_sock = epoll_context.fd;
  epoll_context.ptr = new HttpContext(conn_sock);
  return 0;
}

int HttpEventHandler::on_readable(EpollContext &epoll_context,
                                  char *read_buffer, int buffer_size,
                                  int read_size) {
  HttpContext *http_context = (HttpContext *)epoll_context.ptr;
  if (http_context->get_requset().parse_part == PARSE_REQ_LINE) {
    http_context->record_start_time();
  }

  int ret = http_context->get_requset().parse_request(read_buffer, read_size);
  if (ret != 0) {
    return ret;
  }

  this->handle_request(http_context->get_requset(), http_context->get_res());

  return 0;
}

int HttpEventHandler::on_writeable(EpollContext &epoll_context) {
  int fd = epoll_context.fd;
  HttpContext *hc = (HttpContext *)epoll_context.ptr;
  Response &res = hc->get_res();
  bool is_keepalive =
      (strcasecmp(hc->get_requset().get_header("Connection").c_str(),
                  "keep-alive") == 0);

  if (!res.is_writed) {
    res.gen_response(hc->get_requset().line.http_version, is_keepalive);
    res.is_writed = true;
  }

  char buffer[SS_WRITE_BUFFER_SIZE];
  bzero(buffer, SS_WRITE_BUFFER_SIZE);
  int read_size = 0;

  // 1. read some response bytes
  int ret = res.readsome(buffer, SS_WRITE_BUFFER_SIZE, read_size);
  // 2. write bytes to socket
  int nwrite = send(fd, buffer, read_size, 0);
  if (nwrite < 0) {
    // perror("send fail!");
    return WRITE_CONN_CLOSE;
  }
  // 3. when not write all buffer, we will rollback write index
  if (nwrite < read_size) {
    res.rollback(read_size - nwrite);
  }
  LOG(INFO) << "send complete which write_num: " << nwrite
            << ", read_size: " << read_size;

  bool print_access_log = true;

  if (ret == 1) { /* not send over*/
    print_access_log = false;
    LOG(INFO) << "has big response, we will send part first and send other "
                 "part later ...";
    return WRITE_CONN_CONTINUE;
  }

  if (print_access_log) {
    hc->print_access_log(epoll_context.client_ip);
  }

  if (is_keepalive && nwrite > 0) {
    hc->clear();
    return WRITE_CONN_ALIVE;
  }
  return WRITE_CONN_CLOSE;
}

int HttpEventHandler::on_close(EpollContext &epoll_context) {
  if (epoll_context.ptr == NULL) {
    return 0;
  }
  HttpContext *hc = (HttpContext *)epoll_context.ptr;
  if (hc != NULL) {
    delete hc;
    hc = NULL;
  }
  return 0;
}