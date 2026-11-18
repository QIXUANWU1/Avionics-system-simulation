#include "advisory.h"
#include "genann.h"
#include <math.h>

static double clip(double x,double lo,double hi) { return x<lo?lo:x>hi?hi:x; }
static double clip_activation(const genann *model,double x)
{
    (void)model;
    return clip(x,0,1);
}

AdvisoryModel *ima_advisory_create(void)
{
    /* Hand-set integration fixture, not trained or calibrated. Genann's
       first weight in each row is multiplied by -1 for the bias term. */
    static const double hidden[ADVISORY_HIDDEN][ADVISORY_FEATURES+1]={
        {.17,.95,.12,.24,.09},{.17,.82,-.21,.27,.15},
        {.17,.71,.08,.16,.18},{.17,.85,.06,.14,.27},
        {.17,.76,.12,.26,.29},{.17,.65,-.12,.21,.31},
        {.17,.87,.09,.13,.25},{.17,.88,.07,.19,.21}
    };
    genann *model=genann_init(ADVISORY_FEATURES,1,ADVISORY_HIDDEN,1);
    if (!model) return NULL;
    model->activation_hidden=clip_activation;
    model->activation_output=genann_act_linear;
    unsigned at=0;
    for (unsigned j=0;j<ADVISORY_HIDDEN;j++)
        for (unsigned k=0;k<=ADVISORY_FEATURES;k++) model->weight[at++]=hidden[j][k];
    model->weight[at++]=0;
    for (unsigned j=0;j<ADVISORY_HIDDEN;j++) model->weight[at++]=1.0/ADVISORY_HIDDEN;
    return model;
}

void ima_advisory_destroy(AdvisoryModel *model) { genann_free(model); }

AdvisoryResult ima_advisory_step(AdvisoryModel *model,const Frame *f,AdvisoryWorkspace *work)
{
    const Sample *radar=&f->samples[ADVISORY_RADAR],*nav=&f->samples[ADVISORY_NAV];
    AdvisoryResult out={.radar_sequence=radar->sequence,.nav_sequence=nav->sequence};
    if (!sim_sample_fresh(f,ADVISORY_RADAR)||!sim_sample_fresh(f,ADVISORY_NAV)||radar->v[2]!=1.0)return out;
    const double range=radar->v[0], dbz=radar->v[1], alt=nav->v[0];
    if (range<1||range>400||dbz< -30||dbz>85||alt< -1000||alt>20000) return out;
    const float strength=(float)clip((dbz+10)/70,0,1);
    const float scan_range=(float)clip(range/120,0,1);
    const float altitude=(float)clip((alt+1000)/16000,0,1);
    for (unsigned i=0;i<ADVISORY_ROWS;i++) {
        const int x=(int)(i%128)-64,y=(int)(i/128)-64;
        const float edge=(float)(x*x+y*y)/8192.0f;
        const float texture=(float)(((unsigned)(x*x*3+y*y*7+x*y+64))&127u)/127.0f;
        work->rows[i][0]=(float)clip(strength-.28f*edge+.13f*texture,0,1);
        work->rows[i][1]=scan_range;
        work->rows[i][2]=altitude;
        work->rows[i][3]=texture;
    }
    unsigned count=0;
    for (unsigned i=0;i<ADVISORY_ROWS;i++) {
        const float *row=work->rows[i];
        const double x[ADVISORY_FEATURES]={row[0],row[1],row[2],row[3]};
        count+=(unsigned)(genann_run(model,x)[0]>.32);
    }
    out.score=(float)count/ADVISORY_ROWS;out.valid=1;
    return out;
}
