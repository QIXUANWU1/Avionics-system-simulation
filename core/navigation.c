#include "model_steps.h"
#include "navigation.h"
#include <math.h>

static const double radians_per_degree = 0.017453292519943295;

int navigation_step(const ModelStep *step, Sample *out)
{
    const Sample *input = step->input[0];
    const Sample *previous = step->previous;
    switch (step->id) {
    case SID_position_fixture: {
        if (previous) {
            out->v[0] = previous->v[0];
            out->v[1] = previous->v[1];
        }
        if (!input) return 0;
        const double speed = input->v[1];
        const double track = input->v[2];
        if (speed < 0 || speed > 160 || track < 0 || track >= 360) return 0;
        if (previous && previous->valid && step->tick_ms>=previous->time_ms &&
            step->tick_ms-previous->time_ms<=70) {
            const double elapsed = (step->tick_ms - previous->time_ms) * .001;
            out->v[0] += elapsed * speed * sin(track * radians_per_degree);
            out->v[1] += elapsed * speed * cos(track * radians_per_degree);
        }
        out->v[2] = track;
        return 1;
    }
    case SID_gnss_position:
        if (!input) return 0;
        out->v[0] = round(input->v[0] + 2);
        out->v[1] = round(input->v[1] - 1);
        out->v[2] = 6; /* synthetic receiver estimate, independent of assurance claims */
        return 1;
    case SID_route_indication: {
        if (!input || input->v[2] <= 0 || input->v[2] > 30) return 0;
        const double east = input->v[0], north = input->v[1];
        const double de = LAB_WAYPOINT_E_M - east, dn = LAB_WAYPOINT_N_M - north;
        const double leg = hypot(LAB_WAYPOINT_E_M, LAB_WAYPOINT_N_M);
        out->v[0] = hypot(de, dn) / LAB_METRES_PER_NM;
        out->v[1] = fmod(atan2(de, dn) / radians_per_degree + 360, 360);
        /* Signed lateral offset, positive to the right along the bench route. */
        out->v[2] = (east * LAB_WAYPOINT_N_M - north * LAB_WAYPOINT_E_M) / (leg * LAB_METRES_PER_NM);
        return 1;
    }
    default:
        return 0;
    }
}
