#include "eventloop.h"

#include <sys/epoll.h>
#include <unistd.h>

#include <cassert>
#include <cstdio>
#include <cstdlib>

#include "easylogging++.h"
#include "thread_pool.hpp"
#include "utils.h"

// extern ThreadPool g_work_pool;

EventLoop::EventLoop() { poller_ = std::make_shared<Epoll>(); }

EventLoop::~EventLoop() {}

void EventLoop::init(EventHandlerIface* socket_watcher, int listen_fd) {
  socket_watcher_ = socket_watcher;
  listen_fd_ = listen_fd;
}

int EventLoop::calc_timeout() {
  // 定时事件
  int timeout = -1;
  if (timer_manager_.size() > 0) {
    TimeNode tn = timer_manager_.GetNearbyTimeNode();
    long unix_now = SocketUtils::unix_now_millisecond();
    long diff_time = unix_now - tn.last_active_time_;
    if (diff_time >= EXPIRE_TIME)  // 说明已经有超时事件了
    {
      timeout = 10;
    } else {  // 说明最近的还没有超时
      timeout = EXPIRE_TIME - diff_time;
    }
  }
  return timeout;
}

void EventLoop::loop() {
  int max_events = 1000;
  epoll_event* events = new epoll_event[max_events];
  while (true) {
    int timeout = calc_timeout();
    LOG(INFO) << "timeout: " << timeout;
    int fds_num = this->poller_->poller(events, max_events, timeout);
    LOG(INFO) << "fds_num" << fds_num;
    if (fds_num == -1) {
      LOG(ERROR) << "epoll_wait err";
    }
    // 处理文件事件
    for (int i = 0; i < fds_num; i++) {
      if (events[i].data.fd == this->listen_fd_) {
        // accept connection
        this->handle_accept_event(events[i].data.fd);
      } else if (events[i].events & EPOLLIN) {
        // readable
        this->handle_readable_event(events[i].data.fd);
      } else if (events[i].events & EPOLLOUT) {
        // writeable
        this->handle_writeable_event(events[i].data.fd);
      } else {
        LOG(WARNING) << "unknown events :" << events[i].events;
      }
    }

    // 处理时间事件
    this->handle_timeout_event();
  }

  if (events != NULL) {
    delete[] events;
    events = NULL;
  }
}

void EventLoop::add_to_poller(int fd, uint32_t events) { poller_->add_to_poller(fd, events); }
void EventLoop::update_to_poller(int fd, uint32_t events) { poller_->update_to_poller(fd, events); }
void EventLoop::remove_from_poller(int fd, uint32_t events) { poller_->remove_from_poller(fd, events); }

int EventLoop::close_and_release(int fd) {
  int ret = 0;
  auto fd_iter = fd2context_.find(fd);
  if (fd_iter != fd2context_.end()) {
    auto epoll_context = fd_iter->second;
    if (epoll_context == nullptr) {
      fd2context_.erase(fd_iter);
      return 0;
    }
    assert(epoll_context->fd == fd);
    ret = _close_and_release(epoll_context);
  }
  LOG(INFO) << "fd: " << fd << " connect close";
  return ret;
}

int EventLoop::_close_and_release(std::shared_ptr<EpollContext> context) {
  LOG(INFO) << "access _close_and_release";
  //  this->socket_watcher_->on_close(context);
  int fd = context->fd;

  uint32_t events = EPOLLIN | EPOLLOUT | EPOLLET;
  this->remove_from_poller(fd, events);
  int ret = close(fd);
  LOG(INFO) << "connect close complete which fd: " << fd << ", ret: " << ret;
  return ret;
}

int EventLoop::handle_accept_event(int fd) {
  int listenfd = fd;

  std::string client_ip;
  int client_port;
  int conn_fd = SocketUtils::accept_socket(listenfd, client_ip, client_port);
  if (conn_fd == -1) {
    return -1;
  }
  SocketUtils::set_nonblocking(conn_fd);
  LOG(INFO) << "get accept socket which listen fd:" << listenfd << ",conn_fd: " << conn_fd << "client_ip: " << client_ip
            << " client_port: " << client_port;

  // EpollContext* epoll_context = new EpollContext();  // 构建上下文
  this->fd2context_[conn_fd] = make_shared<EpollContext>();
  this->fd2context_[conn_fd]->fd = conn_fd;
  this->fd2context_[conn_fd]->client_ip = client_ip;
  this->fd2context_[conn_fd]->loop = this;
  this->fd2context_[conn_fd]->nearest_active_time = SocketUtils::unix_now_millisecond();
  this->socket_watcher_->on_accept(this->fd2context_[conn_fd]);
  this->add_to_poller(conn_fd, EPOLLIN | EPOLLET);

  // 加入定时器容器
  this->timer_manager_.AddToTimer(this->fd2context_[conn_fd]->nearest_active_time, this->fd2context_[conn_fd]);
  return 0;
}

