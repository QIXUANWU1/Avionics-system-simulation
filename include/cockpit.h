#ifndef IMA_COCKPIT_H
#define IMA_COCKPIT_H
#include "core/sim.h"
#include <stdio.h>

typedef struct { Frame bus; Sample weather; int live, demo; } CockpitState;
int cockpit_write_svg(FILE *,const CockpitState *);
#endif
