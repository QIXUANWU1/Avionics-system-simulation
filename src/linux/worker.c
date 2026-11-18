#define _POSIX_C_SOURCE 200809L
#include "sample_route.h"
#include "advisory.h"
#include "generated_layout.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#define RX_LIMIT 512
_Static_assert(WIRE_VERSION == IMA_WIRE_VERSION, "wire version");
_Static_assert(SIM_SERVICES == IMA_CATALOG_COUNT && SIM_SERVICES <= 64, "compiled catalog size");
_Static_assert(ADVISORY_SID==IMA_ADVISORY_ID && ADVISORY_PERIOD_MS==IMA_ADVISORY_PERIOD &&
               ADVISORY_RADAR==IMA_ADVISORY_SOURCE_0 && ADVISORY_NAV==IMA_ADVISORY_SOURCE_1,"advisory binding");
enum Reject { BAD_WIRE, BAD_ROUTE, BAD_SAMPLE, REJECT_COUNT };
static int fd, observe;
static unsigned domain, profile;
static uint32_t epoch, session;
static uint64_t received, tx_drops, observation_drops, consumed, rejected[REJECT_COUNT], published, service_mask;
static struct sockaddr_in destinations[32], collector;
static volatile sig_atomic_t running = 1;

static void stop(int sig) { (void)sig; running = 0; }
static uint64_t now_ns(void)
{
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t)) abort();
    return (uint64_t)t.tv_sec * UINT64_C(1000000000) + (uint64_t)t.tv_nsec;
}
static void until(uint64_t when)
{
    const struct timespec t = {(time_t)(when / 1000000000u), (long)(when % 1000000000u)};
    int result;
    do { result = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t, NULL); }
    while (running && result == EINTR);
    if (result && result != EINTR) abort();
}
static struct sockaddr_in endpoint(unsigned port)
{
    return (struct sockaddr_in){.sin_family=AF_INET, .sin_port=htons((uint16_t)port), .sin_addr.s_addr=htonl(INADDR_LOOPBACK)};
}
static int transmit(const uint8_t b[WIRE_SIZE], const struct sockaddr_in *dst)
{
    return sendto(fd, b, WIRE_SIZE, MSG_DONTWAIT, (const struct sockaddr *)dst, sizeof *dst) == WIRE_SIZE;
}
static void mirror(const Message *m)
{
    if (!observe) return;
    uint8_t b[WIRE_SIZE];
    wire_encode(b, m);
    if (!transmit(b, &collector)) observation_drops++;
}
static void publish(const Message *m)
{
    uint8_t b[WIRE_SIZE];
    wire_encode(b, m);
    const uint32_t mask = ima_fanout[profile][m->sid];
    published++;
    service_mask |= UINT64_C(1) << m->sid;
    for (unsigned i=0; i<ima_domain_count[profile]; ++i) {
        if ((mask & (UINT32_C(1) << i)) && !transmit(b, &destinations[i])) tx_drops++;
    }
    if (observe && !transmit(b, &collector)) observation_drops++;
}
static void on_read(const ReadEvent *e, void *ctx)
{
    (void)ctx;
    consumed++;
    const Message m={.kind=KIND_CONSUME,.sid=(uint16_t)e->producer,.epoch=epoch,.sequence=e->sequence,
        .model_ms=e->consumer_ms,.flags=((uint32_t)e->consumer<<8)|(e->status==0),
        .source_ns=now_ns(),.v={(double)e->source_ms,(double)e->status,0}};
    mirror(&m);
}
static void drain(Frame *state, ImaSampleRoute *route, uint32_t ms)
{
    for (int n=0; n<RX_LIMIT; ++n) {
        uint8_t b[WIRE_SIZE];
        struct sockaddr_in from;
        socklen_t fl = sizeof from;
        const ssize_t len = recvfrom(fd,b,sizeof b,MSG_DONTWAIT|MSG_TRUNC,(struct sockaddr *)&from,&fl);
        if (len < 0) {
            if (errno!=EAGAIN && errno!=EWOULDBLOCK && errno!=EINTR) rejected[BAD_WIRE]++;
            break;
        }
        Message m;
        const unsigned port=fl==sizeof from&&from.sin_family==AF_INET&&from.sin_addr.s_addr==htonl(INADDR_LOOPBACK)
                            ?ntohs(from.sin_port):0;
        const unsigned first=ntohs(destinations[0].sin_port);
        const unsigned source=port>=first?port-first:UINT32_MAX;
        const enum ImaRx result=ima_route_accept(route,source,b,(int)len,ms,state,&m);
        if (result!=IMA_RX_OK) {
            rejected[result==IMA_RX_BAD_WIRE?BAD_WIRE:result==IMA_RX_BAD_ROUTE?BAD_ROUTE:BAD_SAMPLE]++;
            continue;
        }
        received++;
        if (observe) {
            m.kind=KIND_RECEIVE; m.flags=domain; m.epoch=epoch; m.source_ns=now_ns();
            mirror(&m);
        }
    }
}
static int validate_layout(void)
{
    if (sim_validate_catalog()) return 1;
    for (unsigned p=0; p<IMA_PROFILE_COUNT; ++p) {
        uint32_t fanout[SIM_SERVICES]={0};
        for (unsigned i=0; i<SIM_SERVICES; ++i) {
            const Service *s=&sim_services[i];
            if (ima_owner[p][i]>=ima_domain_count[p] || (uint32_t)s->period_ms!=ima_period_ms[i] ||
                (uint32_t)s->max_age_ms!=ima_max_age_ms[i]) return 1;
            uint64_t inputs=0;
            for (int k=0; k<s->ninputs; ++k) {
                const unsigned producer=(unsigned)s->inputs[k];
                inputs |= UINT64_C(1)<<producer;
                if (ima_owner[p][producer]!=ima_owner[p][i]) fanout[producer] |= UINT32_C(1)<<ima_owner[p][i];
            }
            if (inputs!=ima_inputs[i]) return 1;
        }
        if (ima_advisory_domain[p]>=0) {
            const unsigned dst=(unsigned)ima_advisory_domain[p];
            if (dst>=ima_domain_count[p]||dst==ima_owner[p][ADVISORY_RADAR]||dst==ima_owner[p][ADVISORY_NAV]) return 1;
            fanout[ADVISORY_RADAR] |= UINT32_C(1)<<dst;
            fanout[ADVISORY_NAV] |= UINT32_C(1)<<dst;
        }
        if (memcmp(fanout, ima_fanout[p], sizeof fanout)) return 1;
    }
    return 0;
}
static uint64_t argument(const char *str, uint64_t minimum, uint64_t maximum)
{
    char *end;
    if (strspn(str,"0123456789") != strlen(str) || !*str) exit(2);
    errno=0;
    const unsigned long long v=strtoull(str,&end,10);
    if (errno || *end || v<minimum || v>maximum) exit(2);
    return (uint64_t)v;
}
int main(int argc, char **argv)
{
    if (argc==2 && !strcmp(argv[1],"--check-layout")) return validate_layout();
    if (argc!=8) {
        fprintf(stderr,"usage: worker PROFILE_INDEX DOMAIN BASE_PORT SESSION_EPOCH START_NS DURATION_MS OBSERVE_0_OR_1\n");
        return 2;
    }
    profile=(unsigned)argument(argv[1],0,IMA_PROFILE_COUNT-1);
    domain=(unsigned)argument(argv[2],0,ima_domain_count[profile]-1);
    const unsigned base=(unsigned)argument(argv[3],1024,65000);
    session=(uint32_t)argument(argv[4],1,UINT32_MAX);
    const uint64_t start=argument(argv[5],1,UINT64_MAX-3601000000000ULL);
    const uint32_t duration=(uint32_t)argument(argv[6],1000,3600000);
    observe=(int)argument(argv[7],0,1);
    epoch=IMA_DOMAIN_EPOCH(session,domain);
    for (unsigned i=0;i<ima_domain_count[profile];++i) destinations[i]=endpoint(IMA_DOMAIN_PORT(base,i));
    collector=endpoint(IMA_OBSERVE_PORT(base));

    signal(SIGTERM,stop); signal(SIGINT,stop);
    fd=socket(AF_INET,SOCK_DGRAM|SOCK_NONBLOCK|SOCK_CLOEXEC,0);
    if (fd<0) { perror("socket"); return 3; }
    if (bind(fd,(const struct sockaddr *)&destinations[domain],sizeof destinations[domain])) {
        perror("bind"); close(fd); return 3;
    }

    const int has_advisory=ima_advisory_domain[profile]==(int)domain;
    AdvisoryWorkspace *work=has_advisory?calloc(1,sizeof *work):NULL;
    AdvisoryModel *model=has_advisory&&work?ima_advisory_create():NULL;
    if (has_advisory&&!model) { perror("advisory allocation");free(work);close(fd);return 3; }
    uint32_t advisory_calls=0, advisory_available=0, advisory_score_ppm=0;
    Frame state;
    sim_init(&state);
    for (int i=0;i<SIM_SERVICES;i++) state.samples[i].valid=0;
    ImaSampleRoute route={.profile=profile,.domain=domain,.session=session};
    uint32_t loop=0;
    until(start);
    while (running) {
        const uint64_t before=now_ns();
        const uint64_t since_ms=(before-start)/1000000u;
        if (since_ms>=duration) break;
        const uint32_t ms=(uint32_t)since_ms / SIM_STEP_MS * SIM_STEP_MS;
        drain(&state,&route,ms);
        ++loop; state.tick_ms=ms;
        const Frame snapshot=state;
        for (unsigned i=0;i<SIM_SERVICES;++i) {
            if (ima_owner[profile][i]!=domain || ms%(uint32_t)sim_services[i].period_ms) continue;
            Frame out;
            sim_service(&snapshot,(int)i,&out,observe?on_read:NULL,NULL);
            state.samples[i]=out.samples[i];
            const Sample *s=&state.samples[i];
            Message m={.kind=KIND_SAMPLE,.sid=(uint16_t)i,.epoch=epoch,.sequence=s->sequence,.model_ms=ms,
                .flags=(uint32_t)s->valid,.source_ns=now_ns()};
            memcpy(m.v,s->v,sizeof m.v);
            publish(&m);
        }
        if (has_advisory && ms%ADVISORY_PERIOD_MS==0) {
            const AdvisoryResult a=ima_advisory_step(model,&snapshot,work);
            advisory_calls++;
            if (a.valid) {
                advisory_available++;
                advisory_score_ppm=(uint32_t)(a.score*1000000.0f);
            }
            const Message m={.kind=KIND_ADVISORY,.sid=ADVISORY_SID,.epoch=epoch,.sequence=advisory_calls,
                .model_ms=ms,.flags=domain*2u+a.valid,.source_ns=now_ns(),.v={(double)a.score,a.radar_sequence,a.nav_sequence}};
            mirror(&m);
        }
        if (observe && loop%10==0) {
            const Message m={.kind=KIND_PROGRESS,.sid=65535,.epoch=epoch,.sequence=loop,.model_ms=ms,
                .flags=domain,.source_ns=now_ns(),
                .v={(double)(rejected[BAD_WIRE]+rejected[BAD_ROUTE]+rejected[BAD_SAMPLE]),(double)tx_drops,(double)(now_ns()-before)/1000.0}};
            mirror(&m);
        }
        until(start+(uint64_t)(ms+SIM_STEP_MS)*1000000u);
    }
    fprintf(stderr,"domain=%u loops=%u published=%" PRIu64 " service_mask=%" PRIu64
            " rx=%" PRIu64 " consume_observed=%" PRIu64 " reject_wire=%" PRIu64 " reject_route=%" PRIu64
            " reject_sample=%" PRIu64 " tx_drop=%" PRIu64 " observe_drop=%" PRIu64
            " advisory_calls=%u advisory_available=%u advisory_last_available_score_ppm=%u\n",
            domain,loop,published,service_mask,received,consumed,rejected[BAD_WIRE],rejected[BAD_ROUTE],
            rejected[BAD_SAMPLE],tx_drops,observation_drops,advisory_calls,advisory_available,advisory_score_ppm);
    if (model) ima_advisory_destroy(model);
    free(work);
    close(fd);
    return 0;
}
