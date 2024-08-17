#include "network/util.h"

#include <arpa/inet.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
namespace network {

static int g_pid = 0;

static thread_local int t_thread_id = 0;

/// @brief 获取当前进程的进程 ID
/// @return 返回当前进程的进程 ID
pid_t getPid() {
  if (g_pid != 0) {
    return g_pid;
  }
  return getpid();
}

/// @brief 使用 thread_local 关键字，确保每个线程有独立的 t_thread_id
/// @return 返回当前线程的线程 ID
pid_t getThreadId() {
  if (t_thread_id != 0) {
    return t_thread_id;
  }
  return static_cast<pid_t>(::syscall(SYS_gettid));
}

/// @brief 使用 gettimeofday() 获取当前时间，时间结果存储在 timeval 结构中
/// @return 返回当前时间的毫秒数
int64_t getNowMs() {
  timeval val;
  gettimeofday(&val, NULL);

  return val.tv_sec * 1000 + val.tv_usec / 1000;  /// gettimeofday() 获取的时间精度到微秒，但是这里返回的是毫秒
}

/// @brief 将网络字节序的字节数组转换为主机字节序的 32 位整数
/// @param buf 指向存储在网络字节序中的 4 字节数据的缓冲区指针
/// @return 返回主机字节序的 32 位整数
int32_t getInt32FromNetByte(const char *buf) {
  int32_t re;
  memcpy(&re, buf, sizeof(re));   /// 使用 memcpy 将字节流复制到一个 int32_t 变量中，然后使用 ntohl() 将网络字节序（大端序）转换为主机字节序
  return ntohl(re);
}

}  // namespace network