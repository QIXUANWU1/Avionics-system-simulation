#ifndef IMA_DDS_LINK_H
#define IMA_DDS_LINK_H
#include "wire.h"

typedef struct ImaDdsLink ImaDdsLink;
ImaDdsLink *ima_dds_open(unsigned base,int display_reader,int display_writer);
void ima_dds_close(ImaDdsLink *);
int ima_dds_write(ImaDdsLink *,unsigned source,const Message *);
int ima_dds_display_write(ImaDdsLink *,unsigned source,const Message *);
/* 1 has data, 0 no data, -1 local DDS error. */
int ima_dds_read(ImaDdsLink *,unsigned *source,Message *);
unsigned ima_dds_domain(unsigned base);
#endif
