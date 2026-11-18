#ifndef IMA_DISPLAY_FEED_H
#define IMA_DISPLAY_FEED_H
#include "core/sim.h"

static inline int ima_display_service(unsigned id)
{
    switch (id) {
    case SID_primary_display:
    case SID_display_airdata:
    case SID_track_rate:
    case SID_flight_mode:
    case SID_engine_display:
    case SID_fuel_endurance:
    case SID_electrical_display:
    case SID_cabin_pressure:
    case SID_inertial_agreement:
    case SID_control_agreement:
    case SID_camera_frame_source:
    case SID_airdata_mach:
    case SID_outside_temperature_display:
    case SID_gear_indication:
    case SID_hydraulic_reserve:
    case SID_weather_display:
    case SID_avionics_cooling:
    case SID_system_cautions:
        return 1;
    default:
        return 0;
    }
}
#endif
