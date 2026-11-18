#ifndef IMA_ARINC429_WORD_H
#define IMA_ARINC429_WORD_H
#include <stdint.h>

/* Bit 1 is bit 0 of the software integer. Host byte order is irrelevant:
   call sites carry the numeric word, without a physical transmitter. */
enum A429Ssm { A429_FAILURE, A429_NO_DATA, A429_TEST, A429_NORMAL };
typedef struct { uint8_t label, sdi; int32_t bnr; enum A429Ssm ssm; } A429Bnr;
int a429_bnr_encode(A429Bnr value,uint32_t *word);
int a429_bnr_decode(uint32_t word,A429Bnr *value);
#endif
