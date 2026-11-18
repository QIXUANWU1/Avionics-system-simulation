#include "arinc429_word.h"
#include "sim.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

int main(void)
{
    A429Bnr readback, input={.label=0xd2,.sdi=1,.bnr=-4,.ssm=A429_NORMAL};
    uint32_t word;
    assert(a429_bnr_encode(input,&word) && word==UINT32_C(0xfffff14b));
    assert(a429_bnr_decode(word,&readback));
    assert(readback.label==input.label && readback.sdi==input.sdi && readback.bnr==-4 && readback.ssm==A429_NORMAL);
    for (unsigned i=0;i<32;i++) assert(!a429_bnr_decode(word^(UINT32_C(1)<<i),&readback));
    input.bnr=-262144;
    assert(a429_bnr_encode(input,&word) && a429_bnr_decode(word,&readback) && readback.bnr==input.bnr);
    input.bnr=262143;
    assert(a429_bnr_encode(input,&word) && a429_bnr_decode(word,&readback) && readback.bnr==input.bnr);
    input.bnr=262144;
    assert(!a429_bnr_encode(input,&word));
    input.bnr=0; input.sdi=4;
    assert(!a429_bnr_encode(input,&word));

    Frame src, packed, out, modified;
    sim_init(&src); src.tick_ms=100;
    src.samples[SID_airdata_sample]=(Sample){.v={70,2100,274},.time_ms=100,.valid=1};
    sim_service(&src,SID_airdata_words,&packed,NULL,NULL);
    assert(packed.samples[SID_airdata_words].valid);
    packed.tick_ms=120;
    sim_service(&packed,SID_airdata_decoded,&out,NULL,NULL);
    const Sample sample=out.samples[SID_airdata_decoded];
    assert(sample.valid && fabs(sample.v[0]-6889.75)<.1 && fabs(sample.v[1]-136.125)<.1 && sample.v[2]==20);

    modified=packed;
    modified.samples[SID_airdata_words].v[0]+=0.5;
    sim_service(&modified,SID_airdata_decoded,&out,NULL,NULL);
    assert(!out.samples[SID_airdata_decoded].valid);

    /* A freshly relayed packet must still carry a recent producer time. */
    modified=packed; modified.tick_ms=201; modified.samples[SID_airdata_words].time_ms=201;
    sim_service(&modified,SID_airdata_decoded,&out,NULL,NULL);
    assert(!out.samples[SID_airdata_decoded].valid);

    modified=packed;
    for (unsigned state=0;state<=2;state++) {
        input=(A429Bnr){.label=0xd2,.sdi=1,.bnr=0,.ssm=(enum A429Ssm)state};
        assert(a429_bnr_encode(input,&word));
        modified.samples[SID_airdata_words].v[0]=word;
        sim_service(&modified,SID_airdata_decoded,&out,NULL,NULL);
        assert(!out.samples[SID_airdata_decoded].valid);
    }
    input=(A429Bnr){.label=0xd2,.sdi=2,.bnr=0,.ssm=A429_NORMAL};
    assert(a429_bnr_encode(input,&word));
    modified.samples[SID_airdata_words].v[0]=word;
    sim_service(&modified,SID_airdata_decoded,&out,NULL,NULL);
    assert(!out.samples[SID_airdata_decoded].valid);

    src.samples[SID_airdata_sample].valid=0;
    sim_service(&src,SID_airdata_words,&packed,NULL,NULL);
    assert(packed.samples[SID_airdata_words].valid);
    assert(a429_bnr_decode((uint32_t)packed.samples[SID_airdata_words].v[0],&readback) && readback.ssm==A429_NO_DATA);
    sim_service(&packed,SID_airdata_decoded,&out,NULL,NULL);
    assert(!out.samples[SID_airdata_decoded].valid);
    puts("Air-data words: known bit pattern, parity, signed range, quantization, source, status and original age passed.");
    return 0;
}
