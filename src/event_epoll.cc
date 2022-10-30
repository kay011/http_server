#include <sys/epoll.h>
#include "event_epoll.h"
#include "easylogging++.h"

Epoll::Epoll(){
    epoll_fd_ = epoll_create(1024);
    if (epoll_fd_ == -1)
    {
        LOG(FATAL) << "epoll_create err";
    } 
}

Epoll::~Epoll(){

}
int Epoll::poller(epoll_event* events, int max_events){
    // TODO
    int fds_num = epoll_wait(epoll_fd_, events, max_events, -1);
    return fds_num;
}
void Epoll::add_to_poller(int fd, epoll_event &event){
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &event) == -1)
    {
        LOG(ERROR) << "epoll_ctl add err";
    }
}
void Epoll::update_to_poller(int fd, epoll_event &event){
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &event) == -1)
    {
        LOG(ERROR) << "epoll_ctl add err";
    }
}
void Epoll::remove_from_poller(int fd, epoll_event &event){
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, &event) == -1)
    {
        LOG(ERROR) << "epoll_ctl add err";
    }
}