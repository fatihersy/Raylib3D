
#ifndef FTIME_H
#define FTIME_H

#include "defines.h"

#define RANDOM_TABLE_NUMBER_COUNT 507

bool time_system_initialize(void);

void update_time(void);

i32 get_random(i32 min, i32 max);

#endif
