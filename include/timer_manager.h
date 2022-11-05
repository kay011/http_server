#include <iostream>
#include <stdio.h>
#include <sstream>
#include <vector>
#include <unistd.h>
#include <queue>
#include <chrono>
#include "easylogging++.h"
using namespace std;

struct TimeNode {
    void *ptr_;
    long last_active_time_;
    TimeNode(void* ptr, long last_active_time) : ptr_(ptr), last_active_time_(last_active_time){}
    bool operator< (const TimeNode& rhs) const {
        if(this->last_active_time_ < rhs.last_active_time_){
            return true;
        }
        return false;
    }
    bool operator> (const TimeNode& rhs) const {
        if(this->last_active_time_ > rhs.last_active_time_){
            return true;
        }
        return false;
    }
};


class TimerManager{
public:
    void AddToTimer(void* ptr){
        std::chrono::milliseconds ms = std::chrono::duration_cast< std::chrono::milliseconds >(
                std::chrono::system_clock::now().time_since_epoch());
        LOG(INFO) << "timestamp: " << ms.count();
        pq.push(TimeNode(ptr, ms.count()));
        LOG(INFO) << "pq.size: "<<pq.size();
    }
    const TimeNode& GetNearbyTimeNode(){
        const TimeNode& timeNode = pq.top();
        return timeNode;
    }
    void PopTopTimeNode(){
        pq.pop();
    }
    int size() const {
        return pq.size();
    }

private:
    std::priority_queue<TimeNode, vector<TimeNode>, greater<TimeNode>> pq;
};
