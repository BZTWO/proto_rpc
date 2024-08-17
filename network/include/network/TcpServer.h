#pragma once

#include <atomic>
#include <map>

#include "network/TcpConnection.h"

namespace network {

class Acceptor;
class EventLoop;
class EventLoopThreadPool;

///
/// TCP server, supports single-threaded and thread-pool models.
///
/// This is an interface class, so don't expose too much details.
class TcpServer {
 public:
  typedef std::function<void(EventLoop *)> ThreadInitCallback;
  enum Option {
    kNoReusePort,
    kReusePort,
  };

  // TcpServer(EventLoop* loop, const InetAddress& listenAddr);
  TcpServer(EventLoop *loop, const InetAddress &listenAddr,
            const std::string &nameArg, Option option = kNoReusePort);
  ~TcpServer();  // force out-line dtor, for std::unique_ptr members.

  const std::string &ipPort() const { return ipPort_; }
  const std::string &name() const { return name_; }
  EventLoop *getLoop() const { return loop_; }

  /// Set the number of threads for handling input.
  ///
  /// Always accepts new connection in loop's thread.
  /// Must be called before @c start
  /// @param numThreads
  /// - 0 means all I/O in loop's thread, no thread will created.
  ///   this is the default value.
  /// - 1 means all I/O in another thread.
  /// - N means a thread pool with N threads, new connections
  ///   are assigned on a round-robin basis.
  void setThreadNum(int numThreads);
  void setThreadInitCallback(const ThreadInitCallback &cb) {
    threadInitCallback_ = cb;
  }
  /// valid after calling start()
  std::shared_ptr<EventLoopThreadPool> threadPool() { return threadPool_; }

  /// Starts the server if it's not listening.
  ///
  /// It's harmless to call it multiple times.
  /// Thread safe.
  void start();

  /// Set connection callback.
  /// Not thread safe.
  void setConnectionCallback(const ConnectionCallback &cb) {
    connectionCallback_ = cb;
  }

  /// Set message callback.
  /// Not thread safe.
  void setMessageCallback(const MessageCallback &cb) { messageCallback_ = cb; }

  /// Set write complete callback.
  /// Not thread safe.
  void setWriteCompleteCallback(const WriteCompleteCallback &cb) {
    writeCompleteCallback_ = cb;
  }

 private:
  /// Not thread safe, but in loop
  /// 在接收到新的连接时被调用，用于处理新连接
  void newConnection(int sockfd, const InetAddress &peerAddr);

  /// Thread safe.
  /// 安全地从连接映射中删除一个连接
  void removeConnection(const TcpConnectionPtr &conn);

  /// Not thread safe, but in loop
  /// 在事件循环的线程中执行，用于从连接映射中删除一个连接
  void removeConnectionInLoop(const TcpConnectionPtr &conn);

  typedef std::map<std::string, TcpConnectionPtr> ConnectionMap; // 一个映射

  EventLoop *loop_;  // the acceptor loop
  const std::string ipPort_;  // 服务器监听的 IP 和端口
  const std::string name_;  // 服务器的名称
  std::unique_ptr<Acceptor> acceptor_;  // avoid revealing Acceptor  接受新的连接
  std::shared_ptr<EventLoopThreadPool> threadPool_;  // 管理事件循环线程池，用于处理连接的 I/O 事件

  ConnectionCallback connectionCallback_;  // 连接回调函数，用于在新连接建立时调用
  MessageCallback messageCallback_;  // 消息回调函数，用于在接收到消息时调用
  WriteCompleteCallback writeCompleteCallback_;  // 写完成回调函数，用于在数据写入完成时调用
  ThreadInitCallback threadInitCallback_; // 线程初始化回调函数，用于在线程池中的线程初始化时调用

  std::atomic<bool> started_;
  // always in loop thread
  int nextConnId_;
  ConnectionMap connections_;  // 下一个连接的 ID
};

}  // namespace network
