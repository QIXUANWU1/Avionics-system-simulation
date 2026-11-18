#include "model_steps.h"
#include <math.h>
#include <string.h>

/* A desktop signal-response fixture, with no aircraft gains or I/O mapping. */
static const double demo_angle_deg[3]={35,20,18};

static int mode_active(const Sample *s)
{
    return s && (s->v[0]==MODE_MANUAL || s->v[0]==MODE_CENTER);
}

int flight_step(const ModelStep *s,Sample *o)
{
    const Sample *a=s->input[0],*b=s->input[1],*c=s->input[2],*d=s->input[3],*p=s->previous;
    const uint32_t phase=s->tick_ms%12000u;
    switch (s->id) {
    case SID_pilot_axes: {
        const double direction=phase<1000?0:phase<4000?1:phase<7500?0:-1;
        o->v[0]=.7*direction; o->v[1]=.4*direction; o->v[2]=.2*direction;
        return 1;
    }
    case SID_pilot_levers:
        o->v[0]=phase<6000?.55:.40;
        o->v[1]=phase>=4000&&phase<7500?MODE_CENTER:MODE_MANUAL;
        o->v[2]=phase<1000;
        return 1;
    case SID_plant_attitude:
    case SID_actuator_response: {
        const int plant=s->id==SID_plant_attitude;
        const int driven=a && (plant || mode_active(b));
        for (unsigned i=0;i<3;i++) {
            const double old=p?axis(p->v[i]):0;
            const double target=driven?axis(a->v[i]):0;
            o->v[i]=old+(plant?.10:.25)*(target-old);
        }
        return 1;
    }
    case SID_plant_motion: {
        const double old_alt=p?p->v[0]:2100,old_speed=p?p->v[1]:76;
        o->v[0]=clip_between(old_alt+(a?axis(a->v[1])*.10:0),1900,2300);
        const double speed_target=55+45*(b?clip_between(b->v[0],0,1):.5);
        o->v[1]=old_speed+.025*(speed_target-old_speed);
        o->v[2]=fmod((p?p->v[2]:90)+360+(a?.30*axis(a->v[2]):0),360);
        return 1;
    }
    case SID_flight_mode: {
        unsigned missing=(!a?1u:0u)|(!b?2u:0u)|(!c?4u:0u)|(!d||d->v[1]!=1.0?8u:0u);
        if (c && c->v[1]!=MODE_MANUAL && c->v[1]!=MODE_CENTER) missing|=4u;
        o->v[0]=missing?MODE_UNAVAILABLE:c->v[1]; o->v[1]=missing; o->v[2]=!missing;
        return 1;
    }
    case SID_control_status:
        o->v[0]=a!=NULL; o->v[1]=b!=NULL; o->v[2]=c?c->v[0]:MODE_UNAVAILABLE;
        return 1;
    default: break;
    }
    if (s->live!=s->required) return 0;
    switch (s->id) {
    case SID_baro_fixture: o->v[0]=a->v[0]-.5; break;
    case SID_gnss_fixture: o->v[0]=a->v[0]+1.5; break;
    case SID_imu_sample:
        for (unsigned i=0;i<3;i++) o->v[i]=round(axis(a->v[i])*demo_angle_deg[i]*100.)*.01;
        break;
    case SID_airdata_sample:
        o->v[0]=airspeed_proxy_mps(a->v[1],a->v[0]); o->v[1]=a->v[0]; o->v[2]=isa_temperature(a->v[0]);
        break;
    case SID_attitude_estimate:
        for (unsigned i=0;i<3;i++) {
            if (fabs(a->v[i])>demo_angle_deg[i]) return 0;
            o->v[i]=a->v[i];
        }
        break;
    case SID_attitude_reference:
        if (!mode_active(b)) return 0;
        for (unsigned i=0;i<3;i++) o->v[i]=b->v[0]==MODE_CENTER?0:axis(a->v[i]);
        break;
    case SID_control_effort:
        if (!mode_active(c)) return 0;
        /* Toy feedback only, on normalized model coordinates. */
        for (unsigned i=0;i<3;i++) {
            const double target=c->v[0]==MODE_CENTER?0:axis(a->v[i]);
            o->v[i]=axis(.65*(target-axis(b->v[i]/demo_angle_deg[i])));
        }
        break;
    default: return 0;
    }
    return 1;
}
