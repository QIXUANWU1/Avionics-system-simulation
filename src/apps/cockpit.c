#include "cockpit.h"
#include "core/cabin_fixture.h"
#include <math.h>
#include <stdlib.h>

typedef struct { FILE *f; const CockpitState *state; } Draw;
static const Sample *get(const Draw *d,ServiceId id)
{
    return sim_sample_fresh(&d->state->bus,id)?&d->state->bus.samples[id]:NULL;
}
static double bound(double v,double a,double b) { return v<a?a:v>b?b:v; }

static void box(FILE *f,int x,int y,int width,int height)
{
    fprintf(f,"<rect x='%d' y='%d' width='%d' height='%d' rx='2' class='box'/>\n",x,y,width,height);
}

static void text_value(FILE *f,int x,int y,const char *style,const char *format,const Sample *s,unsigned k)
{
    fprintf(f,"<text x='%d' y='%d' class='%s'>",x,y,style);
    if (s) fprintf(f,format,s->v[k]); else fputs("—",f);
    fputs("</text>\n",f);
}

static void attitude(Draw *d)
{
    FILE *f=d->f;
    const Sample *att=get(d,SID_primary_display),*height=get(d,SID_display_airdata);
    const Sample *nav=get(d,SID_track_rate),*mode=get(d,SID_flight_mode);
    box(f,24,99,731,579);
    fputs("<text x='47' y='135' class='label'>主飞行显示  PFD</text>",f);
    fputs("<rect x='296' y='157' width='188' height='42' rx='8' class='inset'/>",f);
    text_value(f,390,187,"reading middle","航迹 %03.0f°",nav,0);
    fputs("<circle cx='390' cy='380' r='169' stroke='#567081' stroke-width='2' fill='#080f0e'/>",f);
    if (att) {
        const double bank=bound(att->v[0],-60,60), pitch=bound(att->v[1],-30,30);
        fprintf(f,"<g transform='translate(390 380)'><g clip-path='url(#att)'><g transform='rotate(%g) translate(0 %g)'>",-bank,pitch*4);
        fputs("<rect x='-410' y='-450' width='820' height='450' fill='#356267'/>"
              "<rect x='-410' y='0' width='820' height='450' fill='#644b36'/>"
              "<path d='M-420 0 H420' stroke='white' stroke-width='2'/>",f);
        for (int mark=-20;mark<=20;mark+=5) {
            if (!mark) continue;
            const int y=-mark*4, half=mark%10==0?51:30;
            fprintf(f,"<path d='M%d %d H%d' stroke='white' stroke-width='1.5'/>",-half,y,half);
            if (mark%10==0) fprintf(f,"<text x='%d' y='%d' class='mark'>%d</text>",half+10,y+4,abs(mark));
        }
        fputs("</g></g></g><path d='M325 380h43l9 10h26l9-10h43 M390 365v21' "
              "fill='none' stroke='#ffe780' stroke-width='3'/>",f);
    } else {
        fputs("<text x='390' y='383' class='warning middle'>姿态数据无效</text>",f);
    }
    fputs("<path d='M383 218h14l-7 12z' fill='#d9eff7'/>",f);
    const int tape_x[2]={50,604};
    const Sample *inputs[2]={att,height};
    const unsigned slots[2]={2,0};
    const double scale[2]={10,100};
    for (unsigned i=0;i<2;i++) {
        const int x=tape_x[i];
        fprintf(f,"<text x='%d' y='252' class='sub'>%s</text>",x,i?"气压高度 / FT":"指示空速 / KT");
        fprintf(f,"<rect x='%d' y='266' width='124' height='228' rx='8' class='inset'/>",x);
        const Sample *s=inputs[i];
        if (s) {
            const double value=s->v[slots[i]];
            const double center=round(value/scale[i])*scale[i];
            for (int k=-2;k<=2;k++) {
                const double t=center+k*scale[i], y=380+(value-t)*37/scale[i];
                if (y<278||y>482||fabs(y-380)<23) continue;
                fprintf(f,"<path d='M%d %g h13' stroke='#7f9aa8'/><text x='%d' y='%g' class='tick'>%.0f</text>",
                        x+14,y,x+37,y+5,t);
            }
        }
        fprintf(f,"<rect x='%d' y='355' width='124' height='50' rx='6' fill='#22302b' stroke='#82ad79'/>",x);
        text_value(f,x+62,390,"tape middle","%.0f",s,slots[i]);
    }
    fputs("<path d='M50 567 H728' stroke='#46534b'/>"
          "<text x='52' y='609' class='sub'>垂直速度 / VSI</text>"
          "<text x='310' y='609' class='sub'>地速 / GS</text>"
          "<text x='536' y='609' class='sub'>台架模式</text>",f);
    text_value(f,52,644,"reading","%+.0f ft/min",height,1);
    Sample gs={0};
    if (nav) gs.v[0]=nav->v[2]*1.943844492440605;
    text_value(f,310,644,"reading","%.0f kt",nav?&gs:NULL,0);
    const char *name=!mode||mode->v[0]==0?"不可用":mode->v[0]==1?"人工操纵":"回中模式";
    fprintf(f,"<text x='536' y='644' class='%s'>%s</text>",!mode||mode->v[0]==0?"warning":"reading",name);
}

