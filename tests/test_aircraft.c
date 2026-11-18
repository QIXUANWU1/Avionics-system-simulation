/* Offline end-to-end route test. Each domain has its own cache; model callbacks
   and wire helpers are the production C versions. No sockets or processes. */
#include "sample_route.h"
#include "generated_layout.h"
#include "model_steps.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

#define MAX_DOMAINS 32
typedef struct {
    Frame frame[MAX_DOMAINS];
    ImaSampleRoute route[MAX_DOMAINS];
    unsigned received;
} TestLab;

static void tick(TestLab *lab,unsigned profile,uint32_t now)
{
    const uint32_t session=UINT32_C(0x87654321);
    const unsigned domains=ima_domain_count[profile];
    Sample published[SIM_SERVICES]={0};
    unsigned due[SIM_SERVICES]={0};
    for (unsigned d=0;d<domains;d++) lab->frame[d].tick_ms=now;
    for (unsigned id=0;id<SIM_SERVICES;id++) {
        if (now%(unsigned)sim_services[id].period_ms) continue;
        Frame next;
        sim_service(&lab->frame[ima_owner[profile][id]],(int)id,&next,NULL,NULL);
        published[id]=next.samples[id];
        due[id]=1;
    }
    for (unsigned id=0;id<SIM_SERVICES;id++) {
        if (!due[id]) continue;
        const unsigned owner=ima_owner[profile][id];
        const Sample *s=&published[id];
        lab->frame[owner].samples[id]=*s;
        Message m={.kind=KIND_SAMPLE,.sid=(uint16_t)id,.epoch=IMA_DOMAIN_EPOCH(session,owner),
                   .sequence=s->sequence,.model_ms=s->time_ms,.flags=(uint32_t)s->valid};
        memcpy(m.v,s->v,sizeof m.v);
        uint8_t wire[WIRE_SIZE];
        wire_encode(wire,&m);
        for (unsigned d=0;d<domains;d++) {
            if (!(ima_fanout[profile][id] & (UINT32_C(1)<<d))) continue;
            assert(d!=owner);
            Message msg;
            assert(ima_route_accept(&lab->route[d],owner,wire,WIRE_SIZE,now,&lab->frame[d],&msg)==IMA_RX_OK);
            lab->received++;
        }
    }
}

static Sample sample(const TestLab *lab,unsigned p,ServiceId id)
{
    const Frame *f=&lab->frame[ima_owner[p][id]];
    assert(sim_sample_fresh(f,id));
    return f->samples[id];
}

static void run_profile(unsigned p)
{
    TestLab lab={0};
    for (unsigned d=0;d<ima_domain_count[p];d++)
        lab.route[d]=(ImaSampleRoute){.profile=p,.domain=d,.session=UINT32_C(0x87654321)};
    double bank_after_input=0, fuel_before=0, heading_before=0;
    for (uint32_t t=0;t<=10500;t+=SIM_STEP_MS) {
        tick(&lab,p,t);
        if (t==800) {
            assert(fabs(sample(&lab,p,SID_primary_display).v[0])<.01);
            assert(sample(&lab,p,SID_flight_mode).v[0]==MODE_MANUAL);
            assert(sample(&lab,p,SID_crew_indication).v[0]==0);
            fuel_before=sample(&lab,p,SID_fuel_quantity).v[0];
            heading_before=sample(&lab,p,SID_navigation_display).v[1];
        } else if (t==3500) {
            assert(sample(&lab,p,SID_control_effort).v[0]>.1);
            assert(sample(&lab,p,SID_actuator_response).v[0]>.1);
            bank_after_input=sample(&lab,p,SID_primary_display).v[0];
            assert(bank_after_input>7.0);
            assert(sample(&lab,p,SID_primary_display).v[1]>2);
            assert(sample(&lab,p,SID_primary_display).v[2]>90);
            assert(sample(&lab,p,SID_navigation_display).v[1]>heading_before);
            assert(sample(&lab,p,SID_display_airdata).v[0]>6000);
            assert(sample(&lab,p,SID_atmosphere_sample).v[0]<1013.25);
            assert(sample(&lab,p,SID_gear_indication).v[0]==0);
            assert(sample(&lab,p,SID_weather_display).v[2]>1900);
        } else if (t==6900) {
            assert(sample(&lab,p,SID_flight_mode).v[0]==MODE_CENTER);
            assert(fabs(sample(&lab,p,SID_primary_display).v[0])<bank_after_input*.1);
            assert(sample(&lab,p,SID_engine_display).v[2]<fuel_before);
            assert(sample(&lab,p,SID_radio_indication).v[0]==1);
            assert(sample(&lab,p,SID_cabin_indication).v[1]==1);
        }
    }
    assert(sample(&lab,p,SID_primary_display).v[0]<-7.0);
    assert(lab.received>1000);

    const unsigned control=ima_owner[p][SID_flight_mode];
    Frame lost=lab.frame[control], out;
    lost.samples[SID_attitude_estimate].time_ms=0;
    sim_service(&lost,SID_flight_mode,&out,NULL,NULL);
    const Sample mode=out.samples[SID_flight_mode];
    assert(mode.valid && mode.v[0]==MODE_UNAVAILABLE && (unsigned)mode.v[1]==2);

    lost=lab.frame[ima_owner[p][SID_actuator_response]];
    lost.samples[SID_flight_mode]=mode;
    const double prior=fabs(lost.samples[SID_actuator_response].v[0]);
    sim_service(&lost,SID_actuator_response,&out,NULL,NULL);
    assert(prior>.1 && fabs(out.samples[SID_actuator_response].v[0])<prior);
    lost=lab.frame[ima_owner[p][SID_crew_indication]];
    lost.samples[SID_flight_mode]=mode;
    sim_service(&lost,SID_crew_indication,&out,NULL,NULL);
    assert(out.samples[SID_crew_indication].valid && ((unsigned)out.samples[SID_crew_indication].v[0]&1u));

    printf("Profile %u: pilot, simulated actuator, aircraft response, sensor feedback, displays and systems; %u routed samples.\n",
           p,lab.received);
}

int main(void)
{
    for (unsigned p=0;p<IMA_PROFILE_COUNT;p++) run_profile(p);
    return 0;
}
