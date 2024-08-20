#pragma once

#include "network/TcpServer.h"

namespace google {
namespace protobuf {

class Service;

}  // namespace protobuf
}  // namespace google

namespace network {

class RpcServer {
 public:
  RpcServer(EventLoop *loop, const InetAddress &listenAddr);
  // 设置线程池的线程数量
  void setThreadNum(int numThreads) { server_.setThreadNum(numThreads); }
  // 注册一个 Protocol Buffers 的 Service 对象
  // 把这个服务添加到 services_ 映射中
  void registerService(::google::protobuf::Service *);
  void start();

 private:
  void onConnection(const TcpConnectionPtr &conn);

  // void onMessage(const TcpConnectionPtr& conn,
  //                Buffer* buf,
  //                Timestamp time);

  TcpServer server_;
  std::map<std::string, ::google::protobuf::Service *> services_;
};

}  // namespace network
