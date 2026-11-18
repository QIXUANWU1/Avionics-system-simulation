#include "cabin_fixture.h"
#include "model_steps.h"
#include <math.h>

static double max_difference(const Sample *a,const Sample *b)
{
    double n=0;
    for (unsigned j=0;j<3;j++) n=fmax(n,fabs(a->v[j]-b->v[j]));
    return n;
}

int extended_systems_step(const ModelStep *s,Sample *o)
{
    const Sample *a=s->input[0],*b=s->input[1],*c=s->input[2],*p=s->previous;
    switch (s->id) {
    case SID_imu_backup:
        if (!a) return 0;
        for (unsigned j=0;j<3;j++) {
            const double scale=demo_attitude_scale(j);
            const double bias=(j==1?-.02:.02);
            o->v[j]=clip_between(round((axis(a->v[j])*scale+bias)*50)*.02,-scale,scale);
        }
        return 1;
    case SID_attitude_select:
        if ((!a&&!b) || (a&&b&&max_difference(a,b)>2.0)) return 0;
        for (unsigned j=0;j<3;j++) o->v[j]=a&&b?.5*(a->v[j]+b->v[j]):(a?a:b)->v[j];
        return 1;
    case SID_inertial_agreement:
        o->v[0]=s->live;
        o->v[1]=a&&b?max_difference(a,b):0;
        o->v[2]=!s->live?0:s->live==1?1:o->v[1]>2?3:2; /* missing/single/dual/disagree */
        return 1;
    case SID_shadow_effort:
        if (!a||!b||!c||c->v[0]==MODE_UNAVAILABLE) return 0;
        for (unsigned j=0;j<3;j++) {
            const double target=c->v[0]==MODE_CENTER?0:axis(a->v[j]);
            o->v[j]=axis(.65*(target-axis(b->v[j]/demo_attitude_scale(j))));
        }
        return 1;
    case SID_control_agreement:
        o->v[1]=s->live;
        if (a&&b) {
            o->v[0]=max_difference(a,b);
            const double skew=fabs((double)a->time_ms-b->time_ms);
            o->v[2]=skew>40?0:o->v[0]>.10?2:1; /* unpaired/within tolerance/disagreement */
        }
        return 1;
    case SID_system_cautions:
        o->v[0]=a&&a->v[0]<=8000;
        o->v[1]=b&&b->v[1]==1;
        o->v[2]=c&&c->v[1]==1&&c->v[2]==0;
        return 1;
    default: break;
    }
    if (s->live!=s->required) return 0;
    switch (s->id) {
    case SID_airdata_mach: {
        if (a->v[1]<180||a->v[1]>340||a->v[2]<=0||b->v[1]<0) return 0;
        const double tas=(b->v[1]/1.943844492440605)*sqrt(1.225/a->v[2]);
        o->v[0]=tas/sqrt(1.4*287.05*a->v[1]); o->v[1]=tas*1.943844492440605; o->v[2]=b->v[0];
        break;
    }
    case SID_track_rate:
        o->v[0]=a->v[1]; o->v[2]=a->v[2]; /* wrap distance across 360 degrees */
        if (p && s->tick_ms>p->time_ms)
            o->v[1]=remainder(o->v[0]-p->v[0],360)*1000/(double)(s->tick_ms-p->time_ms);
        break;
    case SID_fuel_endurance:
        if (b->v[2]<=0 || a->v[0]<0) return 0;
        o->v[0]=a->v[0]/(b->v[2]*60); o->v[1]=a->v[0]; o->v[2]=b->v[2]*3600;
        break;
    case SID_bus_load:
        o->v[0]=a->v[0];
        o->v[1]=a->v[1]==1?4+8*(b->v[1]==1)+2*(c->v[0]==1):0;
        o->v[2]=o->v[0]*o->v[1]; break;
    case SID_hydraulic_reserve:
        o->v[0]=clip_between(a->v[0],0,1)*155; /* bench pressure proxy */
        o->v[1]=a->v[1]==1; break;
    case SID_cabin_pressure: {
        const double outside=b->v[0];
        if (outside<=0 || outside>1100) return 0;
        const double setpoint=c->v[1]==1?fmin(a->v[0],5000):a->v[0];
        const double cabin=1013.25*pow(isa_temperature(setpoint*.3048)/288.15,5.25588);
        o->v[0]=setpoint; o->v[1]=fmax(cabin-outside,0)*.1; o->v[2]=cabin;
        break;
    }
    case SID_cabin_lighting: {
        const unsigned scene=(s->tick_ms/2400u)%4u;
        o->v[2]=a->v[1]==1 && b->v[1]==1;
        o->v[0]=!o->v[2]?0:scene==CABIN_DIM?9:220;
        o->v[1]=scene;
        break;
    }
    case SID_camera_frame_source:
        if (a->v[2]!=1) return 0;
        o->v[0]=a->v[0]; o->v[1]=a->v[1]; o->v[2]=a->sequence;
        break;
    case SID_airdata_crosscheck:
        o->v[0]=fabs(a->v[0]-b->v[0]*3.280839895013123);
        o->v[1]=a->v[2]; o->v[2]=o->v[0]<150; break;
    case SID_electrical_display:
        o->v[0]=a->v[0]; o->v[1]=a->v[1]; o->v[2]=b->v[2]; break;
    case SID_outside_temperature_display:
        o->v[0]=a->v[1]-273.15; o->v[1]=a->v[0]; o->v[2]=a->v[2]; break;
    case SID_avionics_cooling:
        o->v[0]=a->v[0]+2+b->v[2]*.017; o->v[1]=a->v[1]==1; o->v[2]=o->v[0]>320;
        break;
    default: return 0;
    }
    return 1;
}
