#ifndef IMA_RESEARCH_WIRE_H
#define IMA_RESEARCH_WIRE_H
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <float.h>
_Static_assert(sizeof(double)==8 && DBL_MANT_DIG==53 && DBL_MAX_EXP==1024, "wire requires IEEE754 binary64");
#define WIRE_SIZE 56
#define WIRE_VERSION 5u
#define KIND_SAMPLE 1
#define KIND_PROGRESS 2
#define KIND_CONSUME 3
#define KIND_RECEIVE 4
#define KIND_ADVISORY 5
typedef struct {
    uint8_t kind;
    uint16_t sid;
    uint32_t epoch,sequence,model_ms,flags;
    uint64_t source_ns;
    double v[3];
} Message;
static inline void le32_put(uint8_t*p,uint32_t v){for(int i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static inline uint32_t le32_get(const uint8_t*p){uint32_t v=0;for(int i=0;i<4;i++)v|=(uint32_t)p[i]<<(8*i);return v;}
static inline void le64_put(uint8_t*p,uint64_t v){for(int i=0;i<8;i++)p[i]=(uint8_t)(v>>(8*i));}
static inline uint64_t le64_get(const uint8_t*p){uint64_t v=0;for(int i=0;i<8;i++)v|=(uint64_t)p[i]<<(8*i);return v;}
static inline void wire_encode(uint8_t*p,const Message*m){
    memcpy(p,"IMAR",4);p[4]=WIRE_VERSION;p[5]=m->kind;p[6]=m->sid&255;p[7]=m->sid>>8;
    le32_put(p+8,m->epoch);le32_put(p+12,m->sequence);le32_put(p+16,m->model_ms);le32_put(p+20,m->flags);le64_put(p+24,m->source_ns);
    for(int i=0;i<3;i++){uint64_t u;memcpy(&u,&m->v[i],8);le64_put(p+32+8*i,u);}
}
static inline int wire_decode(const uint8_t*p,int n,Message*m){
    if(n!=WIRE_SIZE || memcmp(p,"IMAR",4) || p[4]!=WIRE_VERSION || p[5]<1 || p[5]>KIND_ADVISORY)return -1;
    m->kind=p[5];m->sid=p[6]|(uint16_t)p[7]<<8;m->epoch=le32_get(p+8);m->sequence=le32_get(p+12);m->model_ms=le32_get(p+16);m->flags=le32_get(p+20);m->source_ns=le64_get(p+24);
    for(int i=0;i<3;i++){uint64_t u=le64_get(p+32+8*i);memcpy(&m->v[i],&u,8);if(!isfinite(m->v[i]))return -1;}
    return 0;
}
#endif