static void side(Draw *d)
{
    FILE *f=d->f;
    const Sample *engine=get(d,SID_engine_display), *endurance=get(d,SID_fuel_endurance);
    const Sample *elec=get(d,SID_electrical_display),*cabin=get(d,SID_cabin_pressure);
    const Sample *imu=get(d,SID_inertial_agreement), *shadow=get(d,SID_control_agreement);
    box(f,777,99,477,272);
    fputs("<text x='800' y='137' class='label'>动力与机电</text>",f);
    const char *labels[]={"动力指示","发动机温度","剩余油量","估计续航","直流母线","客舱压力高度"};
    const char *formats[]={"%.0f%%","%.0f °C","%.1f kg","%.0f min","%.1f V","%.0f ft"};
    const Sample *values[]={engine,engine,engine,endurance,elec,cabin};
    const unsigned slots[]={0,1,2,0,0,0};
    for (unsigned j=0;j<6;j++) {
        const int x=800+(int)(j%3)*146,y=170+(int)(j/3)*72;
        fprintf(f,"<text x='%d' y='%d' class='sub'>%s</text>",x,y,labels[j]);
        if (engine&&j<2) {
            Sample converted=*engine;
            converted.v[0]*=100;
            converted.v[1]-=273.15;
            text_value(f,x,y+31,"reading",formats[j],&converted,slots[j]);
        } else {
            text_value(f,x,y+31,"reading",formats[j],values[j],slots[j]);
        }
    }
    const char *iru=imu?(imu->v[2]==2?"双路有效":imu->v[2]==1?"单路工作":imu->v[2]==3?"两路分歧":"无数据"):"无数据";
    const char *compare=shadow&&shadow->v[2]==1?"一致":shadow&&shadow->v[2]==2?"不一致":"未配对";
    fprintf(f,"<path d='M800 297h430' stroke='#46534b'/><text x='800' y='335' class='%s'>惯性参考  %s</text>"
              "<text x='1021' y='335' class='%s'>影子计算  %s</text>",
              imu&&imu->v[2]==2?"status":"warning",iru,shadow&&shadow->v[2]==1?"status":"warning",compare);

    box(f,777,391,477,287);
    fputs("<text x='800' y='428' class='label'>客舱观察</text>",f);
    const Sample *camera=get(d,SID_camera_frame_source);
    fputs("<rect x='800' y='452' width='208' height='156' rx='4' class='inset'/>",f);
    const int camera_ok=camera&&camera->v[1]>=0&&camera->v[1]<=3&&
        camera->v[1]==floor(camera->v[1])&&camera->v[2]>=0&&camera->v[2]<=UINT32_MAX;
    if (camera_ok) {
        CabinFrame pic;
        cabin_fixture(&pic,(unsigned)camera->v[1],camera->v[0],(uint32_t)camera->v[2]);
        for (unsigned y=0;y<24;y++) for (unsigned x=0;x<32;x++) {
            const unsigned px=pic.pixels[(y*4+1)*CAMERA_WIDTH+x*4+1];
            fprintf(f,"<rect x='%g' y='%g' width='6.55' height='6.55' fill='#%02x%02x%02x'/>",
                    800+6.5*x,452+6.5*y,px,px,px);
        }
    }
    fputs("<text x='1028' y='475' class='sub'>摄像头替身</text>",f);
    fprintf(f,"<text x='1028' y='508' class='%s'>%s</text>",
            camera_ok?"status":"warning",camera_ok?"画面有效":"无数据");
    text_value(f,1028,538,"sub","照度 %.0f lx",camera,0);
    fputs("<text x='1028' y='579' class='sub'>气象旁路估计</text>",f);
    const Sample *w=&d->state->weather;
    const uint32_t now=d->state->bus.tick_ms;
    Sample fraction=*w;
    fraction.v[0]*=100;
    text_value(f,1028,607,"status","候选回波 %.0f%%",
               w->valid&&now>=w->time_ms&&now-w->time_ms<=1510?&fraction:NULL,0);
    fputs("<text x='800' y='647' class='sub'>合成画面 · 仅供观察</text>",f);
}

