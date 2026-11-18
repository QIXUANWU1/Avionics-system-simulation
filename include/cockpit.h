#ifndef IMA_COCKPIT_H
#define IMA_COCKPIT_H
#include "core/sim.h"
#include <stdio.h>

typedef struct { Frame bus; Sample weather; int live, demo; } CockpitState;
int cockpit_write_svg(FILE *,const CockpitState *);
void cockpit_navigation(FILE *,const CockpitState *);
void cockpit_aircraft_systems(FILE *,const CockpitState *);
#endif
