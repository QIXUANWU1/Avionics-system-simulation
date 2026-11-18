#ifndef IMA_MODEL_STEPS_H
#define IMA_MODEL_STEPS_H
#include "sim.h"

typedef struct {
    unsigned id, live, required;
    uint32_t tick_ms;
    const Sample *previous;
    const Sample *input[SIM_INPUTS];
} ModelStep;

enum LabMode { MODE_UNAVAILABLE, MODE_MANUAL, MODE_CENTER };
/* Only dimensionless, synthetic commands ever leave the control module. */
static inline double clip_between(double x,double lo,double hi) { return x<lo?lo:x>hi?hi:x; }
static inline double axis(double x) { return clip_between(x,-1,1); }
double isa_temperature(double elevation_m);
double airspeed_proxy_mps(double true_mps,double elevation_m);
int airdata_step(const ModelStep *,Sample *);
int flight_step(const ModelStep *,Sample *);
int systems_step(const ModelStep *,Sample *);
int information_step(const ModelStep *,Sample *);
#endif
