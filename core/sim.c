#include "model_steps.h"
#include <math.h>
#include <string.h>

#define S(key,name,units,g,p,a,n,...) {#key,name,units,g,p,a,n,{__VA_ARGS__}},
const Service sim_services[SIM_SERVICES]={
#include "services.def"
};
#undef S

void sim_init(Frame *frame) { *frame=(Frame){0}; }

int sim_sample_fresh(const Frame *frame,unsigned producer)
{
    if (producer>=SIM_SERVICES) return 0;
    const Sample *s=&frame->samples[producer];
    return s->valid==1 && frame->tick_ms>=s->time_ms &&
           frame->tick_ms-s->time_ms<=(unsigned)sim_services[producer].max_age_ms &&
           isfinite(s->v[0]) && isfinite(s->v[1]) && isfinite(s->v[2]);
}

void sim_service(const Frame *in,int id,Frame *out,ReadObserver observer,void *ctx)
{
    *out=*in;
    if (id<0 || id>=SIM_SERVICES) return;
    const Service *s=&sim_services[id];
    Sample *o=&out->samples[id];
    *o=(Sample){.time_ms=in->tick_ms,.sequence=in->samples[id].sequence+1u};
    ModelStep step={.id=(unsigned)id,.tick_ms=in->tick_ms,.required=(unsigned)s->ninputs,
                    .previous=sim_sample_fresh(in,(unsigned)id)?&in->samples[id]:NULL};
    /* Position retains its last coordinates through a gap while publishing invalid data. */
    if (id==SID_position_fixture && in->samples[id].sequence)
        step.previous=&in->samples[id];
    for (int k=0;k<s->ninputs;k++) {
        const int producer=s->inputs[k];
        const Sample *sample=&in->samples[producer];
        const int ready=sim_sample_fresh(in,(unsigned)producer);
        if (ready) { step.input[k]=sample; step.live++; }
        if (observer) {
            const ReadEvent event={producer,id,!ready,sample->time_ms,in->tick_ms,sample->sequence};
            observer(&event,ctx);
        }
    }
    if (s->group==9) o->valid=navigation_step(&step,o);
    else if (s->group==8) o->valid=extended_systems_step(&step,o);
    else if (id>=SID_airdata_words && id<=SID_display_airdata) o->valid=airdata_step(&step,o);
    else if (s->group==0 || s->group==5 || s->group==6) o->valid=flight_step(&step,o);
    else if (s->group==7) o->valid=systems_step(&step,o);
    else o->valid=information_step(&step,o);
}

int sim_validate_catalog(void)
{
    for (int id=0;id<SIM_SERVICES;id++) {
        const Service *s=&sim_services[id];
        if (s->group<0 || s->group>=SIM_GROUPS || s->period_ms<=0 || s->period_ms%SIM_STEP_MS ||
            s->max_age_ms<s->period_ms || s->ninputs<0 || s->ninputs>SIM_INPUTS) return -1;
        for (int j=0;j<id;j++) if (!strcmp(s->key,sim_services[j].key)) return -1;
        for (int k=0;k<s->ninputs;k++) {
            if (s->inputs[k]<0 || s->inputs[k]>=SIM_SERVICES || s->inputs[k]==id) return -1;
            for (int j=0;j<k;j++) if (s->inputs[j]==s->inputs[k]) return -1;
        }
    }
    return 0;
}
