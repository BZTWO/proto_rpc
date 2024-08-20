#include <unistd.h>

#include <cassert>

#include <glog/logging.h>

#include "network/EventLoop.h"
#include "rpc_framework/RpcServer.h"

#include "monitor.pb.h"

using namespace network;

namespace monitor {

class TestServiceImpl : public TestService {
 public:
  // 一个 RPC 服务
  virtual void MonitorInfo(::google::protobuf::RpcController *controller,   // 控制 RPC 的执行
                           const ::monitor::TestRequest *request,           // 接收从客户端发送的请求数据
                           ::monitor::TestResponse *response,               // 设置要发送回客户端的响应数据
                           ::google::protobuf::Closure *done) {             // 异步通知 RPC 框架操作已经完成
    LOG(INFO) << " req:\n" << request->DebugString();
    response->set_status(true);
    std::string c = "hight_" + std::to_string(request->count());
    response->set_cpu_info(c);
    done->Run();   // 服务器已经准备好发送响应给客户端，整个 RPC 请求的生命周期即将结束
  }
};

}  // namespace monitor

int main(int argc, char *argv[]) {
  // google::InitGoogleLogging(argv[0]);
  LOG(INFO) << "pid = " << getpid();
  EventLoop loop;
  InetAddress listenAddr(9981);
  monitor::TestServiceImpl impl;
  RpcServer server(&loop, listenAddr);
  server.registerService(&impl);
  server.start();
  loop.loop();
  google::protobuf::ShutdownProtobufLibrary();   // 清理库的资源
}
