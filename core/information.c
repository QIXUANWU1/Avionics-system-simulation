#include "model_steps.h"
#include <math.h>
#include <string.h>

int information_step(const ModelStep *s,Sample *o)
{
    const Sample *a=s->input[0],*b=s->input[1];
    switch (s->id) {
    case SID_echo_fixture: {
        const unsigned phase=(s->tick_ms/1000u)%20u;
        o->v[0]=45+phase; o->v[1]=30+phase; o->v[2]=1;
        return 1;
    }
    case SID_input_status:
        o->v[0]=s->live; o->v[1]=s->required; return 1;
    case SID_display_status:
        o->v[0]=a!=NULL; o->v[1]=b?b->v[0]:0; o->v[2]=b!=NULL; return 1;
    case SID_crew_indication: {
        const Sample *g=s->input[2], *pfd=s->input[3], *eng=s->input[4], *sys=s->input[5];
        const unsigned flags=(!a||a->v[0]==MODE_UNAVAILABLE?1u:0u)|(!b||b->v[1]!=1?2u:0u)|
                   (!g||g->v[2]!=1?4u:0u)|(!pfd?8u:0u)|(!eng?16u:0u)|(!sys||sys->v[1]!=1?32u:0u);
        unsigned count=0;
        for (unsigned x=flags;x;x>>=1) count+=x&1u;
        o->v[0]=flags; o->v[1]=count; o->v[2]=a?a->v[0]:MODE_UNAVAILABLE;
        return 1;
    }
    default: break;
    }
    if (s->live!=s->required) return 0;
    switch (s->id) {
    case SID_nav_context:
        o->v[0]=a->v[0]; o->v[1]=fabs(a->v[0]-b->v[0]); o->v[2]=1; break;
    case SID_weather_quality:
        if (a->v[2]!=1.0 || a->v[0]<1 || a->v[0]>400 || a->v[1]< -30 || a->v[1]>85) return 0;
        /* fall through */
    case SID_weather_summary:
        memcpy(o->v,a->v,sizeof o->v); break;
    case SID_weather_display:
        o->v[0]=b->v[0]; o->v[1]=b->v[1]; o->v[2]=a->v[0]; break;
    case SID_scan_pair_status:
        o->v[0]=fabs((double)a->time_ms-b->time_ms); o->v[1]=b->v[2]==1.0; break;
    case SID_primary_display:
        o->v[0]=a->v[0]; o->v[1]=a->v[1]; o->v[2]=b->v[1]; break;
    case SID_navigation_display:
        o->v[0]=a->v[0]; o->v[1]=b->v[1]; o->v[2]=b->v[2]; break;
    case SID_engine_display:
        if (s->input[2]->v[1]!=1.0) return 0;
        o->v[0]=a->v[0]; o->v[1]=a->v[1]; o->v[2]=b->v[0]; break;
    case SID_radio_indication:
        o->v[0]=a->v[1]==1.0; break;
    default: return 0;
    }
    return 1;
}
