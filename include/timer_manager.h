#include <stdio.h>
#include <unistd.h>

#include <chrono>
#include <iostream>
#include <memory>
#include <queue>
#include <sstream>
#include <vector>

#include "easylogging++.h"
#include "utils.h"
using namespace std;
// class EpollContext;
struct TimeNode {
  std::shared_ptr<EpollContext> ptr_;
  long last_active_time_;
  TimeNode(std::shared_ptr<EpollContext> ptr, long last_active_time) : ptr_(ptr), last_active_time_(last_active_time) {}
  bool operator<(const TimeNode& rhs) const {
    if (this->last_active_time_ < rhs.last_active_time_) {
      return true;
    }
    return false;
  }
  bool operator>(const TimeNode& rhs) const {
    if (this->last_active_time_ > rhs.last_active_time_) {
      return true;
    }
    return false;
  }
};

class TimerManager {
 public:
  void AddToTimer(long timestamp, std::shared_ptr<EpollContext> ptr) {
    LOG(INFO) << "AddToTimer timestamp: " << timestamp;
    pq.push(TimeNode(ptr, timestamp));
  }
  const TimeNode& GetNearbyTimeNode() {
    const TimeNode& timeNode = pq.top();
    return timeNode;
  }
  void PopTopTimeNode() { pq.pop(); }
  int size() const { return pq.size(); }

 private:
  std::priority_queue<TimeNode, vector<TimeNode>, greater<TimeNode>> pq;
};
