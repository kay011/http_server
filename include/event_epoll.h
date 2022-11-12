#ifndef EVENT_EPOLL_H
#define EVENT_EPOLL_H
#include <sys/epoll.h>

#include "stdint.h"

class Epoll {
 public:
  Epoll();
  ~Epoll() = default;

  int poller(epoll_event* events, int max_events, int timeout);
  void add_to_poller(int fd, uint32_t events);
  void update_to_poller(int fd, uint32_t events);
  void remove_from_poller(int fd, uint32_t events);
  int get_epoll_fd() const { return epoll_fd_; }

 private:
  int epoll_fd_;
};
#endif