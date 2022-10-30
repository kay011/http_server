#ifndef EVENT_EPOLL_H
#define EVENT_EPOLL_H

class Epoll{
public:
    Epoll();
    ~Epoll();

    int poller(epoll_event* events, int max_events);
    void add_to_poller(int fd, epoll_event &event);
    void update_to_poller(int fd, epoll_event &event);
    void remove_from_poller(int fd, epoll_event &event);
    int get_epoll_fd() const {return epoll_fd_;}
private:
    int epoll_fd_;

};
#endif