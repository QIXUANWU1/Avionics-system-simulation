/* Runs the actual C producers and display formatter, then the Linux inference. */
#include "advisory.h"
#include <assert.h>
#include <stdio.h>
#include <math.h>

static AdvisoryWorkspace work;
int main(void)
{
    AdvisoryModel *model=ima_advisory_create();
    assert(model);
    Frame empty, src, baro, nav_input, raw, nav, radar, checked, display;
    sim_init(&empty);empty.tick_ms=500;
    sim_service(&empty,SID_plant_motion,&src,NULL,NULL);
    sim_service(&src,0,&baro,NULL,NULL);
    sim_service(&baro,1,&nav_input,NULL,NULL);
    sim_service(&nav_input,2,&raw,NULL,NULL);
    sim_service(&raw,ADVISORY_NAV,&nav,NULL,NULL);
    sim_service(&nav,ADVISORY_RADAR,&radar,NULL,NULL);
    sim_service(&radar,5,&checked,NULL,NULL);
    sim_service(&checked,6,&display,NULL,NULL);
    assert(display.samples[6].valid && display.samples[6].v[1]==radar.samples[ADVISORY_RADAR].v[1] &&
           display.samples[6].v[2]==radar.samples[ADVISORY_NAV].v[0]);
    const AdvisoryResult base=ima_advisory_step(model,&display,&work);
    assert(base.valid && isfinite(base.score) && base.score>=0 && base.score<=1 &&
           base.radar_sequence==radar.samples[ADVISORY_RADAR].sequence && base.nav_sequence==nav.samples[ADVISORY_NAV].sequence);
    assert(ima_advisory_step(model,&display,&work).score==base.score);
    Frame variant=display;
    variant.samples[ADVISORY_RADAR].v[1]=50;
    AdvisoryResult alt=ima_advisory_step(model,&variant,&work);
    assert(alt.valid && fabsf(alt.score-base.score)>.03f);
    variant=display;variant.samples[ADVISORY_NAV].v[0]=13500;
    alt=ima_advisory_step(model,&variant,&work);
    assert(alt.valid && fabsf(alt.score-base.score)>.03f);
    variant=display;variant.samples[ADVISORY_RADAR].v[2]=0;
    assert(!ima_advisory_step(model,&variant,&work).valid);
    sim_service(&variant,5,&checked,NULL,NULL);
    sim_service(&checked,6,&display,NULL,NULL);
    assert(!display.samples[6].valid);
    variant=radar;variant.tick_ms+=311;
    assert(!ima_advisory_step(model,&variant,&work).valid);
    variant=radar;variant.samples[ADVISORY_NAV].v[0]=NAN;
    assert(!ima_advisory_step(model,&variant,&work).valid);
    _Static_assert(sizeof work==256u*1024u,"synthetic scan budget");
    ima_advisory_destroy(model);
    printf("Weather display and pinned Genann: two C producers, both influences, invalid/stale/NaN; workspace %zu B.\n",sizeof work);
    return 0;
}
