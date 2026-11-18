#ifndef IMA_HEALTH_H
#define IMA_HEALTH_H
#include "sampling_port.h"

typedef enum { HEALTH_START, HEALTH_FRESH, HEALTH_MISSING, HEALTH_STALE, HEALTH_INVALID } ImaHealthStatus;
typedef struct { ImaSequence last; unsigned valid; ImaHealthStatus status; } ImaHealthSample;

static inline int ima_health_update(ImaHealthSample *s,const Message *m,uint32_t epoch)
{
    if (!ima_accept_sample(m,epoch,m->model_ms,&s->last)) return 0;
    s->valid=m->flags;
    return 1;
}

/* Timeouts indicate lack of recent evidence in the observation stream. */
static inline ImaHealthStatus ima_health_status(const ImaHealthSample *s,uint32_t now,uint32_t max_age)
{
    if (!s->last.seen) return now>max_age?HEALTH_MISSING:HEALTH_START;
    if (now<s->last.model_ms) return HEALTH_START;
    if (now-s->last.model_ms>max_age) return HEALTH_STALE;
    return s->valid?HEALTH_FRESH:HEALTH_INVALID;
}
#endif
