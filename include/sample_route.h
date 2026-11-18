#ifndef IMA_SAMPLE_ROUTE_H
#define IMA_SAMPLE_ROUTE_H
#include "sampling_port.h"

typedef struct {
    unsigned profile, domain;
    uint32_t session;
    ImaSequence last[SIM_SERVICES];
} ImaSampleRoute;

enum ImaRx { IMA_RX_OK, IMA_RX_BAD_WIRE, IMA_RX_BAD_ROUTE, IMA_RX_BAD_SAMPLE };

/* The OS adapter maps its own transport source to the configured logical domain.
   A single consumer owns this route state. On rejection, the cache is untouched. */
enum ImaRx ima_route_accept_message(ImaSampleRoute *route,unsigned from,const Message *message,
                                    uint32_t model_ms,Frame *cache);
enum ImaRx ima_route_accept(ImaSampleRoute *route,unsigned from,const uint8_t *data,int length,
                            uint32_t model_ms,Frame *cache,Message *accepted);
#endif
