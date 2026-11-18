#ifndef IMA_TRACE_READ_H
#define IMA_TRACE_READ_H
#include "wire.h"
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
    if (n!=sizeof record || wire_decode(record+9,WIRE_SIZE,m) || m->kind!=record[0] ||
        m->model_ms>3600000u) return -1;
    return 1;
}
#endif
