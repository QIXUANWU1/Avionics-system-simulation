#define _POSIX_C_SOURCE 200809L
#include "linux_lab.h"
#include "health.h"
#include "trace_read.h"
#include <inttypes.h>

static int report(FILE *f,unsigned profile,uint32_t session)
{
    static const char *names[]={"starting","fresh","missing","stale","invalid"};
    static const ServiceId ids[]={SID_imu_sample,SID_imu_backup,SID_attitude_select,SID_primary_display,SID_bus_status};
    enum { COUNT=sizeof ids/sizeof *ids };
    ImaHealthSample state[COUNT]={0};
    uint32_t now=0;
    unsigned check_count=0,changes=0;
    if (!ima_trace_header(f)) return 0;
    Message m;
    int rc;
    while ((rc=ima_trace_next(f,&m))>0) {
        if (m.kind==KIND_SAMPLE) {
            for (unsigned i=0;i<COUNT;i++)
                if (m.sid==ids[i]) (void)ima_health_update(&state[i],&m,IMA_DOMAIN_EPOCH(session,ima_owner[profile][ids[i]]));
        } else if (m.kind==KIND_PROGRESS && m.sid==UINT16_MAX && m.flags<ima_domain_count[profile] &&
                   m.epoch==IMA_DOMAIN_EPOCH(session,m.flags) && m.model_ms>now) {
            now=m.model_ms;
            check_count++;
            for (unsigned i=0;i<COUNT;i++) {
                const ServiceId id=ids[i];
                const ImaHealthStatus next=ima_health_status(&state[i],now,ima_max_age_ms[id]);
                if (next==HEALTH_START||next==state[i].status) continue;
                printf("t=%" PRIu32 "ms %s %s -> %s\n",now,sim_services[id].key,
                       names[state[i].status],names[next]);
                state[i].status=next;
                changes++;
            }
        }
    }
    if (rc<0) return 0;
    printf("checks=%u changes=%u coverage=%s\n",check_count,changes,check_count?"progress-seen":"no-clock");
    return !fflush(stdout);
}

int main(int argc,char **argv)
{
    if (argc!=4) {
        fputs("Use ima-health PROFILE SESSION TRACE.bin (or - for stdin).\n",stderr);
        return 2;
    }
    const unsigned profile=(unsigned)lab_number(argv[1],0,IMA_PROFILE_COUNT-1);
    const uint32_t session=(uint32_t)lab_number(argv[2],1,UINT32_MAX);
    FILE *f=strcmp(argv[3],"-")?fopen(argv[3],"rb"):stdin;
    if (!f) {perror(argv[3]);return 1;}
    const int ok=report(f,profile,session);
    if (f!=stdin) fclose(f);
    if (!ok) fputs("Trace is unreadable, incomplete, or has an incompatible format.\n",stderr);
    return !ok;
}
