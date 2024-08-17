#pragma once

#include <functional>
#include <memory>
#include <vector>

namespace network {

class EventLoop;
class EventLoopThread;

class EventLoopThreadPool {
 public:
  typedef std::function<void(EventLoop *)> ThreadInitCallback;

  EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg);
  ~EventLoopThreadPool();
  void setThreadNum(int numThreads) { numThreads_ = numThreads; }
  void start(const ThreadInitCallback &cb = ThreadInitCallback());

  // valid after calling start()
  /// round-robin
  EventLoop *getNextLoop();

  /// with the same hash code, it will always return the same EventLoop
  EventLoop *getLoopForHash(size_t hashCode);

  std::vector<EventLoop *> getAllLoops();

  bool started() const { return started_; }

  const std::string &name() const { return name_; }

 private:
  EventLoop *baseLoop_;   // 主线程中的 EventLoop 对象
  std::string name_;
  bool started_ = false;
  int numThreads_;     // 线程池中的线程数量,每个线程将运行一个 EventLoop 对象
  int next_;    // 记录当前正在选择的下一个 EventLoop，实现轮询调度策略。
  std::vector<std::unique_ptr<EventLoopThread>> threads_;
  std::vector<EventLoop *> loops_;  // 线程池中所有 EventLoop 对象的指针数组
};

}  // namespace network
