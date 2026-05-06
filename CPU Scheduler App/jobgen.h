#ifndef JOBGEN_H
#define JOBGEN_H

#include "sim.h"  // for Queue, workload_add

typedef enum {
    JG_RANDOM = 0,
    JG_STAGGERED = 1,
    JG_BURSTY = 2
} JobPattern;

typedef struct {
    int n;              // number of jobs
    int min_arrival;    // inclusive
    int max_arrival;    // inclusive
    int min_burst;      // inclusive
    int max_burst;      // inclusive
    JobPattern pattern; // pattern type
    int gap;            // for STAGGERED (arrival gap)
    int group_size;     // for BURSTY (cluster size)
    unsigned seed;      // RNG seed
} JobGenParams;

void jobgen_fill(Queue* workload, const JobGenParams* p);

#endif