#define _POSIX_C_SOURCE 200809L
#include "advisory.h"
#include "cockpit.h"
#include "sampling_port.h"
#include "linux_lab.h"
#include "dds_link.h"
#include "display_feed.h"
#include "trace_read.h"
#include <inttypes.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

static int save_svg(const char *path,const CockpitState *state)
{
    const size_t len=strlen(path);
    if (!len||len>3500) return 0;
    char tmp[len+12];
    (void)snprintf(tmp,sizeof tmp,"%s.imaXXXXXX",path);
    const int fd=mkstemp(tmp);
    if (fd<0) return 0;
    FILE *f=fdopen(fd,"w");
    if (!f) {close(fd);unlink(tmp);return 0;}
    const int ok=cockpit_write_svg(f,state);
    const int closed=fclose(f)==0;
    if (ok&&closed&&rename(tmp,path)==0) return 1;
    unlink(tmp);
    return 0;
}

static int render_demo(const char *arg,const char *path)
{
    char *end;
    errno=0;
    const unsigned long ms=strtoul(arg,&end,10);
    if (errno||!*arg||*end||ms>120000||ms<SIM_STEP_MS) return 0;
    CockpitState state={.demo=1};
    Frame next;
    for (uint32_t t=0;t<=ms;t+=SIM_STEP_MS) {
        state.bus.tick_ms=t;
        next=state.bus;
        for (unsigned id=0;id<SIM_SERVICES;id++) {
            if (t%(unsigned)sim_services[id].period_ms) continue;
            Frame result;
            sim_service(&state.bus,(int)id,&result,NULL,NULL);
            next.samples[id]=result.samples[id];
        }
        state.bus=next;
    }
    AdvisoryWorkspace *work=malloc(sizeof *work);
    AdvisoryModel *model=ima_advisory_create();
    if (!work||!model) {free(work);if (model) ima_advisory_destroy(model);return 0;}
    const AdvisoryResult a=ima_advisory_step(model,&state.bus,work);
    state.weather=(Sample){.v={a.score,0,0},.time_ms=state.bus.tick_ms,.valid=(int)a.valid};
    free(work); ima_advisory_destroy(model);
    return save_svg(path,&state);
}

static int render_trace(FILE *f,const char *output,int stream)
{
    if (!ima_trace_header(f)) return 0;
    CockpitState state={0};
    ImaSequence seen[SIM_SERVICES+1]={0};
    uint32_t last=0;
    int rc;
    Message m;
    while ((rc=ima_trace_next(f,&m))>0) {
        const int weather=m.kind==KIND_ADVISORY&&m.sid==ADVISORY_SID;
        if (!weather&&(m.kind!=KIND_SAMPLE||!ima_display_service(m.sid)||m.flags>1)) continue;
        if (seen[m.sid].seen&&(!ima_newer(m.sequence,seen[m.sid].sequence)||m.model_ms<seen[m.sid].model_ms)) continue;
        seen[m.sid]=(ImaSequence){.sequence=m.sequence,.model_ms=m.model_ms,.seen=1};
        Sample *value=weather?&state.weather:&state.bus.samples[m.sid];
        *value=(Sample){.time_ms=m.model_ms,.sequence=m.sequence,.valid=(int)(m.flags&1u)};
        memcpy(value->v,m.v,sizeof value->v);
        if (state.bus.tick_ms<m.model_ms) state.bus.tick_ms=m.model_ms;
        if (stream&&state.bus.tick_ms-last>=500) {
            if (!save_svg(output,&state)) return 0;
            last=state.bus.tick_ms;
        }
    }
    return rc==0&&save_svg(output,&state);
}

static int render_live(unsigned profile,unsigned base,uint32_t session,uint64_t start,
                       uint32_t duration,const char *path)
{
    CockpitState state={.live=1};
    ImaSequence seen[SIM_SERVICES+1]={0};
    ImaDdsLink *link=ima_dds_open(base,1,0);
    if (!link) return 0;
    uint64_t last=0;
    const struct timespec pause={.tv_nsec=20000000};
    int ok=1;
    for (;;) {
        const uint64_t now=lab_now();
        const uint64_t elapsed=now>=start?(now-start)/UINT64_C(1000000):0;
        /* The final frame lets any remaining samples age out after workers stop. */
        if (elapsed>duration+1800u) break;
        state.bus.tick_ms=(uint32_t)elapsed;
        for (unsigned i=0;i<256;i++) {
            Message m;
            unsigned source;
            const int rc=ima_dds_read(link,&source,&m);
            if (rc<0) {ok=0;break;}
            if (!rc) break;
            const int weather=m.sid==ADVISORY_SID;
            if (!weather&&!ima_display_service(m.sid)) continue;
            const int owner=weather?ima_advisory_domain[profile]:ima_owner[profile][m.sid];
            if (owner<0||source!=(unsigned)owner||!isfinite(m.v[0])||!isfinite(m.v[1])||!isfinite(m.v[2])||
                !ima_accept_sample(&m,IMA_DOMAIN_EPOCH(session,source),state.bus.tick_ms,&seen[m.sid])) continue;
            Sample *value=weather?&state.weather:&state.bus.samples[m.sid];
            *value=(Sample){.time_ms=m.model_ms,.sequence=m.sequence,.valid=(int)m.flags};
            memcpy(value->v,m.v,sizeof value->v);
        }
        if (!ok) break;
        if (!last||now-last>=UINT64_C(100000000)) {
            if (!save_svg(path,&state)) {ok=0;break;}
            last=now;
        }
        nanosleep(&pause,NULL);
    }
    ima_dds_close(link);
    return ok;
}

int main(int argc,char **argv)
{
    if (argc==2&&!strcmp(argv[1],"--clock-ns")) {
        printf("%" PRIu64 "\n",lab_now()); return 0;
    }
    if (argc==3&&!strcmp(argv[1],"--domains")) {
        printf("%u\n",(unsigned)ima_domain_count[lab_number(argv[2],0,IMA_PROFILE_COUNT-1)]); return 0;
    }
    if (argc==8&&!strcmp(argv[1],"--live")) {
        const unsigned profile=(unsigned)lab_number(argv[2],0,IMA_PROFILE_COUNT-1);
        const unsigned base=(unsigned)lab_number(argv[3],1024,65000);
        const uint32_t session=(uint32_t)lab_number(argv[4],1,UINT32_MAX);
        const uint64_t start=lab_number(argv[5],1,UINT64_MAX);
        const uint32_t duration=(uint32_t)lab_number(argv[6],1000,3600000);
        const uint64_t now=lab_now();
        if ((start>now&&start-now>UINT64_C(30000000000)) ||
            (now>start&&now-start>=(uint64_t)duration*UINT64_C(1000000))) return 2;
        return !render_live(profile,base,session,start,duration,argv[7]);
    }
    if (argc==4&&!strcmp(argv[1],"--demo")) return !render_demo(argv[2],argv[3]);
    if (argc==3&&!strcmp(argv[1],"--stream")) return !render_trace(stdin,argv[2],1);
    if (argc==4&&!strcmp(argv[1],"--trace")) {
        FILE *f=fopen(argv[2],"rb");
        if (!f) {perror(argv[2]);return 1;}
        const int ok=render_trace(f,argv[3],0);
        fclose(f);
        return !ok;
    }
    fputs("Use --live PROFILE BASE SESSION START_NS DURATION_MS OUT.svg, --demo MODEL_MS OUT.svg, or --trace TRACE.bin OUT.svg\n",stderr);
    return 2;
}
