#include "rtos_local_dds.h"
#include "generated_layout.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    for (unsigned profile=0; profile<IMA_PROFILE_COUNT; ++profile) {
        const Sample source={.time_ms=100,.sequence=1,.valid=1,.v={70,2200,275}};
        unsigned domain;
        Message message;
        Sample received={0};
        ImaRtosDdsReader reader={.profile=profile,.board='A',.consumer=SID_airdata_words,.session=77};

        assert(ima_rtos_dds_prepare(profile,'A',SID_airdata_sample,77,&source,100000000,&domain,&message));
        assert(domain==ima_owner[profile][SID_airdata_words]); /* native and container share a Linux domain */
        assert(!ima_rtos_dds_accept(&reader,domain+1,&message,110,&received));
        assert(!ima_rtos_dds_accept(&reader,domain,&message,171,&received));
        assert(!ima_rtos_dds_accept(&reader,domain,&message,80,&received));
        message.epoch++;
        assert(!ima_rtos_dds_accept(&reader,domain,&message,110,&received));
        message.epoch--;
        assert(ima_rtos_dds_accept(&reader,domain,&message,110,&received));
        assert(received.valid==1 && received.v[1]==2200);
        assert(!ima_rtos_dds_accept(&reader,domain,&message,110,&received));
        message.sequence++;
        message.flags=0;
        assert(ima_rtos_dds_accept(&reader,domain,&message,110,&received) && !received.valid);

        assert(!ima_rtos_dds_prepare(profile,'A',SID_plant_motion,77,&source,0,&domain,&message));
        assert(!ima_rtos_dds_prepare(profile,'D',SID_route_indication,77,&source,0,&domain,&message));
        assert(ima_rtos_dds_prepare(profile,'C',SID_imu_backup,77,&source,0,&domain,&message));
        reader=(ImaRtosDdsReader){.profile=profile,.board='B',.consumer=SID_attitude_select,.session=77};
        assert(!ima_rtos_dds_accept(&reader,domain,&message,110,&received)); /* neighbour board */
        reader=(ImaRtosDdsReader){.profile=profile,.board='C',.consumer=SID_shadow_effort,.session=77};
        assert(ima_rtos_dds_accept(&reader,domain,&message,110,&received));

        reader=(ImaRtosDdsReader){.profile=profile,.board='C',.consumer=SID_imu_backup,.session=77};
        assert(!ima_rtos_dds_accept(&reader,domain,&message,110,&received)); /* no self loop */
    }
    puts("Board-local DDS policy: local native/container delivery, rejected board, owner, age, session and duplicates.");
    return 0;
}
