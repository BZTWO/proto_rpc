#include "network/Socket.h"

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>  // snprintf

#include <cassert>

#include <glog/logging.h>

#include "network/InetAddress.h"
#include "network/SocketsOps.h"

using namespace network;
namespace network {
Socket::~Socket() { sockets::close(sockfd_); }

/// @brief 获取套接字的 TCP 连接信息。
/// 
/// 该函数通过 `getsockopt` 系统调用来获取与当前 TCP 连接相关的状态信息，
/// 并将这些信息填充到提供的 `tcp_info` 结构体中。
/// 
/// @param tcpi 指向一个 `tcp_info` 结构体的指针，函数将 TCP 连接的信息写入到这个结构体中。
/// 
/// @return 如果 `getsockopt` 调用成功，返回 `true`；否则返回 `false`。
///        成功时，`tcpi` 中包含了 TCP 连接的状态信息；失败时，`tcpi` 可能会保持为全零。
bool Socket::getTcpInfo(struct tcp_info *tcpi) const {
  socklen_t len = sizeof(*tcpi);
  memset(tcpi, 0, len);
  return ::getsockopt(sockfd_, SOL_TCP, TCP_INFO, tcpi, &len) == 0;
}

/// @brief 获取套接字的 TCP 连接信息并格式化为字符串。
/// 
/// 该函数通过 `getTcpInfo` 函数获取当前 TCP 连接的状态信息，并将这些信息格式化为一个字符串，写入到提供的缓冲区中。
/// 如果获取 TCP 信息失败，缓冲区内容未定义。
/// 
/// @param buf 指向字符数组的指针，用于存储格式化后的 TCP 信息字符串。
/// @param len 缓冲区的大小，确保不会发生缓冲区溢出。
/// @return 如果成功获取并格式化 TCP 信息，返回 `true`；否则返回 `false`。成功时，`buf` 中包含格式化后的 TCP 信息。
bool Socket::getTcpInfoString(char *buf, int len) const {
  struct tcp_info tcpi;
  bool ok = getTcpInfo(&tcpi);  // 获取 TCP 信息
  if (ok) {
    snprintf(
        buf, len,
        "unrecovered=%u "
        "rto=%u ato=%u snd_mss=%u rcv_mss=%u "
        "lost=%u retrans=%u rtt=%u rttvar=%u "
        "sshthresh=%u cwnd=%u total_retrans=%u",
        tcpi.tcpi_retransmits,  // 未恢复的 RTO 超时次数
        tcpi.tcpi_rto,          // 重传超时时间（微秒）
        tcpi.tcpi_ato,          // 预测的软时钟滴答时间（微秒）
        tcpi.tcpi_snd_mss, tcpi.tcpi_rcv_mss,  // 发送和接收 MSS
        tcpi.tcpi_lost,     // 丢失的包数
        tcpi.tcpi_retrans,  // 重传的包数
        tcpi.tcpi_rtt,      // 平滑的往返时间（微秒）
        tcpi.tcpi_rttvar,   // RTT 的中位偏差
        tcpi.tcpi_snd_ssthresh, tcpi.tcpi_snd_cwnd,  // 发送的慢启动阈值和拥塞窗口
        tcpi.tcpi_total_retrans);  // 总重传次数
  }
  return ok;
}

/// @brief 将套接字绑定到指定的地址和端口。
/// 
/// 该函数使用 `bind` 系统调用将套接字 `sockfd_` 绑定到 `addr` 所指定的地址和端口。如果绑定失败，函数将记录一个致命错误并终止程序。
/// 
/// @param addr 要绑定到套接字的目标地址和端口。
void Socket::bindAddress(const InetAddress &addr) {
  int ret = ::bind(sockfd_, addr.getSockAddr(),
                   static_cast<socklen_t>(sizeof(struct sockaddr_in6)));
  if (ret < 0) {
    LOG(FATAL) << "sockets::bindOrDie";
  }
}

/// @brief 将套接字设为监听状态，以接受传入的连接请求。
/// 
/// 该函数使用 `listen` 系统调用将套接字 `sockfd_` 设置为监听状态，允许它接受传入的连接请求。`SOMAXCONN` 是系统定义的常量，用于指定监听队列的最大长度。如果监听失败，函数将记录一个致命错误并终止程序。
void Socket::listen() {
  int ret = ::listen(sockfd_, SOMAXCONN);
  if (ret < 0) {
    LOG(FATAL) << "sockets::listenOrDie";
  }
}


int Socket::accept(InetAddress *peeraddr) {
  struct sockaddr_in6 addr;
  memset(&addr, 0, sizeof(addr));

  socklen_t addrlen = static_cast<socklen_t>(sizeof(addr));
  int client_fd =
      ::accept(sockfd_, reinterpret_cast<sockaddr *>(&addr), &addrlen);
  if (client_fd < 0) {
    LOG(ERROR) << "Socket::accept error";
  }
  if (client_fd >= 0) {
    peeraddr->setSockAddrInet6(addr);
  }
  return client_fd;
}

// 完成了数据的发送，并希望通知对方不再发送更多数据时可以调用 shutdownWrite
// 对方接收到这一信号后，可以知道数据传输已经结束，并关闭连接
void Socket::shutdownWrite() {
  // sockets::shutdownWrite(sockfd_);
  if (::shutdown(sockfd_, SHUT_WR) < 0) {
    LOG(ERROR) << "sockets::shutdownWrite";
  }
}

/// @brief 设置套接字的 TCP_NODELAY 选项。
/// 
/// 该方法用于控制套接字是否启用 Nagle 算法。当 `on` 为 true 时，禁用 Nagle 算法，
/// 这可以通过立即发送小数据包来减少延迟，而不等待将它们合并。当 `on` 为 false 时，
/// 启用 Nagle 算法，这可能通过将小数据包合并后再发送来提高网络效率。这个选项在
/// 需要低延迟通信的场景中非常有用。
///
/// @param on 如果为 true，则禁用 Nagle 算法（启用 TCP_NODELAY）。如果为 false，则
///           启用 Nagle 算法（禁用 TCP_NODELAY）。
void Socket::setTcpNoDelay(bool on) {
  int optval = on ? 1 : 0;
  ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval,
               static_cast<socklen_t>(sizeof optval));
  // FIXME CHECK
}

/// 控制是否允许重用本地地址
void Socket::setReuseAddr(bool on) {
  int optval = on ? 1 : 0;
  ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval,
               static_cast<socklen_t>(sizeof optval));
  // FIXME CHECK
}

/// /// 控制是否允许重用本地端口
void Socket::setReusePort(bool on) {
#ifdef SO_REUSEPORT
  int optval = on ? 1 : 0;
  int ret = ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval,
                         static_cast<socklen_t>(sizeof optval));
  if (ret < 0 && on) {
    LOG(ERROR) << "SO_REUSEPORT failed.";
  }
#else
  if (on) {
    LOG_ERROR << "SO_REUSEPORT is not supported.";
  }
#endif
}

/// 启用/禁用连接保持活动
void Socket::setKeepAlive(bool on) {
  int optval = on ? 1 : 0;
  ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval,
               static_cast<socklen_t>(sizeof optval));
  // FIXME CHECK
}
}  // namespace network