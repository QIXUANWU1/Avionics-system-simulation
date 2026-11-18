#ifndef IMA_RTOS_LOCAL_DDS_H
#define IMA_RTOS_LOCAL_DDS_H

#include "sampling_port.h"

/* One reader per service. Board is 'A', 'B' or 'C'. All participants on that
   board share a session and the board clock supplied by the platform. */
typedef struct {
    unsigned profile, board;
    ServiceId consumer;
    uint32_t session;
    ImaSequence last[SIM_INPUTS];
} ImaRtosDdsReader;

/* The platform maps the prepared fields to imalab::State and publishes them
   on its local DDS participant. Neither call performs I/O or allocates. */
int ima_rtos_dds_prepare(unsigned profile, unsigned board, ServiceId producer,
                        uint32_t session, const Sample *sample, uint64_t source_ns,
                        unsigned *source_domain, Message *message);
int ima_rtos_dds_accept(ImaRtosDdsReader *reader, unsigned source_domain,
                       const Message *message, uint32_t now_ms, Sample *sample);

#endif
