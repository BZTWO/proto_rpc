#pragma once

#include <map>
#include <vector>

#include "network/EventLoop.h"
struct epoll_event;
namespace network {
class Channel;
///
/// IO Multiplexing with epoll(4).
///
/// I/O 事件的分发器
class Poller {
 public:
  typedef std::vector<Channel *> ChannelList;
  Poller(EventLoop *loop);
  ~Poller();

  void poll(int timeoutMs, ChannelList *activeChannels);
  void updateChannel(Channel *channel);
  void removeChannel(Channel *channel);

  void assertInLoopThread() const { ownerLoop_->assertInLoopThread(); }
  bool hasChannel(Channel *channel) const;

 private:
  EventLoop *ownerLoop_;  // 确保 Poller 在正确的线程上操作
  static const int kInitEventListSize = 16;

  static const char *operationToString(int op);

  void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;
  void update(int operation, Channel *channel);

  typedef std::vector<struct epoll_event> EventList;

  int epollfd_;
  EventList events_;   // 存储 epoll 触发的事件，用于在 epoll_wait 调用后处理发生的事件

  typedef std::map<int, Channel *> ChannelMap;
  ChannelMap channels_;   // 存储所有注册到 Poller 的 Channel 对象，通过文件描述符来快速查找对应的 Channel
};

}  // namespace network