int EventLoop::handle_readable_event(int fd) {
  // 从map中找，如果没找到，直接fatal
  auto fd_iter = fd2context_.find(fd);
  if (fd_iter == fd2context_.end()) {
    LOG(FATAL) << "can not find fd in eventloop";
    return -1;
  }

  auto epoll_context = fd_iter->second;
  assert(epoll_context->ptr != nullptr);
  epoll_context->nearest_active_time = SocketUtils::unix_now_millisecond();
  epoll_context->read_buffer.resize(SS_READ_BUFFER_SIZE, '\0');
  epoll_context->read_size = (int)SocketUtils::readn(fd, epoll_context->read_buffer.data(), SS_READ_BUFFER_SIZE);

  // async
  g_work_pool.enqueue(biz_routine, epoll_context);
  return 0;
}

int EventLoop::handle_writeable_event(int fd) {
  auto fd_iter = fd2context_.find(fd);
  if (fd_iter == fd2context_.end()) {
    LOG(FATAL) << "can not find fd in eventloop";
    return -1;
  }
  auto epoll_context = fd_iter->second;
  // int fd = epoll_context->fd;
  assert(fd == epoll_context->fd);
  LOG(INFO) << "start write data";
  epoll_context->nearest_active_time = SocketUtils::unix_now_millisecond();
  int ret = socket_watcher_->on_writeable(epoll_context);
  if (ret == WRITE_CONN_CLOSE) {
    close_and_release(fd);  // 断开
    return 0;
  }
  uint32_t events;
  if (ret == WRITE_CONN_CONTINUE) {
    events = EPOLLOUT | EPOLLET;
  } else  // WRITE_CONN_ALIVE
  {
    events = EPOLLIN | EPOLLET;  // 长连接
  }
  this->update_to_poller(fd, events);
  return 0;
}

int EventLoop::handle_timeout_event() {
  LOG(INFO) << "access handle_timeout_event";
  while (timer_manager_.size() > 0) {
    // 先取出top
    TimeNode tn = timer_manager_.GetNearbyTimeNode();
    if (tn.ptr_ == nullptr) {
      timer_manager_.PopTopTimeNode();
      continue;
    }
    auto epoll_context = tn.ptr_;
    // 如果最近活跃时间 - now >= EXPIRE_TIME, 直接删除
    long unix_now = SocketUtils::unix_now_millisecond();
    if (unix_now - epoll_context->nearest_active_time >= EXPIRE_TIME) {
      LOG(INFO) << "del conn fd: " << epoll_context->fd;
      _close_and_release(epoll_context);
      // 清理内存
      fd2context_.erase(epoll_context->fd);
      timer_manager_.PopTopTimeNode();
    } else {
      if (epoll_context->nearest_active_time > tn.last_active_time_) {
        timer_manager_.PopTopTimeNode();
        timer_manager_.AddToTimer(epoll_context->nearest_active_time, epoll_context);
      } else {  // epoll_context->nearest_active_time == tn.last_active_time_ 且没超时
        break;  // 避免全部遍历
      }
    }
  }
  return 0;
}

void biz_routine(std::shared_ptr<EpollContext> epoll_context) {
  EventLoop* loop = epoll_context->loop;
  auto read_buffer = epoll_context->read_buffer;
  int buffer_size = (int)epoll_context->read_buffer.size();
  int read_size = epoll_context->read_size;
  int fd = epoll_context->fd;
  int handle_ret = 0;
  if (read_size > 0) {
    LOG(INFO) << "read success which read size: " << read_size;
    handle_ret = loop->get_socket_watcher()->on_readable(epoll_context, read_buffer.data(), buffer_size, read_size);
  }
  if (read_size <= 0 || handle_ret < 0) {
    loop->close_and_release(fd);
    return;
  }
  uint32_t events;
  if (handle_ret == READ_CONTINUE) {
    events = EPOLLIN | EPOLLET;
  } else {
    events = EPOLLOUT | EPOLLET;
  }
  loop->update_to_poller(fd, events);
}