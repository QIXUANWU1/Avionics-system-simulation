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
    Frame fault={.tick_ms=200};
    fault.samples[SID_airdata_decoded]=make(200,6800,130,20);
    fault.samples[SID_attitude_estimate]=make(200,4,3,1);
    const Sample height=run(&fault,SID_display_airdata);
    assert(height.valid && height.v[0]==6800); /* no GNSS */
    assert(run(&fault,SID_primary_display).valid);
    fault.samples[SID_airdata_decoded].valid=0;
    assert(run(&fault,SID_primary_display).valid && !run(&fault,SID_display_airdata).valid);
    fault.samples[SID_gnss_fixture]=make(200,2100,90,75);
    assert(run(&fault,SID_navigation_display).valid); /* no pressure data */
    fault.samples[SID_airdata_decoded]=make(200,6800,130,20);
    assert(run(&fault,SID_airdata_crosscheck).v[0]>80); /* GNSS 2100 m versus pressure 6800 ft */

    fault.samples[SID_gear_position_fixture]=make(200,1,1,0);
    fault.samples[SID_gear_indication]=run(&fault,SID_gear_indication);
    assert(fault.samples[SID_gear_indication].valid && fault.samples[SID_gear_indication].v[0]==1);
    fault.samples[SID_engine_indication]=make(200,.55,420,.12);
    fault.samples[SID_hydraulic_status]=run(&fault,SID_hydraulic_status); /* engine pump without bus sample */
    assert(fault.samples[SID_hydraulic_status].valid && fault.samples[SID_hydraulic_status].v[2]==1);
    fault.samples[SID_plant_motion]=make(200,2000,75,90);
    fault.samples[SID_weight_on_wheels_fixture]=run(&fault,SID_weight_on_wheels_fixture);
    assert(run(&fault,SID_brake_indication).v[0]==0); /* gear extended in flight */
    fault.samples[SID_plant_motion].v[0]=0;
    fault.samples[SID_weight_on_wheels_fixture]=run(&fault,SID_weight_on_wheels_fixture);
    assert(run(&fault,SID_brake_indication).v[0]==1);
    fault.samples[SID_pilot_levers]=make(200,.55,MODE_MANUAL,0);
    assert(run(&fault,SID_gear_position_fixture).v[0]==1); /* weight on wheels holds gear down */
    fault.samples[SID_weight_on_wheels_fixture].v[0]=0;
    assert(run(&fault,SID_gear_position_fixture).v[0]<1);
    fault.samples[SID_weight_on_wheels_fixture].v[0]=1;
    fault.samples[SID_hydraulic_status].valid=0;
    const Sample brake=run(&fault,SID_brake_indication);
    assert(brake.valid && brake.v[0]==0 && brake.v[1]==1 && brake.v[2]==0);
    assert(run(&fault,SID_gear_indication).v[0]==1);
    fault.samples[SID_gear_position_fixture].v[2]=1;
    assert(!run(&fault,SID_gear_indication).valid); /* contradictory limit switches */
    fault.samples[SID_engine_indication].v[0]=0;
    fault.samples[SID_bus_status]=make(200,25,1,1);
    assert(run(&fault,SID_hydraulic_status).v[2]==2);

    puts("Independent flight instruments, gear indication, ground brake and hydraulic fallback passed.");
    puts("Lane loss/disagreement, fuel data, and cooling status cases passed.");
    return 0;
}
