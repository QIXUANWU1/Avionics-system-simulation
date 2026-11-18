#include "model_steps.h"
#include <stddef.h>

int systems_step(const ModelStep *s,Sample *o)
{
    const Sample *a=s->input[0],*b=s->input[1],*c=s->input[2],*p=s->previous;
    switch (s->id) {
    case SID_battery_indication:
        o->v[0]=25; o->v[1]=1;
        return 1;
    case SID_bus_status:
        o->v[0]=a&&a->v[0]>.2?28:b&&b->v[1]==1?b->v[0]:0;
        o->v[1]=o->v[0]>20; o->v[2]=o->v[0]==28?2:o->v[1];
        return 1;
    case SID_gear_position_fixture: {
        if (!p&&!a) return 0;
        const double prior=p?p->v[0]:1; /* the synthetic aircraft starts with gear extended */
        const double target=a&&a->v[2]==1?1:0;
        const int direction_allowed=target==1 || (c && c->v[0]==0);
        const double change=a && b && b->v[1]==1 && direction_allowed
                            ? clip_between(target-prior,-.10,.10):0;
        o->v[0]=clip_between(prior+change,0,1);
        o->v[1]=o->v[0]>=.999; o->v[2]=o->v[0]<=.001;
        return 1;
    }
    case SID_hydraulic_status: {
        if (!a && !b) return 0;
        const int main_available=a && a->v[0]>.2;
        const int standby_available=b && b->v[1]==1;
        o->v[0]=main_available?clip_between(a->v[0],0,1):standby_available?.42:0;
        o->v[1]=main_available || standby_available;
        o->v[2]=main_available?1:standby_available?2:0;
        return 1;
    }
    case SID_brake_indication:
        if (!c) return 0;
        o->v[0]=c->v[0]==1 && a && a->v[0]==1 && b && b->v[1]==1;
        o->v[1]=c->v[0]==1;
        o->v[2]=b!=NULL;
        return 1;
    case SID_systems_summary:
        o->v[0]=a&&a->v[1]==1; o->v[1]=b&&b->v[1]==1; o->v[2]=c&&c->v[0]==1;
        return 1;
    default: break;
    }
    if (s->live!=s->required) return 0;
    switch (s->id) {
    case SID_engine_indication:
        o->v[0]=clip_between(a->v[0],0,1); o->v[1]=310+220*o->v[0]; o->v[2]=.05+.16*o->v[0];
        break;
    case SID_fuel_quantity:
        o->v[1]=clip_between(a->v[2],0,1);
        o->v[0]=p?clip_between(p->v[0]-o->v[1]*.1,0,120):120;
        break;
    case SID_gear_indication:
        if (a->v[1]==1&&a->v[2]==1) return 0;
        o->v[0]=a->v[1]==1?1:a->v[2]==1?0:2;
        o->v[1]=a->v[1]==1; o->v[2]=a->v[2]==1;
        break;
    case SID_cabin_indication:
        o->v[0]=b->v[1]==1?295:a->v[1]; o->v[1]=b->v[1]==1;
        break;
    case SID_weight_on_wheels_fixture:
        /* The simulated runway datum is 0 m. This bench starts airborne. */
        o->v[1]=a->v[0];
        o->v[0]=a->v[0]<=1.0 && b->v[1]==1;
        break;
    default: return 0;
    }
    return 1;
}
