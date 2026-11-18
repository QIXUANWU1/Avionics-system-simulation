#include "sample_route.h"
#include "generated_layout.h"

enum ImaRx ima_route_accept(ImaSampleRoute *route,unsigned from,const uint8_t *data,int length,
                            uint32_t model_ms,Frame *cache,Message *m)
{
    if (wire_decode(data,length,m) || m->kind!=KIND_SAMPLE || m->sid>=SIM_SERVICES) return IMA_RX_BAD_WIRE;
    const unsigned profile=route->profile,domain=route->domain;
    if (profile>=IMA_PROFILE_COUNT || domain>=ima_domain_count[profile]) return IMA_RX_BAD_ROUTE;
    const unsigned owner=ima_owner[profile][m->sid];
    if (from!=owner || !(ima_fanout[profile][m->sid]&(UINT32_C(1)<<domain))) return IMA_RX_BAD_ROUTE;
    if (!ima_accept_sample(m,IMA_DOMAIN_EPOCH(route->session,owner),model_ms,&route->last[m->sid]))
        return IMA_RX_BAD_SAMPLE;
    Sample *s=&cache->samples[m->sid];
    memcpy(s->v,m->v,sizeof s->v);
    s->valid=(int)m->flags; s->sequence=m->sequence; s->time_ms=m->model_ms;
    return IMA_RX_OK;
}
