#include "event_epoll.h"

#include <sys/epoll.h>

#include "easylogging++.h"

Epoll::Epoll() {
  epoll_fd_ = epoll_create(1024);
  if (epoll_fd_ == -1) {
    LOG(FATAL) << "epoll_create err";
  }
}

Epoll::~Epoll() {}
int Epoll::poller(epoll_event* events, int max_events, int timeout) {
  // TODO
  int fds_num = epoll_wait(epoll_fd_, events, max_events, timeout);
  return fds_num;
}
void Epoll::add_to_poller(int fd, uint32_t events) {
  struct epoll_event ev;
  ev.data.fd = fd;
  ev.events = events;
  if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) == -1) {
    LOG(ERROR) << "epoll_ctl add err";
  }
}
void Epoll::update_to_poller(int fd, uint32_t events) {
  struct epoll_event ev;
  ev.data.fd = fd;
  ev.events = events;
  if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev) == -1) {
    LOG(ERROR) << "epoll_ctl add err";
  }
}
void Epoll::remove_from_poller(int fd, uint32_t events) {
  struct epoll_event ev;
  ev.data.fd = fd;
  ev.events = events;
  if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, &ev) == -1) {
    LOG(ERROR) << "epoll_ctl add err";
  }
}