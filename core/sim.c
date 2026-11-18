#include "sim.h"
#include <math.h>
#include <string.h>

#define S(key,name,units,g,p,n,...) {#key,name,units,g,p,n,{__VA_ARGS__}},
const Service sim_services[SIM_SERVICES]={
#include "services.def"
};
#undef S

void sim_init(Frame *frame) { *frame=(Frame){0}; }

int sim_sample_fresh(const Frame *frame,unsigned producer)
{
    const Sample *s=&frame->samples[producer];
    return s->valid==1 && frame->tick_ms>=s->time_ms &&
           frame->tick_ms-s->time_ms<=3u*(unsigned)sim_services[producer].period_ms+SIM_STEP_MS &&
           isfinite(s->v[0]) && isfinite(s->v[1]) && isfinite(s->v[2]);
}

void sim_service(const Frame *in,int id,Frame *out,ReadObserver observer,void *ctx)
{
    *out=*in;
    if (id<0 || id>=SIM_SERVICES) return;
    const Service *service=&sim_services[id];
    Sample *o=&out->samples[id];
    const Sample *inputs[SIM_INPUTS]={0};
    unsigned live=0;
    *o=(Sample){.time_ms=in->tick_ms,.sequence=in->samples[id].sequence+1u};
    for (int k=0;k<service->ninputs;k++) {
        const int producer=service->inputs[k];
        const Sample *s=&in->samples[producer];
        const int ready=sim_sample_fresh(in,(unsigned)producer);
        if (ready) { inputs[k]=s; live++; }
        if (observer) {
            const ReadEvent event={producer,id,!ready,s->time_ms,in->tick_ms,s->sequence};
            observer(&event,ctx);
        }
    }
    const unsigned phase=(in->tick_ms/1000u)%20u;
    if (id==8) { o->v[0]=live; o->v[1]=service->ninputs; o->valid=1; return; }
    if (id==9) {
        o->v[0]=inputs[0]!=NULL; o->v[1]=inputs[1]?inputs[1]->v[0]:0;
        o->v[2]=inputs[1]!=NULL; o->valid=1; return;
    }
    if (live!=(unsigned)service->ninputs) return;
    const Sample *a=inputs[0],*b=inputs[1];
    switch (id) {
    case 0: o->v[0]=2000+80*phase; break;
    case 1: o->v[0]=2006+80*phase; break;
    case 2: o->v[0]=45+phase; o->v[1]=30+phase; o->v[2]=1; break;
    case 3: o->v[0]=a->v[0]; o->v[1]=fabs(a->v[0]-b->v[0]); o->v[2]=1; break;
    case 4: memcpy(o->v,a->v,sizeof o->v); break;
    case 5:
        if (a->v[2]!=1.0 || a->v[0]<1 || a->v[0]>400 || a->v[1]< -30 || a->v[1]>85) return;
        memcpy(o->v,a->v,sizeof o->v); break;
    case 6:
        o->v[0]=b->v[0]; o->v[1]=b->v[1]; o->v[2]=a->v[0]; break;
    case 7:
        o->v[0]=fabs((double)a->time_ms-b->time_ms); o->v[1]=b->v[2]==1.0; break;
    default: return;
    }
    o->valid=1;
}

int sim_validate_catalog(void)
{
    for (int id=0;id<SIM_SERVICES;id++) {
        const Service *s=&sim_services[id];
        if (s->group<0 || s->group>=SIM_GROUPS || s->period_ms<=0 || s->period_ms%SIM_STEP_MS ||
            s->ninputs<0 || s->ninputs>SIM_INPUTS) return -1;
        for (int j=0;j<id;j++) if (!strcmp(s->key,sim_services[j].key)) return -1;
        for (int k=0;k<s->ninputs;k++) {
            if (s->inputs[k]<0 || s->inputs[k]>=SIM_SERVICES || s->inputs[k]==id) return -1;
            for (int j=0;j<k;j++) if (s->inputs[j]==s->inputs[k]) return -1;
        }
    }
    return 0;
}
