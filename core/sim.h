#ifndef IMA_CIVIL_WEATHER_SIM_H
#define IMA_CIVIL_WEATHER_SIM_H
#include <stdint.h>
#define SIM_SERVICES 10
#define SIM_GROUPS 5
#define SIM_INPUTS 3
#define SIM_STEP_MS 10

typedef struct { double v[3]; uint32_t time_ms, sequence; int valid; } Sample;
typedef struct { const char *key, *name, *units; int group, period_ms, ninputs; int inputs[SIM_INPUTS]; } Service;
typedef struct { Sample samples[SIM_SERVICES]; uint32_t tick_ms; } Frame;
typedef struct { int producer, consumer, status; uint32_t source_ms, consumer_ms, sequence; } ReadEvent;
typedef void (*ReadObserver)(const ReadEvent *, void *);
extern const Service sim_services[SIM_SERVICES];

void sim_init(Frame *);
int sim_sample_fresh(const Frame *, unsigned producer);
/* in and out must be distinct; every service reads the supplied snapshot. */
void sim_service(const Frame *in, int service, Frame *out, ReadObserver, void *);
int sim_validate_catalog(void);
#endif
