#pragma once

#include <map>
#include <mutex>

#include <google/protobuf/service.h>

#include "RpcCodec.h"

#include "rpc.pb.h"

namespace google {
namespace protobuf {

// Defined in other files.
class Descriptor;         // descriptor.h
class ServiceDescriptor;  // descriptor.h
class MethodDescriptor;   // descriptor.h
class Message;            // message.h

class Closure;

class RpcController;
class Service;

}  // namespace protobuf
}  // namespace google

namespace network {

// Abstract interface for an RPC channel.  An RpcChannel represents a
// communication line to a Service which can be used to call that Service's
// methods.  The Service may be running on another machine.  Normally, you
// should not call an RpcChannel directly, but instead construct a stub Service
// wrapping it.  Example:
// FIXME: update here
//   RpcChannel* channel = new MyRpcChannel("remotehost.example.com:1234");
//   MyService* service = new MyService::Stub(channel);
//   service->MyMethod(request, &response, callback);
class RpcChannel : public ::google::protobuf::RpcChannel {
 public:
  RpcChannel();

  explicit RpcChannel(const TcpConnectionPtr &conn);

  ~RpcChannel() override;

  void setConnection(const TcpConnectionPtr &conn) { conn_ = conn; }

  void setServices(
      const std::map<std::string, ::google::protobuf::Service *> *services) {
    services_ = services;
  }

  // Call the given method of the remote service.  The signature of this
  // procedure looks the same as Service::CallMethod(), but the requirements
  // are less strict in one important way:  the request and response objects
  // need not be of any specific class as long as their descriptors are
  // method->input_type() and method->output_type().
  // 负责在客户端调用远程服务的方法
  void CallMethod(const ::google::protobuf::MethodDescriptor *method,
                  ::google::protobuf::RpcController *controller,
                  const ::google::protobuf::Message *request,
                  ::google::protobuf::Message *response,
                  ::google::protobuf::Closure *done) override;

  void onMessage(const TcpConnectionPtr &conn, Buffer *buf);

 private:
  // 进一步解析 RPC 消息，并调用相应的服务方法或处理响应
  void onRpcMessage(const TcpConnectionPtr &conn,
                    const RpcMessagePtr &messagePtr);

  // 处理 RPC 请求完成后的回调
  // 当服务器处理完一个 RPC 请求后，会调用这个函数将结果打包并发送回客户端
  void doneCallback(::google::protobuf::Message *response, int64_t id);

  // 处理从远程服务器接收到的响应消息
  void handle_response_msg(const RpcMessagePtr &messagePtr);
  // 负责处理客户端发来的 RPC 请求消息，并将其转发给注册的服务（Service）进行处理，最后将响应发送回客户端
  void handle_request_msg(const TcpConnectionPtr &conn,
                          const RpcMessagePtr &messagePtr);
  struct OutstandingCall {
    ::google::protobuf::Message *response;   // 存储服务器回复的位置
    ::google::protobuf::Closure *done;       // 收到响应后将执行的回调函数
  };

  ProtoRpcCodec codec_;
  TcpConnectionPtr conn_;
  std::atomic<int64_t> id_;      // 唯一的 RPC 请求 ID
 
  std::mutex mutex_;
  std::map<int64_t, OutstandingCall> outstandings_;      // (尚未完成)存储正在等待响应的 RPC 调用信息，包括响应消息和回调

  const std::map<std::string, ::google::protobuf::Service *> *services_;     // 保存服务名称到服务对象的映射，用于查找并处理 RPC 请求
};
typedef std::shared_ptr<RpcChannel> RpcChannelPtr;

}  // namespace network