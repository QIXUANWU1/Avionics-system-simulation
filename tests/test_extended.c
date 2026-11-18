#include "model_steps.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static Sample run(const Frame *in,ServiceId id)
{
    Frame out;
    sim_service(in,id,&out,NULL,NULL);
    return out.samples[id];
}
static Sample make(uint32_t t,double a,double b,double c)
{
    return (Sample){.v={a,b,c},.time_ms=t,.sequence=1,.valid=1};
}

int main(void)
{
    Frame f={.tick_ms=100};
    f.samples[SID_imu_sample]=make(100,5,1,2);
    f.samples[SID_imu_backup]=make(100,4.9,1,2);
    Sample selected=run(&f,SID_attitude_select);
    assert(selected.valid && fabs(selected.v[0]-4.95)<.0001 && run(&f,SID_inertial_agreement).v[2]==2);
    f.samples[SID_imu_backup].time_ms=10;
    selected=run(&f,SID_attitude_select);
    assert(selected.valid && selected.v[0]==5 && run(&f,SID_inertial_agreement).v[2]==1);
    f.samples[SID_imu_backup]=make(100,12,1,2);
    assert(!run(&f,SID_attitude_select).valid && run(&f,SID_inertial_agreement).v[2]==3);
    f.samples[SID_imu_sample].valid=0;
    selected=run(&f,SID_attitude_select);
    assert(selected.valid && selected.v[0]==12);
    f.samples[SID_imu_backup].valid=0;
    assert(!run(&f,SID_attitude_select).valid);
    f.samples[SID_control_effort]=make(100,.1,.2,.04);
    f.samples[SID_shadow_effort]=make(100,.11,.2,.05);
    assert(run(&f,SID_control_agreement).v[2]==1);
    f.samples[SID_shadow_effort].v[0]=.5;
    assert(run(&f,SID_control_agreement).v[2]==2);
    f.samples[SID_shadow_effort].time_ms=55;
    assert(run(&f,SID_control_agreement).v[2]==0);

    f.samples[SID_fuel_quantity]=make(100,120,.1,0);
    f.samples[SID_engine_indication]=make(100,.5,450,.2);
    Sample fuel=run(&f,SID_fuel_endurance);
    assert(fuel.valid && fabs(fuel.v[0]-10)<.00001 && fuel.v[2]==720);
    f.samples[SID_engine_indication].v[2]=0;
    assert(!run(&f,SID_fuel_endurance).valid);
    f.samples[SID_cabin_pressure]=make(100,5000,6,850);
    f.samples[SID_hydraulic_reserve]=make(100,80,1,0);
    f.samples[SID_avionics_cooling]=make(100,310,1,0);
    assert(run(&f,SID_system_cautions).v[2]==1);
    f.samples[SID_avionics_cooling]=make(100,325,1,1);
    assert(run(&f,SID_system_cautions).v[2]==0);
    puts("Lane loss/disagreement, fuel data, and cooling status cases passed.");
    return 0;
}
