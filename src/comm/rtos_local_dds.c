#include "rtos_local_dds.h"
#include "generated_layout.h"

_Static_assert(SIM_SERVICES == IMA_CATALOG_COUNT && SIM_SERVICES <= 64, "DDS service table");

static int valid_board(unsigned profile, unsigned board, unsigned service)
{
    return profile < IMA_PROFILE_COUNT && service < SIM_SERVICES &&
           board >= 'A' && board <= 'C' && ima_rtos_board[profile][service] == board;
}

static int finite_values(const double values[3])
{
    return isfinite(values[0]) && isfinite(values[1]) && isfinite(values[2]);
}

int ima_rtos_dds_prepare(unsigned profile, unsigned board, ServiceId producer,
                        uint32_t session, const Sample *sample, uint64_t source_ns,
                        unsigned *source_domain, Message *message)
{
    if (!valid_board(profile,board,producer) ||
        !(ima_rtos_publishers[profile] & (UINT64_C(1) << producer)) ||
        (sample->valid != 0 && sample->valid != 1) || !finite_values(sample->v))
        return 0;

    const unsigned owner=ima_owner[profile][producer];
    *message=(Message){.kind=KIND_SAMPLE,.sid=(uint16_t)producer,
        .epoch=IMA_DOMAIN_EPOCH(session,owner),.sequence=sample->sequence,
        .model_ms=sample->time_ms,.flags=(uint32_t)sample->valid,.source_ns=source_ns};
    memcpy(message->v,sample->v,sizeof message->v);
    *source_domain=owner;
    return 1;
}

int ima_rtos_dds_accept(ImaRtosDdsReader *reader, unsigned source_domain,
                       const Message *message, uint32_t now_ms, Sample *sample)
{
    const unsigned profile=reader->profile, consumer=reader->consumer;
    const unsigned producer=message->sid;
    if (!valid_board(profile,reader->board,consumer) || producer >= SIM_SERVICES ||
        !(ima_rtos_local_inputs[profile][consumer] & (UINT64_C(1) << producer)) ||
        source_domain != ima_owner[profile][producer] || !finite_values(message->v) ||
        (now_ms >= message->model_ms && now_ms-message->model_ms > ima_max_age_ms[producer]))
        return 0;

    const Service *service=&sim_services[consumer];
    for (int i=0; i<service->ninputs; ++i) {
        if (service->inputs[i] != (int)producer) continue;
        if (!ima_accept_sample(message,IMA_DOMAIN_EPOCH(reader->session,source_domain),
                               now_ms,&reader->last[i]))
            return 0;
        *sample=(Sample){.time_ms=message->model_ms,.sequence=message->sequence,.valid=(int)message->flags};
        memcpy(sample->v,message->v,sizeof sample->v);
        return 1;
    }
    return 0;
}
