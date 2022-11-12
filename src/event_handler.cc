#include "event_handler.h"

#include <sys/socket.h>

#include "unistd.h"

void HttpEventHandler::add_mapping(const std::string& path, method_handler_callback handler, HttpMethod method) {
  Resource resource = {method, handler, nullptr};
  resource_map_[path] = resource;
}

void HttpEventHandler::add_mapping(const std::string& path, json_handler_callback handler, HttpMethod method) {
  Resource resource = {method, nullptr, handler};
  resource_map_[path] = resource;
}

int HttpEventHandler::handle_request(std::shared_ptr<Request> request, std::shared_ptr<Response> response) {
  std::string uri = request->get_request_uri();
  if (this->resource_map_.find(uri) == this->resource_map_.end()) {  // not found
    response->code_msg = STATUS_NOT_FOUND;
    response->body = STATUS_NOT_FOUND.msg;
    LOG(INFO) << "page not found which uri: " << uri.c_str();
    return 0;
  }

  Resource resource = this->resource_map_[request->get_request_uri()];
  // check method
  HttpMethod method = resource.method;
  if (method.name != request->line.method) {
    response->code_msg = STATUS_METHOD_NOT_ALLOWED;
    response->set_head("Allow", method.name);
    response->body.clear();
    LOG(INFO) << "not allow method, allowed: " << method.name.c_str()
              << ", request method: " << request->line.method.c_str();
    return 0;
  }

  if (resource.json_handler_cb != nullptr) {
    Json::Value root;
    // 改为线程池
    resource.json_handler_cb(*request.get(), root);
    response->set_body(root);
  } else if (resource.method_handler_cb != nullptr) {
    resource.method_handler_cb(*request.get(), *response.get());
  }
  LOG(INFO) << "handle response success which code" << response->code_msg.status_code
            << ", msg: " << response->code_msg.msg.c_str();
  return 0;
}

int HttpEventHandler::on_accept(std::shared_ptr<EpollContext> epoll_context) {
  int conn_sock = epoll_context->fd;
  epoll_context->ptr = std::shared_ptr<HttpContext>(new HttpContext(conn_sock), [](HttpContext* hc) { delete hc; });
  return 0;
}

int HttpEventHandler::on_readable(std::shared_ptr<EpollContext> epoll_context, char* read_buffer, int buffer_size,
                                  int read_size) {
  std::shared_ptr<HttpContext> http_context = std::static_pointer_cast<HttpContext>(epoll_context->ptr);
  if (http_context->get_requset()->parse_part == PARSE_REQ_LINE) {
    http_context->record_start_time();
  }

  int ret = http_context->get_requset()->parse_request(read_buffer, read_size);
  if (ret != 0) {
    return ret;
  }

  this->handle_request(http_context->get_requset(), http_context->get_resp());

  return 0;
}

int HttpEventHandler::on_writeable(std::shared_ptr<EpollContext> epoll_context) {
  int fd = epoll_context->fd;
  std::shared_ptr<HttpContext> hc = std::static_pointer_cast<HttpContext>(epoll_context->ptr);
  std::shared_ptr<Response> resp = hc->get_resp();
  bool is_keepalive = (strcasecmp(hc->get_requset()->get_header("Connection").c_str(), "keep-alive") == 0);

  if (!resp->is_writed) {
    resp->gen_response(hc->get_requset()->line.http_version, is_keepalive);
    resp->is_writed = true;
  }

  char buffer[SS_WRITE_BUFFER_SIZE];
  bzero(buffer, SS_WRITE_BUFFER_SIZE);
  int read_size = 0;

  // 1. read some response bytes
  int ret = resp->readsome(buffer, SS_WRITE_BUFFER_SIZE, read_size);
  // 2. write bytes to socket
  int nwrite = send(fd, buffer, read_size, 0);
  if (nwrite < 0) {
    return WRITE_CONN_CLOSE;
  }
  // 3. when not write all buffer, we will rollback write index
  if (nwrite < read_size) {
    resp->rollback(read_size - nwrite);
  }
  LOG(INFO) << "send complete which write_num: " << nwrite << ", read_size: " << read_size;

  bool print_access_log = true;

  if (ret == 1) { /* not send over*/
    print_access_log = false;
    LOG(INFO) << "has big response, we will send part first and send other "
                 "part later ...";
    return WRITE_CONN_CONTINUE;
  }

  if (print_access_log) {
    hc->print_access_log(epoll_context->client_ip);
  }

  if (is_keepalive && nwrite > 0) {
    hc->clear();
    return WRITE_CONN_ALIVE;
  }
  return WRITE_CONN_CLOSE;
}

int HttpEventHandler::on_close(std::shared_ptr<EpollContext> epoll_context) {
  // TODO
  return 0;
}