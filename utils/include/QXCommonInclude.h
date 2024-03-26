#ifndef _QX_INCLUDE_H_
#define _QX_INCLUDE_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <stddef.h>
#include <unistd.h>
#include <stdarg.h>
#include <malloc.h>
#include <stdint.h>
#include <signal.h>
#include <endian.h>
#include <assert.h>
#include <ctype.h>

#include <pthread.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/file.h>

#include "event2/event.h"
#include "event2/event_struct.h"
#include "event2/util.h"
#include "event2/thread.h"

#include "QXErrno.h"

#ifdef __cplusplus
extern "C"{
#endif

#define BOOL                                        unsigned char
#define TRUE                                        1
#define FALSE                                       0
#define __in
#define __in_ecout(size)
#define __out
#define __out_ecout(size)
#define __inout
#define __inout_ecout(size)
#define UNUSED(_p_)                                 ((void)(_p_))
#define MUST_CHECK                                  __attribute__((warn_unused_result))
#define LIKELY(x)                                   __builtin_expect(!!(x), 1)
#define UNLIKELY(x)                                 __builtin_expect(!!(x), 0)

#define QX_BUFF_16                                  16
#define QX_BUFF_32                                  32
#define QX_BUFF_64                                  64
#define QX_BUFF_128                                 128
#define QX_BUFF_256                                 256
#define QX_BUFF_512                                 512
#define QX_BUFF_1024                                1024
#define QX_BUFF_2048                                2048
#define QX_BUFF_4096                                4096
#define QX_BUFF_8192                                8192

#define QX_TCP_SERVER_PORT                          15000
#define QX_MAX_CLIENT_NUM_PER_SERVER                128
#define QX_KILL_SIGNAL                              SIGUSR1

#define QX_UATOMIC_INC(addr)                        __sync_fetch_and_add((addr), 1)
#define QX_UATOMIC_DEC(addr)                        __sync_fetch_and_add((addr), -1)

#ifdef __cplusplus
}
#endif

#endif /* _QX_INCLUDE_H_ */
