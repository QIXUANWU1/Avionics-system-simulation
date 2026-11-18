#include "cockpit.h"
#include "core/navigation.h"
#include "core/model_steps.h"
#include <math.h>

static const Sample *fresh(const CockpitState *state, ServiceId id)
{
    return sim_sample_fresh(&state->bus, id) ? &state->bus.samples[id] : NULL;
}

static void value(FILE *out, int x, int y, const char *label, const Sample *sample,
                  unsigned field, double scale, double offset, const char *format)
{
    fprintf(out, "<text x='%d' y='%d' class='sub'>%s</text>"
                 "<text x='%d' y='%d' class='number'>", x, y, label, x, y+32);
    if (sample) fprintf(out, format, sample->v[field] * scale + offset);
    else fputs("—", out);
    fputs("</text>", out);
}

static void indicator(FILE *out, int x, int y, const char *name, const char *reading, int normal)
{
    fprintf(out, "<text x='%d' y='%d' class='sub'>%s</text>"
                 "<text x='%d' y='%d' class='%s'>%s</text>",
            x, y, name, x, y+29, normal ? "reading" : "warning", reading);
}

void cockpit_navigation(FILE *out, const CockpitState *state)
{
    const Sample *fix = fresh(state, SID_gnss_position);
    const Sample *route = fresh(state, SID_route_indication);
    const Sample *track = fresh(state, SID_track_rate);
    const Sample *air = fresh(state, SID_airdata_crosscheck);

    fputs("<rect x='24' y='100' width='805' height='700' class='box'/>"
          "<rect x='850' y='100' width='406' height='700' class='box'/>"
          "<text x='48' y='137' class='label'>平面导航 · 北向上</text>"
          "<text x='874' y='137' class='label'>当前航段 · WP01</text>"
          "<rect x='45' y='153' width='764' height='605' class='inset'/>"
          "<defs><clipPath id='nav-map' clipPathUnits='userSpaceOnUse'>"
          "<rect x='45' y='153' width='764' height='605'/></clipPath></defs>"
          "<g clip-path='url(#nav-map)'>", out);
    const int cx = 427, cy = 455;
    const double pixels_per_m = 153.0 / LAB_METRES_PER_NM;

    if (fix && fix->v[2] > 0 && fix->v[2] <= 30) {
        const double origin_x = cx - fix->v[0] * pixels_per_m;
        const double origin_y = cy + fix->v[1] * pixels_per_m;
        const double target_x = origin_x + LAB_WAYPOINT_E_M * pixels_per_m;
        const double target_y = origin_y - LAB_WAYPOINT_N_M * pixels_per_m;
        for (int i=-2; i<=2; i++) {
            const int x=cx+153*i, y=cy+153*i;
            fprintf(out, "<path d='M%d 153v605 M45 %dh764' stroke='#28382f'/>", x, y);
        }
        fprintf(out, "<path d='M%.2f %.2f L%.2f %.2f' stroke='#ba8fde' stroke-width='3' fill='none'/>"
                     "<circle cx='%.2f' cy='%.2f' r='4' fill='#ba8fde'/>"
                     "<path d='M%.2f %.2f l7 7 -7 7 -7 -7 z' fill='none' stroke='#dcb6ef' stroke-width='2'/>"
                     "<text x='%.2f' y='%.2f' class='status'>WP01</text>",
                origin_x, origin_y, target_x, target_y, origin_x, origin_y,
                target_x, target_y-7, target_x-18, target_y-19);
        if (track) {
            fprintf(out, "<g transform='translate(%d %d) rotate(%.2f)'>"
                         "<path d='M0 -20v-67' stroke='#eee9cf' stroke-dasharray='6 5'/>"
                         "<path d='M0 -17 L13 12 L0 6 L-13 12 Z' fill='#eee9cf'/></g>",
                    cx, cy, track->v[0]);
        } else {
            fprintf(out, "<circle cx='%d' cy='%d' r='6' fill='#eee9cf'/>", cx, cy);
        }
    } else {
        fputs("<text x='427' y='455' class='warning middle'>导航位置无效</text>", out);
    }
    fputs("</g><path d='M70 215v-28l-5 9m5-9l5 9 M55 737h153 m-153-5v10 m153-10v10' "
          "stroke='#c1d2c3' fill='none'/>"
          "<text x='82' y='199' class='sub'>北</text>"
          "<text x='214' y='742' class='sub'>1 NM</text>"
          "<text x='48' y='785' class='sub'>当地平面坐标 · 紫线为预置参考航段</text>", out);

    value(out, 874, 187, "航点距离", route, 0, 1, 0, "%.2f NM");
    value(out, 1064, 187, "航点方位", route, 1, 1, 0, "%03.0f°");
    value(out, 874, 282, "当前地面航迹", track, 0, 1, 0, "%03.0f°");
    value(out, 1064, 282, "地速", track, 2, 1.943844492, 0, "%.0f kt");
    value(out, 874, 377, "横向偏差（右为正）", route, 2, 1, 0, "%+.2f NM");
    fputs("<path d='M882 444h338 M882 437v14 M966 437v14 M1051 435v18"
          " M1136 437v14 M1220 437v14' stroke='#64796b'/>", out);
    if (route) {
        const double x = 1051+338*clip_between(route->v[2], -.5, .5);
        fprintf(out, "<path d='M%.1f 436l8 8-8 8-8-8z' fill='#ba8fde'/>", x);
    }
    fputs("<text x='874' y='477' class='sub'>左 0.5 NM</text>"
          "<text x='1221' y='477' text-anchor='end' class='sub'>右 0.5 NM</text>"
          "<path d='M874 511h350' stroke='#3a4741'/>", out);
    value(out, 874, 555, "GNSS 东坐标", fix, 0, 1, 0, "%.0f m");
    value(out, 1064, 555, "GNSS 北坐标", fix, 1, 1, 0, "%.0f m");
    value(out, 874, 643, "水平误差估计", fix, 2, 1, 0, "%.0f m");
    value(out, 1064, 643, "高度源差", air, 0, 1, 0, "%.0f ft");
    fputs("<text x='874' y='760' class='sub'>来源  GNSS 替身 / 航段解算 / 大气数据比对</text>", out);
}

