#ifndef IMA_SAMPLING_PORT_H
#define IMA_SAMPLING_PORT_H

#include "wire.h"
#include "core/sim.h"

#define IMA_DOMAIN_PORT(base, domain) ((base) + 100u + (domain))
#define IMA_OBSERVE_PORT(base) ((base) + 200u)
#define IMA_DOMAIN_EPOCH(session, domain) ((session) ^ ((uint32_t)(domain) << 24))

/* Serial arithmetic: duplicate, backwards and ambiguous half range are rejected. */
static inline int ima_newer(uint32_t current, uint32_t previous)
{
    const uint32_t step = current - previous;
    return step != 0 && step < UINT32_C(0x80000000);
}

typedef struct {
    uint32_t sequence, model_ms;
    unsigned seen;
} ImaSequence;

/* Expects a decoded message. Only one configured epoch can update the cache. */
static inline int ima_accept_sample(const Message *m, uint32_t expected_epoch,
                                    uint32_t local_ms, ImaSequence *seq)
{
    if (m->kind != KIND_SAMPLE || m->epoch != expected_epoch || m->flags > 1u ||
        (m->model_ms > local_ms && m->model_ms - local_ms > SIM_STEP_MS) ||
        (seq->seen && (!ima_newer(m->sequence, seq->sequence) || m->model_ms<seq->model_ms))) {
        return 0;
    }
    seq->sequence = m->sequence;
    seq->model_ms = m->model_ms;
    seq->seen = 1;
    return 1;
}
#endif
