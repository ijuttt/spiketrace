#ifndef CPU_H
#define CPU_H

#include "spkt_common.h"

spkt_status_t get_curr_stats(struct curr_stat *curr, int max_cores);

spkt_status_t cpu_calc_usage_delta_batch(const struct curr_stat *prev,
                                         const struct curr_stat *curr,
                                         int num_cores, double *out_usage);

#endif
