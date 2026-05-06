#include "jobgen.h"

static int rnd(int a,int b){ return a + rand() % (b - a + 1); }

void jobgen_fill(Queue* workload, const JobGenParams* p){
    if (!workload || !p || p->n <= 0) return;
    srand(p->seed ? p->seed : 42);

    int tid = 1;
    if (p->pattern == JG_STAGGERED) {
        int t = p->min_arrival;
        for (int i=0;i<p->n;i++,tid++){
            int burst = rnd(p->min_burst, p->max_burst);
            workload_add(workload, tid, t, burst);
            t += (p->gap > 0 ? p->gap : 1);
        }
        return;
    }

    if (p->pattern == JG_BURSTY) {
        int g = (p->group_size > 0 ? p->group_size : 3);
        int arrivals = p->max_arrival - p->min_arrival + 1;
        if (arrivals < 1) arrivals = 1;

        for (int i=0;i<p->n;i++){
            // pick a cluster center, then jitter a bit
            int cluster = rnd(p->min_arrival, p->max_arrival);
            int jitter  = rnd(0, (arrivals/10)+1);
            int at = cluster + (i%g==0 ? 0 : rnd(-jitter, +jitter));
            if (at < p->min_arrival) at = p->min_arrival;
            if (at > p->max_arrival) at = p->max_arrival;
            int burst = rnd(p->min_burst, p->max_burst);
            workload_add(workload, tid++, at, burst);
        }
        return;
    }

    // default: RANDOM
    for (int i=0;i<p->n;i++,tid++){
        int at = rnd(p->min_arrival, p->max_arrival);
        int burst = rnd(p->min_burst, p->max_burst);
        workload_add(workload, tid, at, burst);
    }
}