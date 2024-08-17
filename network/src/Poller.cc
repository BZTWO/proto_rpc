#include "network/Poller.h"

#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <cassert>

#include <glog/logging.h>

#include "network/Channel.h"

using namespace network;

// On Linux, the constants of poll(2) and epoll(4)
// are expected to be the same.
static_assert(EPOLLIN == POLLIN, "epoll uses same flag values as poll");
static_assert(EPOLLPRI == POLLPRI, "epoll uses same flag values as poll");
static_assert(EPOLLOUT == POLLOUT, "epoll uses same flag values as poll");
static_assert(EPOLLRDHUP == POLLRDHUP, "epoll uses same flag values as poll");
static_assert(EPOLLERR == POLLERR, "epoll uses same flag values as poll");
static_assert(EPOLLHUP == POLLHUP, "epoll uses same flag values as poll");

namespace {
const int kNew = -1;
const int kAdded = 1;
const int kDeleted = 2;
}  // namespace

namespace network {
// EPOLL_CLOEXEC 表示在执行 exec() 系统调用时关闭这个 epoll 文件描述符。
//这有助于避免文件描述符泄漏到子进程中。
Poller::Poller(EventLoop *loop)
    : ownerLoop_(loop),
      epollfd_(::epoll_create1(EPOLL_CLOEXEC)),
      events_(kInitEventListSize) {
  if (epollfd_ < 0) {
    LOG(FATAL) << "EPollPoller::EPollPoller";
  }
}

Poller::~Poller() { ::close(epollfd_); }
// 添加到活跃列表
void Poller::poll(int timeoutMs, ChannelList *activeChannels) {
  LOG(INFO) << "fd total count " << channels_.size();
  int numEvents = ::epoll_wait(epollfd_, &*events_.begin(),
                               static_cast<int>(events_.size()), timeoutMs);
  int savedErrno = errno;

  if (numEvents > 0) {
    LOG(INFO) << numEvents << " events happened";
    fillActiveChannels(numEvents, activeChannels);
    // 如果所有的 epoll_event 都被用上了，则数组会扩容
    if (size_t(numEvents) == events_.size()) {
      events_.resize(events_.size() * 2);
    }
  } else if (numEvents == 0) {
    LOG(INFO) << "nothing happened";
  } else {
    // error happens, log uncommon ones
    if (savedErrno != EINTR) {
      errno = savedErrno;
      LOG(ERROR) << "EPollPoller::poll()";
    }
  }
}
//轮询 添加到活跃列表
void Poller::fillActiveChannels(int numEvents, ChannelList *activeChannels) const {
  assert(size_t(numEvents) <= events_.size());
  for (int i = 0; i < numEvents; ++i) {
    Channel *channel = static_cast<Channel *>(events_[i].data.ptr);  // 获取与该事件关联的 Channel 对象
#ifndef NDEBUG
    int fd = channel->fd();
    ChannelMap::const_iterator it = channels_.find(fd);
    assert(it != channels_.end());
    assert(it->second == channel);
#endif
    channel->set_revents(events_[i].events);  // 将 epoll_event 的事件类型存储到 Channel 对象中
    activeChannels->push_back(channel);
  }
}
//更新红黑树
void Poller::updateChannel(Channel *channel) {
  Poller::assertInLoopThread();
  const int index = channel->index();
  LOG(INFO) << "fd = " << channel->fd() << " events = " << channel->events()
            << " index = " << index;
  if (index == kNew || index == kDeleted) {
    // a new one, add with EPOLL_CTL_ADD
    int fd = channel->fd();
    if (index == kNew) {
      assert(channels_.find(fd) == channels_.end());
      channels_[fd] = channel;
    } else  // index == kDeleted
    {
      assert(channels_.find(fd) != channels_.end());
      assert(channels_[fd] == channel);
    }

    channel->set_index(kAdded);
    update(EPOLL_CTL_ADD, channel);
  } else {
    // update existing one with EPOLL_CTL_MOD/DEL
    int fd = channel->fd();
    (void)fd;
    assert(channels_.find(fd) != channels_.end());
    assert(channels_[fd] == channel);
    assert(index == kAdded);
    if (channel->isNoneEvent()) {
      update(EPOLL_CTL_DEL, channel);
      channel->set_index(kDeleted);
    } else {
      update(EPOLL_CTL_MOD, channel);
    }
  }
}
//从映射中删除 channel
void Poller::removeChannel(Channel *channel) {
  Poller::assertInLoopThread();
  int fd = channel->fd();
  LOG(INFO) << "fd = " << fd;
  assert(channels_.find(fd) != channels_.end());
  assert(channels_[fd] == channel);
  assert(channel->isNoneEvent());
  int index = channel->index();
  assert(index == kAdded || index == kDeleted);
  size_t n = channels_.erase(fd);
  (void)n;
  assert(n == 1);

  if (index == kAdded) {
    update(EPOLL_CTL_DEL, channel);
  }
  channel->set_index(kNew);
}
//根据 operation 在红黑树上操作，更新红黑树
void Poller::update(int operation, Channel *channel) {
  struct epoll_event event;
  memset(&event, 0, sizeof(event));
  event.events = channel->events();
  event.data.ptr = channel;
  int fd = channel->fd();
  if (::epoll_ctl(epollfd_, operation, fd, &event) < 0) {
    if (operation == EPOLL_CTL_DEL) {
      LOG(ERROR) << "epoll_ctl op =" << operationToString(operation)
                 << " fd =" << fd;
    } else {
      LOG(FATAL) << "epoll_ctl op =" << operationToString(operation)
                 << " fd =" << fd;
    }
  }
}

const char *Poller::operationToString(int op) {
  switch (op) {
    case EPOLL_CTL_ADD:
      return "ADD";
    case EPOLL_CTL_DEL:
      return "DEL";
    case EPOLL_CTL_MOD:
      return "MOD";
    default:
      assert(false && "ERROR op");
      return "Unknown Operation";
  }
}
// 检查在映射中是否存在 fd
bool Poller::hasChannel(Channel *channel) const {
  assertInLoopThread();
  ChannelMap::const_iterator it = channels_.find(channel->fd());
  return it != channels_.end() && it->second == channel;
}

}  // namespace network