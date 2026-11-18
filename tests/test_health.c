#include "health.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    ImaHealthSample s={0};
    assert(ima_health_status(&s,70,70)==HEALTH_START);
    assert(ima_health_status(&s,71,70)==HEALTH_MISSING);
    Message m={.kind=KIND_SAMPLE,.sid=SID_imu_sample,.epoch=20,.sequence=2,.model_ms=80,.flags=1};
    assert(!ima_health_update(&s,&m,21));
    assert(ima_health_update(&s,&m,20));
    assert(ima_health_status(&s,150,70)==HEALTH_FRESH);
    assert(ima_health_status(&s,151,70)==HEALTH_STALE);
    m.flags=0;
    assert(!ima_health_update(&s,&m,20)); /* duplicate must not replace the cached valid value */
    assert(ima_health_status(&s,150,70)==HEALTH_FRESH);
    m.sequence=3;m.model_ms=160;
    assert(ima_health_update(&s,&m,20));
    assert(ima_health_status(&s,160,70)==HEALTH_INVALID);
    m.sequence=4;m.model_ms=159;m.flags=1;
    assert(!ima_health_update(&s,&m,20));
    m.model_ms=180;
    assert(ima_health_update(&s,&m,20));
    assert(ima_health_status(&s,180,70)==HEALTH_FRESH);
    puts("Passive health checks: startup, missing, stale, invalid, wrong session, duplicate, older and fresh recovery.");
    return 0;
}
