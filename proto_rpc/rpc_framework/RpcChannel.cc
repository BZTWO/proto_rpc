#include "RpcChannel.h"

#include <cassert>

#include <glog/logging.h>
#include <google/protobuf/descriptor.h>

#include "rpc.pb.h"
using namespace network;
RpcChannel::RpcChannel()
    : codec_(std::bind(&RpcChannel::onRpcMessage, this, std::placeholders::_1,
                       std::placeholders::_2)),
      services_(NULL) {
  LOG(INFO) << "RpcChannel::ctor - " << this;
}

RpcChannel::RpcChannel(const TcpConnectionPtr &conn)
    : codec_(std::bind(&RpcChannel::onRpcMessage, this, std::placeholders::_1,
                       std::placeholders::_2)),
      conn_(conn),
      services_(NULL) {
  LOG(INFO) << "RpcChannel::ctor - " << this;
}

RpcChannel::~RpcChannel() {
  LOG(INFO) << "RpcChannel::dtor - " << this;
  for (const auto &outstanding : outstandings_) {
    OutstandingCall out = outstanding.second;
    delete out.response;
    delete out.done;
  }
}

void RpcChannel::CallMethod(const ::google::protobuf::MethodDescriptor  *method,      // 要调用的具体 RPC 方法
                                  ::google::protobuf::RpcController     *controller,  // 管理 RPC 调用的状态和信息
                            const ::google::protobuf::Message           *request,     // RPC 方法的请求参数
                                  ::google::protobuf::Message           *response,    // 存储来自服务器端的响应
                                  ::google::protobuf::Closure           *done) {      // 服务器处理完请求后，回调函数将被调用
  RpcMessage message;
  message.set_type(REQUEST);
  int64_t id = id_.fetch_add(1) + 1;                      // 生成一个全局唯一的请求 ID
  message.set_id(id);
  message.set_service(method->service()->full_name());    // 获取该方法所属服务的完整名称
  message.set_method(method->name());                     // 获取被调用方法的名称
  message.set_request(request->SerializeAsString());      // 请求对象序列化为字符串

  OutstandingCall out = {response, done};
  {
    std::unique_lock<std::mutex> lock(mutex_);
    outstandings_[id] = out;
  }
  codec_.send(conn_, message);
}

void RpcChannel::onMessage(const TcpConnectionPtr &conn, Buffer *buf) {
  codec_.onMessage(conn, buf);
}

void RpcChannel::onRpcMessage(const TcpConnectionPtr &conn,
                              const RpcMessagePtr &messagePtr) {
  assert(conn == conn_);
  RpcMessage &message = *messagePtr;
  if (message.type() == RESPONSE) {
    handle_response_msg(messagePtr);
  } else if (message.type() == REQUEST) {
    handle_request_msg(conn, messagePtr);
  }
}

void RpcChannel::handle_response_msg(const RpcMessagePtr &messagePtr) {
  RpcMessage &message = *messagePtr;
  int64_t id = message.id();

  OutstandingCall out = {NULL, NULL};

  {
    std::unique_lock<std::mutex> lock(mutex_);
    auto it = outstandings_.find(id);
    if (it != outstandings_.end()) {
      out = it->second;
      outstandings_.erase(it);
    }
  }

  if (out.response) {
    std::unique_ptr<google::protobuf::Message> d(out.response);
    if (!message.response().empty()) {
      out.response->ParseFromString(message.response());    // 将消息体解析到 response 对象中
    }
    if (out.done) {
      out.done->Run();    // RPC 调用已经完成，并执行用户提供的回调函数
    }
  }
}

void RpcChannel::handle_request_msg(const TcpConnectionPtr &conn,
                                    const RpcMessagePtr &messagePtr) {
  RpcMessage &message = *messagePtr;
  ErrorCode error = WRONG_PROTO;
  if (services_) {
    std::map<std::string, google::protobuf::Service *>::const_iterator it =
        services_->find(message.service());
    if (it != services_->end()) {
      google::protobuf::Service *service = it->second;
      assert(service != NULL);
      const google::protobuf::ServiceDescriptor *desc = service->GetDescriptor();
      const google::protobuf::MethodDescriptor *method = desc->FindMethodByName(message.method());
      
      if (method) {
        std::unique_ptr<google::protobuf::Message> request(
            service->GetRequestPrototype(method).New());   // 获取当前 RPC 方法的请求消息原型
        if (request->ParseFromString(message.request())) {
          google::protobuf::Message *response =
              service->GetResponsePrototype(method).New(); // 用于存储服务器生成的响应数据
          // response is deleted in doneCallback
          int64_t id = message.id();
          service->CallMethod(
              method, NULL, request.get(), response,
              NewCallback(this, &RpcChannel::doneCallback, response, id));  // 处理请求
          error = NO_ERROR;
        } else {
          error = INVALID_REQUEST;
        }
      } else {
        error = NO_METHOD;
      }
    } else {
      error = NO_SERVICE;
    }
  } else {
    error = NO_SERVICE;
  }
  if (error != NO_ERROR) {
    RpcMessage response;
    response.set_type(RESPONSE);
    response.set_id(message.id());
    response.set_error(error);
    codec_.send(conn_, response);  // // 通过 codec_ 发送回客户端
  }
}

void RpcChannel::doneCallback(::google::protobuf::Message *response,
                              int64_t id) {
  std::unique_ptr<google::protobuf::Message> d(response);
  RpcMessage message;
  message.set_type(RESPONSE);
  message.set_id(id);
  message.set_response(response->SerializeAsString());  // FIXME: error check
  codec_.send(conn_, message);
}
