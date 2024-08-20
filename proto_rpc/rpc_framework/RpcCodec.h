#pragma once

#include <iostream>
#include <memory>
#include <string>
#include <type_traits>

#include "rpc.pb.h"

namespace network {

class Buffer;
class TcpConnection;
typedef std::shared_ptr<TcpConnection> TcpConnectionPtr;

class RpcMessage;
typedef std::shared_ptr<RpcMessage> RpcMessagePtr;
extern const char rpctag[];  // = "RPC0";

// wire format
//
// Field     Length  Content
//
// size      4-byte  N+8
// "RPC0"    4-byte
// payload   N-byte
// checksum  4-byte  adler32 of "RPC0"+payload
//

class ProtoRpcCodec {
 public:
  const static int kHeaderLen = sizeof(int32_t);     // 消息头的长度
  const static int kChecksumLen = sizeof(int32_t);   // 校验和的长度
  const static int kMaxMessageLen =
      64 * 1024 * 1024;  // same as codec_stream.h kDefaultTotalBytesLimit  消息的最大长度
  enum ErrorCode {
    kNoError = 0,
    kInvalidLength,
    kCheckSumError,
    kInvalidNameLen,
    kUnknownMessageType,
    kParseError,
  };
  typedef std::shared_ptr<TcpConnection> TcpConnectionPtr;
  typedef std::function<void(const TcpConnectionPtr &, const RpcMessagePtr &)>
      ProtobufMessageCallback;

  typedef std::shared_ptr<google::protobuf::Message> MessagePtr;

  explicit ProtoRpcCodec(const ProtobufMessageCallback &messageCb)
      : messageCallback_(messageCb) {}
  ~ProtoRpcCodec() {}

  // 消息进行序列化，并通过 TcpConnection 发送出去
  void send(const TcpConnectionPtr &conn,
            const ::google::protobuf::Message &message);

  // 读取数据并解析 接收
  void onMessage(const TcpConnectionPtr &conn, Buffer *buf);

  // 从缓冲区中解析数据并反序列化为 Protobuf 消息
  bool parseFromBuffer(const void *buf, int len,
                       google::protobuf::Message *message);

  // 将 Protobuf 消息序列化并存储到缓冲区中，准备发送
  int serializeToBuffer(const google::protobuf::Message &message, Buffer *buf);

  ErrorCode parse(const char *buf, int len,
                  ::google::protobuf::Message *message);

  // 消息编码并填充到 Buffer 中，准备发送
  void fillEmptyBuffer(Buffer *buf, const google::protobuf::Message &message);

  static int32_t checksum(const void *buf, int len);         // 生成校验
  static bool validateChecksum(const char *buf, int len);    // 验证消息的校验和
  static int32_t asInt32(const char *buf);                   // 计算 将字节数组转换为 32 位整数，用于解码消息的长度、校验和等字段

 private:
  ProtobufMessageCallback messageCallback_;
  int kMinMessageLen = 4;
  const std::string tag_ = "RPC0";
};

}  // namespace network
