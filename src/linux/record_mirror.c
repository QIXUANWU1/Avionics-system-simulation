#define _POSIX_C_SOURCE 200809L
#include "linux_lab.h"
#include <signal.h>
#include <poll.h>

/* The observer has no send path. Outputs raw IMATRCE1 for offline decoding. */
static volatile sig_atomic_t running=1;
static void stop(int n){(void)n;running=0;}
static int mirror_source(unsigned p,unsigned base,uint32_t session,const Message *m,const struct sockaddr_in *from)
{
    unsigned i;
    switch(m->kind) {
    case KIND_SAMPLE:
        if (m->sid>=SIM_SERVICES||m->flags>1)return 0;
        i=ima_owner[p][m->sid];break;
    case KIND_PROGRESS:
        if (m->sid!=UINT16_MAX)return 0;
        i=m->flags;break;
    case KIND_CONSUME: {
        const unsigned consumer=m->flags>>8;
        if (m->sid>=SIM_SERVICES||consumer>=SIM_SERVICES||(m->flags&255u)>1||
            !(ima_inputs[consumer]&(UINT64_C(1)<<m->sid)))return 0;
        i=ima_owner[p][consumer];break;
    }
    case KIND_RECEIVE:
        i=m->flags;
        if (i>=ima_domain_count[p]||m->sid>=SIM_SERVICES||!(ima_fanout[p][m->sid]&(UINT32_C(1)<<i)))return 0;
        break;
    case KIND_ADVISORY:
        i=m->flags/2u;
        if (m->sid!=IMA_ADVISORY_ID||i>=ima_domain_count[p]||ima_advisory_domain[p]!=(int)i)return 0;
        break;
    default:return 0;
    }
    return i<ima_domain_count[p]&&m->epoch==IMA_DOMAIN_EPOCH(session,i)&&from->sin_family==AF_INET&&
           from->sin_addr.s_addr==htonl(INADDR_LOOPBACK)&&from->sin_port==htons((uint16_t)IMA_DOMAIN_PORT(base,i));
}
static int offline_check(void)
{
    const uint32_t session=200;
    const unsigned p=1,d=(unsigned)ima_advisory_domain[p];
    const Message m={.kind=KIND_ADVISORY,.sid=IMA_ADVISORY_ID,.flags=2*d+1,.epoch=IMA_DOMAIN_EPOCH(session,d)};
    const struct sockaddr_in ok=lab_addr(IMA_DOMAIN_PORT(31000,d)),wrong=lab_addr(IMA_DOMAIN_PORT(31000,0));
    return !mirror_source(p,31000,session,&m,&ok)||mirror_source(p,31000,session,&m,&wrong)
           ||mirror_source(0,31000,session,&m,&ok)||mirror_source(p,31000,session+1,&m,&ok);
}
int main(int argc,char **argv)
{
    if (argc==2&&!strcmp(argv[1],"--check-sources"))return offline_check();
    if (argc!=4) {
        fputs("usage: ima-record-mirror PROFILE_INDEX BASE_PORT SESSION_EPOCH > TRACE.bin\n",stderr);return 2;
    }
    const unsigned profile=(unsigned)lab_number(argv[1],0,IMA_PROFILE_COUNT-1);
    const unsigned base=(unsigned)lab_number(argv[2],1024,65000);
    const uint32_t session=(uint32_t)lab_number(argv[3],1,UINT32_MAX);
    struct sigaction sa={.sa_handler=stop};
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGTERM,&sa,NULL)||sigaction(SIGINT,&sa,NULL))return 3;
    const int fd=lab_bind(IMA_OBSERVE_PORT(base),SOCK_NONBLOCK);
    if (fd<0) { perror("mirror bind");return 3; }
    int result=0;
    if (fwrite("IMATRCE1",1,8,stdout)!=8) result=4;
    while (running&&!result) {
        struct pollfd interest={.fd=fd,.events=POLLIN};
        const int ready=poll(&interest,1,200);
        if (ready<0 && errno!=EINTR) { result=4;break; }
        if (ready<=0||!running)continue;
        uint8_t b[9+WIRE_SIZE];struct sockaddr_in from;socklen_t len=sizeof from;
        const ssize_t n=recvfrom(fd,b+9,WIRE_SIZE,MSG_TRUNC,(struct sockaddr *)&from,&len);
        if (n<0) {
            if (errno==EINTR||errno==EAGAIN||errno==EWOULDBLOCK)continue;
            perror("mirror receive");result=4;break;
        }
        Message m;
        if (len!=sizeof from||n!=WIRE_SIZE||wire_decode(b+9,WIRE_SIZE,&m)||!mirror_source(profile,base,session,&m,&from))continue;
        b[0]=m.kind;le64_put(b+1,lab_now());
        if (fwrite(b,1,sizeof b,stdout)!=sizeof b)result=4;
    }
    if (fflush(stdout))result=4;
    close(fd);return result;
}
