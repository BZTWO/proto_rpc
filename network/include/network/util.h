#pragma once

#include <sys/types.h>
#include <unistd.h>

namespace network {
// 返回当前进程的进程 ID
pid_t getPid();       

// 返回当前线程的线程 ID
pid_t getThreadId();   

// 返回当前时间的毫秒数
int64_t getNowMs();     
                         
// 从网络字节序的字节数组中提取一个 32 位整数
int32_t getInt32FromNetByte(const char *buf);    

}  /// namespace network
