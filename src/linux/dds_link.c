#include "dds_link.h"
#include "ImalabState.h"
#include <dds/dds.h>
#include <stdio.h>
#include <stdlib.h>

enum { BATCH=32 };
struct ImaDdsLink {
    dds_entity_t domain, participant, reader, writer, display_writer;
    imalab_State cache[BATCH];
    unsigned size, cursor;
};

unsigned ima_dds_domain(unsigned base) { return 1+base%200; }

static int entity_ok(dds_entity_t value,const char *role)
{
    if (value>=0) return 1;
    fprintf(stderr,"DDS %s: %s\n",role,dds_strretcode(value));
    return 0;
}

ImaDdsLink *ima_dds_open(unsigned base,int display_reader,int display_writer)
{
    static const char config[]=
        "<CycloneDDS><Domain Id=\"any\"><General>"
        "<NetworkInterfaceAddress>lo</NetworkInterfaceAddress>"
        "<AllowMulticast>false</AllowMulticast></General><Discovery>"
        "<ParticipantIndex>auto</ParticipantIndex><MaxAutoParticipantIndex>31</MaxAutoParticipantIndex>"
        "<Peers><Peer Address=\"127.0.0.1\"/></Peers>"
        "</Discovery></Domain></CycloneDDS>";
    ImaDdsLink *link=calloc(1,sizeof *link);
    if (!link) return NULL;
    const dds_domainid_t id=ima_dds_domain(base);
    link->domain=dds_create_domain(id,config);
    if (!entity_ok(link->domain,"domain")) goto fail;
    link->participant=dds_create_participant(id,NULL,NULL);
    if (!entity_ok(link->participant,"participant")) goto fail;
    dds_qos_t *q=dds_create_qos();
    if (!q) goto fail;
    dds_qset_history(q,DDS_HISTORY_KEEP_LAST,1);
    dds_qset_resource_limits(q,96,64,1);
    dds_qset_reliability(q,DDS_RELIABILITY_BEST_EFFORT,0);
    dds_qset_durability(q,DDS_DURABILITY_VOLATILE);
    int ok=1;
    if (!display_reader) {
        const dds_entity_t topic=dds_create_topic(link->participant,&imalab_State_desc,"ImalabStateV5",NULL,NULL);
        ok=entity_ok(topic,"state topic");
        if (ok) {
            link->reader=dds_create_reader(link->participant,topic,q,NULL);
            dds_qset_lifespan(q,DDS_SECS(2));
            link->writer=dds_create_writer(link->participant,topic,q,NULL);
            ok=entity_ok(link->reader,"state reader") && entity_ok(link->writer,"state writer");
        }
    }
    if (ok&&(display_reader||display_writer)) {
        const dds_entity_t topic=dds_create_topic(link->participant,&imalab_State_desc,"ImalabDisplayV5",NULL,NULL);
        ok=entity_ok(topic,"display topic");
        if (ok&&display_reader) {
            link->reader=dds_create_reader(link->participant,topic,q,NULL);
            ok=entity_ok(link->reader,"display reader");
        } else if (ok) {
            link->display_writer=dds_create_writer(link->participant,topic,q,NULL);
            ok=entity_ok(link->display_writer,"display writer");
        }
    }
    dds_delete_qos(q);
    if (ok) return link;
fail:
    ima_dds_close(link);
    return NULL;
}

void ima_dds_close(ImaDdsLink *link)
{
    if (!link) return;
    if (link->domain>0) (void)dds_delete(link->domain);
    free(link);
}

static int write_state(dds_entity_t writer,unsigned source,const Message *m)
{
    imalab_State st={.service_id=m->sid,.source_domain=(uint16_t)source,
        .epoch=m->epoch,.sequence_no=m->sequence,.model_ms=m->model_ms,.quality=m->flags,.source_ns=m->source_ns};
    memcpy(st.value,m->v,sizeof st.value);
    return dds_write(writer,&st)==DDS_RETCODE_OK;
}

int ima_dds_write(ImaDdsLink *link,unsigned source,const Message *m)
{
    return write_state(link->writer,source,m);
}

int ima_dds_display_write(ImaDdsLink *link,unsigned source,const Message *m)
{
    return write_state(link->display_writer,source,m);
}

int ima_dds_read(ImaDdsLink *link,unsigned *source,Message *m)
{
    for (;;) {
        if (link->cursor<link->size) {
            const imalab_State *st=&link->cache[link->cursor++];
            *source=st->source_domain;
            *m=(Message){.kind=KIND_SAMPLE,.sid=st->service_id,.epoch=st->epoch,.sequence=st->sequence_no,
                         .model_ms=st->model_ms,.flags=st->quality,.source_ns=st->source_ns};
            memcpy(m->v,st->value,sizeof m->v);
            return 1;
        }
        void *buffers[BATCH];
        dds_sample_info_t info[BATCH];
        imalab_State rows[BATCH];
        for (unsigned i=0;i<BATCH;i++) buffers[i]=&rows[i];
        const dds_return_t n=dds_take(link->reader,buffers,info,BATCH,BATCH);
        if (n<=0) return n<0?-1:0;
        link->cursor=link->size=0;
        for (dds_return_t i=0;i<n;i++)
            if (info[i].valid_data) link->cache[link->size++]=rows[i];
        if (!link->size) return 0;
    }
}
