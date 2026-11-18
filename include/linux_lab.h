#ifndef IMA_LINUX_LAB_H
#define IMA_LINUX_LAB_H
#include "sampling_port.h"
#include "generated_layout.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

static inline uint64_t lab_number(const char *s,uint64_t lo,uint64_t hi)
{
    if (!*s || strspn(s,"0123456789")!=strlen(s)) exit(2);
    char *end;
    errno=0;
    unsigned long long value=strtoull(s,&end,10);
    if (errno||*end||value<lo||value>hi) exit(2);
    return (uint64_t)value;
}
static inline uint64_t lab_now(void)
{
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC,&t)) abort();
    return (uint64_t)t.tv_sec*UINT64_C(1000000000)+(uint64_t)t.tv_nsec;
}
static inline struct sockaddr_in lab_addr(unsigned port)
{
    return (struct sockaddr_in){.sin_family=AF_INET,.sin_port=htons((uint16_t)port),.sin_addr.s_addr=htonl(INADDR_LOOPBACK)};
}
static inline int lab_bind(unsigned port,int flags)
{
    const struct sockaddr_in addr=lab_addr(port);
    const int fd=socket(AF_INET,SOCK_DGRAM|SOCK_CLOEXEC|flags,0);
    if (fd<0) return -1;
    if (bind(fd,(const struct sockaddr *)&addr,sizeof addr)) {
        const int error=errno;
        close(fd);errno=error;return -1;
    }
    return fd;
}
static inline int lab_send(int fd,unsigned port,const uint8_t packet[WIRE_SIZE])
{
    const struct sockaddr_in addr=lab_addr(port);
    return sendto(fd,packet,WIRE_SIZE,MSG_DONTWAIT,(const struct sockaddr *)&addr,sizeof addr)==WIRE_SIZE;
}
#endif
