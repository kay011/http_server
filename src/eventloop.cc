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

void EventLoop::loop() {
  int max_events = 1000;
  epoll_event* events = new epoll_event[max_events];
  while (true) {
    // 定时事件
    // 从一个map中取最近即将超时的事件
    // for(auto it = fd2expire_time_.begin(); it != fd2expire_time_.end();
    // ++it){
    //     LOG(INFO) << "fd: " <<it->first << "-" << "expire_time: " <<
    //     it->second;
    // }
    // LOG(INFO) << "yhd_size: " <<fd2expire_time_.size();
    int timeout = -1;
    // LOG(INFO) << "timer_manager_.size(): " << timer_manager_.size();
    // if(timer_manager_.size() > 0){
    //     LOG(INFO) << "yhd_test";
    //     TimeNode tn = timer_manager_.GetNearbyTimeNode();
    //     std::chrono::milliseconds now_ms = std::chrono::duration_cast<
    //     std::chrono::milliseconds >(
    //         std::chrono::system_clock::now().time_since_epoch());
    //     long diff_time = now_ms.count() - tn.last_active_time_;
    //     LOG(INFO) << "yhd diff_time: " << diff_time;
    //     if (diff_time - EXPIRE_TIME >= 0) // 说明已经有超时事件了
    //     {
    //         timeout = 0;
    //     } else {    // 说明最近的还没有超时
    //         timeout = EXPIRE_TIME - diff_time;
    //     }
    // }
    LOG(INFO) << "timeout: " << timeout;
    int fds_num = this->poller_->poller(events, max_events, timeout);
    // int fds_num = epoll_wait(this->get_epoll_fd(), events, max_events, -1);
    // LOG(INFO) << "fds_num" << fds_num;
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
        LOG(WARNING) << "unkonw events :" << events[i].events;
      }
    }

    // 处理时间事件
    // this->handle_timeout_event();
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
  auto fd_iter = fd2context_.find(fd);
  if (fd_iter != fd2context_.end()) {
    EpollContext* hc = fd_iter->second;
    assert(hc != NULL);
    __close_and_release(hc);
    fd2context_.erase(fd_iter);
  }
  LOG(INFO) << "connect close";
  return 0;
}

int EventLoop::__close_and_release(EpollContext* context) {
  LOG(INFO) << "access __close_and_release";
  socket_watcher_->on_close(*context);
  int fd = context->fd;

  uint32_t events = EPOLLIN | EPOLLOUT | EPOLLET;
  this->remove_from_poller(fd, events);
  // epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, &event);
  // 删除read_buffer;
  if (context->read_buffer != NULL) {
    delete context->read_buffer;
    context->read_buffer = NULL;
  }
  if (context != NULL) {
    delete context;
    context = NULL;
  }
  int ret = close(fd);
  LOG(INFO) << "connect close complete which fd: " << fd << ", ret: " << ret;
  return ret;
}

int EventLoop::handle_accept_event(int fd) {
  int listenfd = fd;

  std::string client_ip;
  int conn_fd = SocketUtils::accept_socket(listenfd, client_ip);
  if (conn_fd == -1) {
    return -1;
  }
  SocketUtils::set_nonblocking(conn_fd);
  LOG(INFO) << "get accept socket which listen fd:" << listenfd << ",conn_fd: " << conn_fd;

  EpollContext* epoll_context = new EpollContext();
  epoll_context->fd = conn_fd;
  epoll_context->client_ip = client_ip;
  epoll_context->loop = this;
  this->fd2context_[conn_fd] = epoll_context;  // 交由 eventloop 管理

  socket_watcher_->on_accept(*epoll_context);

  // struct epoll_event conn_fd_ev;
  // conn_fd_ev.data.fd = conn_fd;
  // conn_fd_ev.events = EPOLLIN | EPOLLET;
  this->add_to_poller(conn_fd, EPOLLIN | EPOLLET);

  return 0;
}

int EventLoop::handle_readable_event(int fd) {
  // 从map中找，如果没找到，直接fatal
  auto fd_iter = fd2context_.find(fd);
  if (fd_iter == fd2context_.end()) {
    LOG(FATAL) << "can not find fd in eventloop";
    return -1;
  }

  EpollContext* epoll_context = fd_iter->second;
  assert(epoll_context->ptr != NULL);
  epoll_context->buffer_size = SS_READ_BUFFER_SIZE;
  epoll_context->read_buffer = new char[epoll_context->buffer_size];
  memset(epoll_context->read_buffer, 0, epoll_context->buffer_size);
  // int read_size = recv(fd, read_buffer, buffer_size, 0);
  assert(fd == epoll_context->fd);
  epoll_context->read_size = SocketUtils::readn(fd, epoll_context->read_buffer, epoll_context->buffer_size);
  // pthread_t tid;
  // int ret = pthread_create(&tid, NULL, biz_routine, epoll_context);
  // if(ret != 0){
  //     LOG(FATAL) << "pthread_create err";
  // }
  // pthread_detach(tid);
  // biz_routine(epoll_context);
  g_work_pool.enqueue(biz_routine, epoll_context);
  return 0;
}

int EventLoop::handle_writeable_event(int fd) {
  // 从map中找，如果没找到，直接fatal
  auto fd_iter = fd2context_.find(fd);
  if (fd_iter == fd2context_.end()) {
    LOG(FATAL) << "can not find fd in eventloop";
    return -1;
  }
  EpollContext* epoll_context = fd_iter->second;
  // int fd = epoll_context->fd;
  assert(fd == epoll_context->fd);
  LOG(INFO) << "start write data";

  int ret = socket_watcher_->on_writeable(*epoll_context);
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
  // epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
  return 0;
}

int EventLoop::handle_timeout_event() {
  LOG(INFO) << "yhd access handle_timeout_event";
  LOG(INFO) << "yhd timer_manager_.size(): " << timer_manager_.size();
  while (timer_manager_.size() > 0) {
    // 先取出top
    TimeNode tn = timer_manager_.GetNearbyTimeNode();
    std::chrono::milliseconds now_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
    long diff_time = now_ms.count() - tn.last_active_time_;
    LOG(INFO) << "diff_time: " << diff_time;
    if (diff_time - EXPIRE_TIME >= 0) {
      if (tn.ptr_ == NULL) {
        timer_manager_.PopTopTimeNode();
        continue;
      }
      EpollContext* context = (EpollContext*)tn.ptr_;
      // 断开连接
      __close_and_release(context);
      // 清理内存
      fd2context_.erase(context->fd);

      timer_manager_.PopTopTimeNode();
    } else {
      break;
    }
  }
  return 0;
}

void* biz_routine(void* args) {
  EpollContext* epoll_context = (EpollContext*)args;
  assert(epoll_context != NULL);
  EventLoop* loop = epoll_context->loop;
  char* read_buffer = epoll_context->read_buffer;
  int buffer_size = epoll_context->buffer_size;
  int read_size = epoll_context->read_size;
  int fd = epoll_context->fd;
  int handle_ret = 0;
  if (read_size > 0) {
    LOG(INFO) << "read success which read size: " << read_size;
    handle_ret = loop->get_socket_watcher()->on_readable(*epoll_context, read_buffer, buffer_size, read_size);
  }
  if (read_size <= 0 || handle_ret < 0) {
    loop->close_and_release(fd);
    return 0;
  }
  uint32_t events;
  if (handle_ret == READ_CONTINUE) {
    events = EPOLLIN | EPOLLET;
  } else {
    events = EPOLLOUT | EPOLLET;
  }
  loop->update_to_poller(fd, events);
}