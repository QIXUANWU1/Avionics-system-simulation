#include "arinc429_word.h"
#include "model_steps.h"
#include <math.h>

enum { LAB_ALT_LABEL=0xd2, LAB_SPEED_LABEL=0x6b, LAB_SDI=1, MAX_BUNDLE_AGE_MS=100 };
static const double metres_to_feet=3.280839895013123;
static const double mps_to_knots=1.943844492440605;

double isa_temperature(double elevation_m)
{
    return 288.15-.0065*clip_between(elevation_m,-1000,11000);
}

static double isa_static_pa(double elevation_m)
{
    /* Troposphere approximation used only over the synthetic bench envelope. */
    return 101325*pow(isa_temperature(elevation_m)/288.15,5.25588);
}

double airspeed_proxy_mps(double true_mps,double elevation_m)
{
    const double density=isa_static_pa(elevation_m)/(287.05*isa_temperature(elevation_m));
    return true_mps*sqrt(density/1.225);
}

static int exact_u32(double x,uint32_t *raw)
{
    if (!isfinite(x) || x<0 || x>UINT32_MAX || x!=floor(x)) return 0;
    *raw=(uint32_t)x;
    return 1;
}

int airdata_step(const ModelStep *s,Sample *o)
{
    const Sample *a=s->input[0],*p=s->previous;
    switch (s->id) {
    case SID_airdata_words: {
        /* Private test labels and scaling, not an aircraft label map. */
        const enum A429Ssm quality=a?A429_NORMAL:A429_NO_DATA;
        const double alt=a?a->v[1]*metres_to_feet:0,speed=a?a->v[0]*mps_to_knots:0;
        if (alt< -2000 || alt>65000 || speed<0 || speed>600) return 0;
        uint32_t alt_word,speed_word;
        if (!a429_bnr_encode((A429Bnr){LAB_ALT_LABEL,LAB_SDI,(int32_t)lround(alt*4),quality},&alt_word) ||
            !a429_bnr_encode((A429Bnr){LAB_SPEED_LABEL,LAB_SDI,(int32_t)lround(speed*8),quality},&speed_word)) return 0;
        o->v[0]=alt_word; o->v[1]=speed_word; o->v[2]=a?a->time_ms:s->tick_ms;
        return 1;
    }
    case SID_airdata_decoded: {
        uint32_t aw,sw,source_ms;
        A429Bnr altitude,speed;
        if (!a || !exact_u32(a->v[0],&aw) || !exact_u32(a->v[1],&sw) || !exact_u32(a->v[2],&source_ms) ||
            source_ms>a->time_ms || s->tick_ms<source_ms || s->tick_ms-source_ms>MAX_BUNDLE_AGE_MS ||
            !a429_bnr_decode(aw,&altitude) || !a429_bnr_decode(sw,&speed) ||
            altitude.label!=LAB_ALT_LABEL || speed.label!=LAB_SPEED_LABEL ||
            altitude.sdi!=LAB_SDI || speed.sdi!=LAB_SDI ||
            altitude.ssm!=A429_NORMAL || speed.ssm!=A429_NORMAL) return 0;
        const double alt_ft=altitude.bnr*.25,speed_kt=speed.bnr*.125;
        if (alt_ft< -2000 || alt_ft>65000 || speed_kt<0 || speed_kt>600) return 0;
        o->v[0]=alt_ft; o->v[1]=speed_kt; o->v[2]=s->tick_ms-source_ms;
        return 1;
    }
    case SID_atmosphere_sample:
        if (!a || a->v[2]<180 || a->v[2]>340) return 0;
        o->v[1]=a->v[2];
        o->v[0]=isa_static_pa(a->v[1])*.01;
        o->v[2]=(o->v[0]*100)/(287.05*o->v[1]);
        return 1;
    case SID_display_airdata:
        if (!a) return 0;
        o->v[0]=a->v[0];
        if (p && s->tick_ms>p->time_ms) {
            const double rate=(o->v[0]-p->v[0])*60000/(double)(s->tick_ms-p->time_ms);
            o->v[1]=.75*p->v[1]+.25*rate;
        }
        return 1;
    default: return 0;
    }
}
