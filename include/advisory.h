#ifndef IMA_WEATHER_ADVISORY_H
#define IMA_WEATHER_ADVISORY_H
#include "core/sim.h"

#define ADVISORY_SID 10u
#define ADVISORY_RADAR 4u
#define ADVISORY_NAV 3u
#define ADVISORY_PERIOD_MS 500u
#define ADVISORY_ROWS 16384u
#define ADVISORY_FEATURES 4u
#define ADVISORY_HIDDEN 8u

typedef struct genann AdvisoryModel;
/* One synthetic 128 x 128 scan, four features per cell. */
typedef struct { float rows[ADVISORY_ROWS][ADVISORY_FEATURES]; } AdvisoryWorkspace;
typedef struct {
    uint32_t radar_sequence, nav_sequence;
    float score;
    unsigned valid;
} AdvisoryResult;

/* One model per worker; genann_run updates internal scratch. */
AdvisoryModel *ima_advisory_create(void);
void ima_advisory_destroy(AdvisoryModel *model);
AdvisoryResult ima_advisory_step(AdvisoryModel *model,const Frame *snapshot,AdvisoryWorkspace *work);
#endif
