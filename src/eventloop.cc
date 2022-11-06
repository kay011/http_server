#include "eventloop.h"
#include <sys/epoll.h>
#include "easylogging++.h"
#include "utils.h"
#include <unistd.h>
#include <cstdlib>
#include <cstdio>

EventLoop::EventLoop(){
    poller_ = std::make_shared<Epoll>();
}

EventLoop::~EventLoop(){

}

void EventLoop::init(EventHandlerIface* socket_watcher){
    socket_watcher_ = socket_watcher;
}  

void EventLoop::loop(int listenfd){
    int max_events = 1000;
    epoll_event *events = new epoll_event[max_events];
    while (true)
    {
        // 定时事件
        // 从一个map中取最近即将超时的事件
        // for(auto it = fd2expire_time_.begin(); it != fd2expire_time_.end(); ++it){
        //     LOG(INFO) << "fd: " <<it->first << "-" << "expire_time: " << it->second;
        // }
        // LOG(INFO) << "yhd_size: " <<fd2expire_time_.size();
        int timeout = -1;
        LOG(INFO) << "timer_manager_.size(): " << timer_manager_.size();
        if(timer_manager_.size() > 0){
            LOG(INFO) << "yhd_test";
            TimeNode tn = timer_manager_.GetNearbyTimeNode();
            std::chrono::milliseconds now_ms = std::chrono::duration_cast< std::chrono::milliseconds >(
                std::chrono::system_clock::now().time_since_epoch());
            long diff_time = now_ms.count() - tn.last_active_time_;
            LOG(INFO) << "yhd diff_time: " << diff_time;
            if (diff_time - EXPIRE_TIME >= 0) // 说明已经有超时事件了
            {
                timeout = 0;
            } else {    // 说明最近的还没有超时
                timeout = EXPIRE_TIME - diff_time;
            }
        }
        LOG(INFO) << "timeout: " << timeout;
        int fds_num = this->poller_->poller(events, max_events, timeout);
        // int fds_num = epoll_wait(this->get_epoll_fd(), events, max_events, -1);
        // LOG(INFO) << "fds_num" << fds_num;
        if (fds_num == -1)
        {
            LOG(ERROR) << "epoll_wait err";
        }
        // 处理文件事件
        for (int i = 0; i < fds_num; i++)
        {
            if (events[i].data.fd == listenfd)
            {
                // accept connection
                this->handle_accept_event(events[i]);
            }
            else if (events[i].events & EPOLLIN)
            {
                // readable
                this->handle_readable_event(events[i]);
            }
            else if (events[i].events & EPOLLOUT)
            {
                // writeable
                this->handle_writeable_event(events[i]);
            }
            else
            {
                LOG(WARNING) << "unkonw events :" << events[i].events;
            }
        }

        // 处理时间事件
        this->handle_timeout_event();
    }

    if (events != NULL)
    {
        delete[] events;
        events = NULL;
    }
}

void EventLoop::add_to_poller(int fd, epoll_event &event){
    poller_->add_to_poller(fd, event);
}
void EventLoop::update_to_poller(int fd, epoll_event &event){
    poller_->update_to_poller(fd, event);
}
void EventLoop::remove_from_poller(int fd, epoll_event &event){
    poller_->remove_from_poller(fd, event);
}


int EventLoop::close_and_release(epoll_event &event){
    if (event.data.ptr == NULL)
    {
        return 0;
    }

    LOG(INFO) << "connect close";

    EpollContext *hc = (EpollContext *)event.data.ptr;

    __close_and_release(hc);
    return 0;
}

int EventLoop::__close_and_release(EpollContext* context){
    LOG(INFO) << "access __close_and_release";
    socket_watcher_->on_close(*context);
    int fd = context->fd;
    struct epoll_event event;
    event.data.fd = fd;
    // event.events = EPOLLIN | EPOLLOUT | EPOLLET;
    this->remove_from_poller(fd, event);
    // epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, &event);
    if (context != NULL){
        delete context;
        context = NULL;
    }
    int ret = close(fd);
    LOG(INFO) << "connect close complete which fd: " << fd << ", ret: " << ret;
    return ret;
}

