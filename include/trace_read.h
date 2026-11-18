#ifndef IMA_TRACE_READ_H
#define IMA_TRACE_READ_H
#include "wire.h"
#include "core/sim.h"
#include "generated_layout.h"
#include <stdio.h>

static inline int ima_trace_header(FILE *f)
{
    char header[8];
    return fread(header,1,sizeof header,f)==sizeof header && !memcmp(header,"IMATRCE1",8);
}

/* 1 record, 0 clean EOF, -1 malformed or read error. */
static inline int ima_trace_next(FILE *f,Message *m)
{
    uint8_t record[9+WIRE_SIZE];
    const size_t n=fread(record,1,sizeof record,f);
    if (!n) return ferror(f)?-1:0;
    if (n!=sizeof record) return -1;
    uint8_t *wire=record+9;
    const int legacy=wire[4]==4 || wire[4]==5 ? wire[4] : 0;
    const unsigned legacy_advisory=legacy==4?59u:62u;
    if (legacy) wire[4]=WIRE_VERSION;
    if (wire_decode(wire,WIRE_SIZE,m) || m->kind!=record[0] || m->model_ms>3600000u) return -1;
    if (legacy && m->kind==KIND_ADVISORY) {
        if (m->sid!=legacy_advisory) return -1;
        m->sid=IMA_ADVISORY_ID;
    } else if (legacy && m->kind!=KIND_PROGRESS && m->sid>=legacy_advisory) {
        return -1;
    }
    /* Old versions used distinct meanings for these display fields. */
    if (legacy && m->kind==KIND_SAMPLE &&
        (m->sid==SID_brake_indication || m->sid==SID_airdata_crosscheck)) m->flags=0;
    return 1;
}
#endif
