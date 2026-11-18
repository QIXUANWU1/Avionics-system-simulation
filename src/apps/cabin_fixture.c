#include "core/cabin_fixture.h"
#include <math.h>

static uint32_t noise(uint32_t *s)
{
    *s^=*s<<13; *s^=*s>>17; *s^=*s<<5;
    return *s;
}
static int byte(int x) { return x<0?0:x>255?255:x; }

void cabin_fixture(CabinFrame *f,unsigned scene,double lux,uint32_t seed)
{
    uint32_t state=seed?seed:0x19286affu;
    /* A generic cabin corridor in perspective; no camera or real image data. */
    const int illumination=(int)fmin(fmax(lux,0),400), center=64;
    for (int y=0;y<CAMERA_HEIGHT;y++) {
        for (int x=0;x<CAMERA_WIDTH;x++) {
            const int dx=x>center?x-center:center-x;
            const int floor_edge=7+(y>43?(y-43)*3/5:0);
            const int ceiling_edge=7+(y<43?(43-y)*2/3:0);
            const int grain=(int)(noise(&state)%11u)-5;
            int p=47;
            if (y>=43 && dx<floor_edge) p=76+(y-42)/3;
            else if (y<43 && dx<ceiling_edge) p=118-(43-y)/2+(dx<3?27:0);
            else if (y>=31) {
                const int rib=(dx-(y>=43?floor_edge:ceiling_edge))/(7+y/15);
                p=(rib%3==0?34:61)+(y>72?9:0);
                if (y<52&&y>36&&rib%3==2) p=115;
            }
            if (y>33&&y<51&&dx<7) p=151;
            p=p*illumination/245+grain;
            if (scene==CABIN_DIM) p=(p*illumination)/250;
            if (scene==CABIN_OCCLUDED && x>24 && x<105 && y>18 && y<79) p=8+grain/3;
            if (scene==CABIN_GLARE && x>18 && x<109 && y>13 && y<81) p=249+grain/2;
            f->pixels[y*CAMERA_WIDTH+x]=(uint8_t)byte(p);
        }
    }
}
