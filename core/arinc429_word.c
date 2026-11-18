#include "arinc429_word.h"

static uint8_t reverse_label(uint8_t x)
{
    x=(uint8_t)(((x&0xaau)>>1)|((x&0x55u)<<1));
    x=(uint8_t)(((x&0xccu)>>2)|((x&0x33u)<<2));
    return (uint8_t)((x>>4)|(x<<4));
}

static unsigned odd(uint32_t n)
{
    n^=n>>16; n^=n>>8; n^=n>>4; n^=n>>2; n^=n>>1;
    return n&1u;
}

int a429_bnr_encode(A429Bnr v,uint32_t *word)
{
    if (!word || v.sdi>3 || (unsigned)v.ssm>A429_NORMAL || v.bnr< -262144 || v.bnr>262143) return 0;
    uint32_t bits=(uint32_t)reverse_label(v.label)|((uint32_t)v.sdi<<8)|
                  (((uint32_t)v.bnr&UINT32_C(0x7ffff))<<10)|((uint32_t)v.ssm<<29);
    *word=bits|((uint32_t)(odd(bits)^1u)<<31);
    return 1;
}

int a429_bnr_decode(uint32_t word,A429Bnr *v)
{
    if (!v || !odd(word)) return 0;
    const uint32_t raw=(word>>10)&UINT32_C(0x7ffff);
    *v=(A429Bnr){.label=reverse_label((uint8_t)word),.sdi=(uint8_t)((word>>8)&3u),
                 .bnr=(int32_t)raw-(raw&UINT32_C(0x40000)?524288:0),
                 .ssm=(enum A429Ssm)((word>>29)&3u)};
    return 1;
}
