#ifndef CPU_H
#define CPU_H

#include "spkt_common.h"

spkt_status_t get_curr_stats(struct curr_stat *curr, int max_cores);

#endif