void cockpit_aircraft_systems(FILE *out, const CockpitState *state)
{
    const Sample *engine = fresh(state, SID_engine_display);
    const Sample *fuel = fresh(state, SID_fuel_endurance);
    const Sample *electric = fresh(state, SID_electrical_display);
    const Sample *gear = fresh(state, SID_gear_indication);
    const Sample *travel = fresh(state, SID_gear_position_fixture);
    const Sample *hydraulic = fresh(state, SID_hydraulic_reserve);
    const Sample *brake = fresh(state, SID_brake_indication);
    const Sample *cabin = fresh(state, SID_cabin_pressure);
    const Sample *vent = fresh(state, SID_cabin_indication);
    const Sample *radio = fresh(state, SID_radio_indication);
    const Sample *crew = fresh(state, SID_crew_indication);
    const Sample *checks = fresh(state, SID_system_cautions);

    const int columns[] = {24, 442, 860};
    const char *titles[] = {"动力与燃油", "供电与液压", "起落架与客舱"};
    for (unsigned i=0; i<3; ++i) {
        fprintf(out, "<rect x='%d' y='100' width='396' height='570' class='box'/>"
                     "<text x='%d' y='140' class='label'>%s</text>",
                columns[i], columns[i]+25, titles[i]);
    }
    value(out, 49, 187, "动力指示", engine, 0, 100, 0, "%.0f%%");
    value(out, 49, 267, "发动机温度", engine, 1, 1, -273.15, "%.0f °C");
    value(out, 49, 347, "剩余燃油", engine, 2, 1, 0, "%.1f kg");
    value(out, 49, 427, "燃油流量", fuel, 2, 1, 0, "%.1f kg/h");
    value(out, 49, 507, "估计续航", fuel, 0, 1, 0, "%.0f min");
    value(out, 467, 187, "直流母线电压", electric, 0, 1, 0, "%.1f V");
    value(out, 467, 267, "总线电流", electric, 1, 1, 0, "%.1f A");
    const char *source = !electric ? "无数据" : electric->v[2]==2 ? "发电机" :
                         electric->v[2]==1 ? "电池" : "断开";
    indicator(out, 467, 347, "供电来源", source, electric && electric->v[2] > 0);
    value(out, 467, 427, "液压压力", hydraulic, 0, 1, 0, "%.0f bar");
    indicator(out, 467, 507, "无线电供电指示",
              !radio?"无数据":radio->v[0]==1?"可用":"不可用", radio && radio->v[0]==1);
    const char *gear_state = !gear?"无数据":gear->v[0]==1?"放下":gear->v[0]==0?"收上":"转换中";
    indicator(out, 885, 187, "起落架位置", gear_state, gear && gear->v[0]!=2);
    value(out, 1080, 187, "机构行程", travel, 0, 100, 0, "%.0f%%");
    const char *brake_state = !brake ? "无数据" :
                              brake->v[1]!=1 ? "空中" :
                              brake->v[2]!=1 ? "液压数据缺失" :
                              brake->v[0]==1 ? "可用" : "不可用";
    indicator(out, 885, 267, "地面刹车状态", brake_state,
              brake && (brake->v[1]!=1 || brake->v[0]==1));
    value(out, 885, 347, "客舱压力高度", cabin, 0, 1, 0, "%.0f ft");
    value(out, 1080, 347, "客舱压差", cabin, 1, 1, 0, "%.1f kPa");
    value(out, 885, 427, "客舱温度", vent, 0, 1, -273.15, "%.1f °C");
    indicator(out, 885, 507, "通风", !vent?"无数据":vent->v[1]==1?"运行":"停运", vent&&vent->v[1]==1);

    fputs("<rect x='24' y='692' width='1232' height='108' class='box'/>"
          "<text x='49' y='729' class='label'>系统状态</text>", out);
    value(out, 225, 726, "座舱汇总告警数", crew, 1, 1, 0, "%.0f");
    const char *names[] = {"舱压", "液压", "冷却"};
    for (unsigned i=0; i<3; ++i) {
        const int normal = checks && checks->v[i]==1;
        indicator(out, 435+(int)i*250, 726, names[i], !checks?"无数据":normal?"正常":"检查", normal);
    }
}
