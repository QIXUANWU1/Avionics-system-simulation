/* Offline wire and freshness checks: opens no socket and runs no application. */
#include "sampling_port.h"
#include <assert.h>
#include <stdio.h>
static unsigned checks;
#define CHECK(c) do { if (!(c)) { fprintf(stderr,"failed line %d: %s\n",__LINE__,#c); return 1; } ++checks; } while (0)
int main(void)
{
    const Message original={.kind=KIND_SAMPLE,.sid=8,.epoch=0x01234567u,.sequence=UINT32_MAX,.model_ms=0x1234,
        .flags=1,.source_ns=UINT64_C(0x123456789abcdef0),.v={1.25,-2.5,3.75}};
    Message m;
    uint8_t b[WIRE_SIZE];
    wire_encode(b,&original);
    CHECK(!memcmp(b,"IMAR\x03\x01\x08\x00\x67\x45\x23\x01",12));
    CHECK(!wire_decode(b,sizeof b,&m) && m.sequence==original.sequence && m.model_ms==original.model_ms &&
          m.source_ns==original.source_ns && m.v[1]==original.v[1]);
    CHECK(wire_decode(b,sizeof b-1,&m)!=0);
    b[4]=9; CHECK(wire_decode(b,sizeof b,&m)!=0); b[4]=WIRE_VERSION;
    m=original;m.v[2]=NAN;wire_encode(b,&m);CHECK(wire_decode(b,sizeof b,&m)!=0);
    CHECK(ima_newer(0,UINT32_MAX) && !ima_newer(10,10) && !ima_newer(9,10) && !ima_newer(0x80000000u,0));
    m=original;
    const uint32_t epoch=original.epoch, local=original.model_ms;
    ImaSequence s={0};
    CHECK(!ima_accept_sample(&m,epoch+1,local,&s) && !s.seen);
    CHECK(ima_accept_sample(&m,epoch,local,&s) && s.seen && s.sequence==UINT32_MAX);
    CHECK(!ima_accept_sample(&m,epoch,local,&s));
    m.sequence=0;m.model_ms=local+11;
    CHECK(!ima_accept_sample(&m,epoch,local,&s) && s.sequence==UINT32_MAX);
    m.model_ms=local+10; m.flags=2;
    CHECK(!ima_accept_sample(&m,epoch,local,&s) && s.sequence==UINT32_MAX);
    m.flags=0; m.kind=KIND_PROGRESS;
    CHECK(!ima_accept_sample(&m,epoch,local,&s));
    m.kind=KIND_SAMPLE;
    CHECK(ima_accept_sample(&m,epoch,local,&s) && s.sequence==0);
    m.sequence=1;m.epoch++;
    CHECK(!ima_accept_sample(&m,epoch,local,&s) && s.sequence==0);
    m.epoch=epoch; m.model_ms=local;
    CHECK(!ima_accept_sample(&m,epoch,local,&s) && s.sequence==0);
    m.model_ms=local+20;
    CHECK(ima_accept_sample(&m,epoch,local+20,&s) && s.sequence==1 && s.model_ms==local+20);
    printf("%u offline C sampling/wire checks passed\n",checks);
    return 0;
}