static void systems_strip(Draw *d)
{
    FILE *f=d->f;
    const Sample *speed=get(d,SID_airdata_mach),*temp=get(d,SID_outside_temperature_display);
    const Sample *gear=get(d,SID_gear_indication),*hyd=get(d,SID_hydraulic_reserve);
    const Sample *wx=get(d,SID_weather_display),*cool=get(d,SID_avionics_cooling);
    box(f,24,698,1230,102);
    const char *labels[]={"真空速估计","外界温度","起落架","液压压力","气象回波","设备冷却"};
    for (unsigned i=0;i<6;i++)
        fprintf(f,"<text x='%u' y='731' class='sub'>%s</text>",48u+201u*i,labels[i]);
    text_value(f,48,771,"reading","%.0f kt",speed,1);
    text_value(f,249,771,"reading","%+.1f °C",temp,0);
    fprintf(f,"<text x='450' y='771' class='reading'>%s</text>",
            !gear?"—":gear->v[0]==1?"放下":gear->v[0]==0?"收上":"转换中");
    text_value(f,651,771,"reading","%.0f bar",hyd,0);
    text_value(f,852,771,"reading","%.0f dBZ",wx,1);
    Sample temp_c={0};
    if (cool) temp_c.v[0]=cool->v[0]-273.15;
    text_value(f,1053,771,cool&&cool->v[1]==1&&cool->v[2]==0?"reading":"warning","%.0f °C",
               cool?&temp_c:NULL,0);
}

int cockpit_write_svg(FILE *f,const CockpitState *state)
{
    fputs("<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 1280 840' width='1280' height='840'>\n"
          "<title>固定翼航电综合显示</title>"
          "<defs><clipPath id='att'><circle cx='0' cy='0' r='168'/></clipPath></defs>"
          "<style>text{font-family:'Microsoft YaHei','Noto Sans CJK SC','DejaVu Sans',Arial,sans-serif;fill:#dde9dd}.title{font-size:26px;font-weight:700}"
          ".label{font-size:18px;font-weight:700;letter-spacing:1px}.sub{font-size:13px;letter-spacing:.7px;fill:#a3b2a7}"
          ".reading{font-size:25px;font-weight:700}.tape{font-size:29px;font-weight:700}.tick{font-size:15px}"
          ".mark{font-size:12px;fill:#fff}.middle{text-anchor:middle}.warning{font-size:15px;font-weight:700;fill:#ffcf7a}"
          ".status{font-size:15px;font-weight:700;fill:#9fe48b}.box{fill:#171d1c;stroke:#3a4741}"
          ".inset{fill:#090e0d;stroke:#46534b}</style>"
          "<rect width='1280' height='840' fill='#0b100f'/>"
          "<text x='24' y='43' class='title'>固定翼航电综合显示</text>"
          "<text x='24' y='68' class='sub'>软件台架  /  合成传感器输入</text>\n",f);
    fprintf(f,"<text x='1254' y='51' text-anchor='end' class='status'>%s · 台架时间  %.2f s</text>\n",
            state->live?"DDS 在线":state->demo?"离线演示":"记录回放",state->bus.tick_ms*.001);
    Draw d={f,state};
    const Sample *checks=get(&d,SID_system_cautions);
    const int normal=checks&&checks->v[0]==1&&checks->v[1]==1&&checks->v[2]==1;
    fprintf(f,"<text x='935' y='51' text-anchor='end' class='%s'>机电：%s</text>",
            normal?"status":"warning",!checks?"无数据":normal?"正常":"检查");
    attitude(&d); side(&d); systems_strip(&d);
    fputs("<text x='24' y='826' class='sub'>仪表只读  /  数据过期自动清除  /  训练台架显示</text></svg>\n",f);
    return !ferror(f);
}
