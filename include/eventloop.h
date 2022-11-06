#ifndef EVENTLOOP_H
#define EVENTLOOP_H
#include <map>
#include <memory>
#include <queue>

#include "event_epoll.h"
#include "event_handler.h"
#include "timer_manager.h"

class EventLoop {
 public:
  EventLoop();
  ~EventLoop();
  void init(EventHandlerIface* socket_watcher, int listen_fd);
  void loop();
  int get_epoll_fd() const { return poller_->get_epoll_fd(); }

  void add_to_poller(int fd, uint32_t events);
  void update_to_poller(int fd, uint32_t events);
  void remove_from_poller(int fd, uint32_t events);

  int close_and_release(int fd);
  int handle_accept_event(int fd);
  int handle_readable_event(int fd);
  int handle_writeable_event(int fd);

  int handle_timeout_event();
  int __close_and_release(EpollContext* context);

 public:
  EventHandlerIface* get_socket_watcher() const { return socket_watcher_; }

 private:
  int listen_fd_;
  EventHandlerIface* socket_watcher_;
  std::shared_ptr<Epoll> poller_;

  std::map<int, EpollContext*> fd2context_;
  TimerManager timer_manager_;
};
void biz_routine(void* args);
#endif