int EventLoop::handle_accept_event(epoll_event &event){
    int listenfd = event.data.fd;

    std::string client_ip;
    int conn_sock = SocketUtils::accept_socket(listenfd, client_ip);
    if (conn_sock == -1)
    {
        return -1;
    }
    SocketUtils::set_nonblocking(conn_sock);
    LOG(INFO) << "get accept socket which listen fd:" << listenfd << ",conn_sock_fd: " << conn_sock;

    EpollContext *epoll_context = new EpollContext();
    epoll_context->fd = conn_sock;
    epoll_context->client_ip = client_ip;
    this->fd2context_[conn_sock]  = epoll_context;
    this->timer_manager_.AddToTimer(epoll_context);

    socket_watcher_->on_accept(*epoll_context);

    // this->poller_->add_to_poller(conn_sock, EPOLLIN | EPOLLET, epoll_context);
    struct epoll_event conn_sock_ev;
    conn_sock_ev.events = EPOLLIN | EPOLLET;
    conn_sock_ev.data.ptr = epoll_context;
    this->add_to_poller(conn_sock, conn_sock_ev);

    return 0;
}

int EventLoop::biz_routine(epoll_event &event, char* read_buffer, int buffer_size, int read_size){
    EpollContext *epoll_context = (EpollContext *)event.data.ptr;
    int fd = epoll_context->fd;
    int handle_ret = 0;
    if (read_size > 0)
    {
        LOG(INFO) << "read success which read size: " << read_size;
        handle_ret = socket_watcher_->on_readable(*epoll_context, read_buffer, buffer_size, read_size);
    }
    if (read_size <= 0 || handle_ret < 0)
    {
        this->close_and_release(event);
        return 0;
    }
    if (handle_ret == READ_CONTINUE)
    {
        event.events = EPOLLIN | EPOLLET;
    }
    else
    {
        event.events = EPOLLOUT | EPOLLET;
    }
    // epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
    this->update_to_poller(fd, event);
    return 0;
}

int EventLoop::handle_readable_event(epoll_event &event){
    EpollContext *epoll_context = (EpollContext *)event.data.ptr;
    int fd = epoll_context->fd;
    int buffer_size = SS_READ_BUFFER_SIZE;
    char read_buffer[buffer_size];
    memset(read_buffer, 0, buffer_size);
    // int read_size = recv(fd, read_buffer, buffer_size, 0);
    int read_size = SocketUtils::readn(fd, read_buffer, buffer_size);

    // 异步
    // 先抽出一个函数出来
    this->biz_routine(event, read_buffer, buffer_size, read_size);
    return 0;
}

int EventLoop::handle_writeable_event(epoll_event &event){
    EpollContext *epoll_context = (EpollContext *)event.data.ptr;
    int fd = epoll_context->fd;
    LOG(INFO) << "start write data";

    int ret = socket_watcher_->on_writeable(*epoll_context);
    if (ret == WRITE_CONN_CLOSE)
    {
        close_and_release(event);  // 断开
        return 0;
    }

    if (ret == WRITE_CONN_CONTINUE)
    {
        event.events = EPOLLOUT | EPOLLET;
    }
    else // WRITE_CONN_ALIVE
    {
        event.events = EPOLLIN | EPOLLET; // 长连接
    }
    this->update_to_poller(fd, event);
    // epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
    return 0;
}

int EventLoop::handle_timeout_event(){
    LOG(INFO) << "yhd access handle_timeout_event";
    LOG(INFO) << "yhd timer_manager_.size(): " << timer_manager_.size();
    while(timer_manager_.size() > 0){
        // 先取出top
        TimeNode tn =timer_manager_.GetNearbyTimeNode();
        std::chrono::milliseconds now_ms = std::chrono::duration_cast< std::chrono::milliseconds >(
            std::chrono::system_clock::now().time_since_epoch());
        long diff_time = now_ms.count() - tn.last_active_time_;
        LOG(INFO) << "diff_time: " << diff_time;
        if(diff_time - EXPIRE_TIME >= 0){
            if(tn.ptr_ == NULL){
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