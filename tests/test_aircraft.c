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
    unsigned produced_valid[SIM_SERVICES];
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
        if (published[id].valid) lab->produced_valid[id]=1;
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
    double bank_after_input=0, fuel_before=0, heading_before=0, route_before=0;
    for (uint32_t t=0;t<=10500;t+=SIM_STEP_MS) {
        tick(&lab,p,t);
        if (t==800) {
            assert(fabs(sample(&lab,p,SID_primary_display).v[0])<.01);
            assert(sample(&lab,p,SID_flight_mode).v[0]==MODE_MANUAL);
            assert(sample(&lab,p,SID_crew_indication).v[0]==0);
            fuel_before=sample(&lab,p,SID_fuel_quantity).v[0];
            heading_before=sample(&lab,p,SID_navigation_display).v[1];
            route_before=sample(&lab,p,SID_route_indication).v[0];
            assert(route_before>2 && route_before<2.5);
        } else if (t==1500) {
            assert(sample(&lab,p,SID_gear_indication).v[0]==2);
            assert(sample(&lab,p,SID_gear_position_fixture).v[0]>0);
        } else if (t==3500) {
            assert(sample(&lab,p,SID_control_effort).v[0]>.1);
            assert(sample(&lab,p,SID_actuator_response).v[0]>.1);
            bank_after_input=sample(&lab,p,SID_primary_display).v[0];
            assert(bank_after_input>7.0);
            assert(sample(&lab,p,SID_primary_display).v[1]>2);
            const double indicated_airspeed=sample(&lab,p,SID_airdata_decoded).v[1];
            assert(indicated_airspeed>90 && indicated_airspeed<200);

            const Sample nav=sample(&lab,p,SID_navigation_display), gps=sample(&lab,p,SID_gnss_fixture);
            assert(nav.v[1]>heading_before && fabs(nav.v[1]-gps.v[1])<1);
            assert(fabs(nav.v[2]-gps.v[2])<.5);
            const Sample position=sample(&lab,p,SID_gnss_position);
            const Sample route=sample(&lab,p,SID_route_indication);
            assert(position.v[0]>200 && fabs(position.v[1])<40);
            assert(route.v[0]<route_before-.07 && route.v[0]>1.9);
            assert(route.v[2]>0 && route.v[2]<.1);
            assert(sample(&lab,p,SID_display_airdata).v[0]>6000);
            assert(sample(&lab,p,SID_atmosphere_sample).v[0]<1013.25);
            assert(sample(&lab,p,SID_gear_indication).v[0]==0);
            assert(sample(&lab,p,SID_weather_display).v[2]>1900);
        } else if (t==6900) {
            assert(sample(&lab,p,SID_flight_mode).v[0]==MODE_CENTER);
            assert(fabs(sample(&lab,p,SID_primary_display).v[0])<bank_after_input*.1);
            const double fuel=sample(&lab,p,SID_fuel_quantity).v[0];
            const double indicated=sample(&lab,p,SID_engine_display).v[2];
            assert(indicated<fuel_before && fabs(indicated-fuel)<.05);
            assert(fuel>119 && fuel<120);
            assert(sample(&lab,p,SID_radio_indication).v[0]==1);
            assert(sample(&lab,p,SID_cabin_indication).v[1]==1);
        }
    }
    assert(sample(&lab,p,SID_primary_display).v[0]<-7.0);
    assert(lab.received>1000);
    for (unsigned id=0;id<SIM_SERVICES;id++) assert(lab.produced_valid[id]);

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

    lost=lab.frame[ima_owner[p][SID_route_indication]];
    lost.samples[SID_gnss_position].time_ms=0;
    sim_service(&lost,SID_route_indication,&out,NULL,NULL);
    assert(!out.samples[SID_route_indication].valid);

    lost=lab.frame[ima_owner[p][SID_position_fixture]];
    const double east=lost.samples[SID_position_fixture].v[0];
    lost.samples[SID_plant_motion].valid=0;
    sim_service(&lost,SID_position_fixture,&out,NULL,NULL);
    assert(!out.samples[SID_position_fixture].valid && out.samples[SID_position_fixture].v[0]==east);
    out.samples[SID_plant_motion].valid=1;
    out.tick_ms+=20;
    sim_service(&out,SID_position_fixture,&lost,NULL,NULL);
    assert(lost.samples[SID_position_fixture].valid && lost.samples[SID_position_fixture].v[0]==east);

    printf("Profile %u: pilot, simulated actuator, aircraft response, sensor feedback, displays and systems; %u routed samples and all %u service outputs.\n",
           p,lab.received,(unsigned)SIM_SERVICES);
}

int main(void)
{
    for (unsigned p=0;p<IMA_PROFILE_COUNT;p++) run_profile(p);
    return 0;
}
