#include "sim.h"
#include "dispatch.h"
#include "cpu.h"
#include "jobgen.h"

// keep these consistent with plot script
#define MAX_TICKS 10000

/* helpers duplicated from sim.c in a local form */
static Thread* make_thread_dummy(void){ return NULL; } /* just to hint separate TU */

static void collect_completions(CPU* cpu, Queue* finished) {
    for (int i = 0; i < cpu->ncores; ++i) {
        Thread* t = cpu->core[i];
        if (!t) continue;
        if (t->remaining == 0) {
            (void)cpu_unbind_core(cpu, i);
            t->state = ST_FINISHED;
            if (t->finish_time < 0) t->finish_time = SIM_TIME;
            q_push(finished, t);
        }
    }
}

static int all_done(const Queue* ready, const Queue* waiting, const CPU* cpu) {
    if (!q_empty((Queue*)ready))   return 0;
    if (!q_empty((Queue*)waiting)) return 0;
    for (int i = 0; i < cpu->ncores; ++i)
        if (cpu->core[i]) return 0;
    return 1;
}

static int rnd(int a, int b) { return a + rand() % (b - a + 1); }

static void random_interrupts(const InterruptConfig* cfg, CPU* cpu, Queue* waiting, Log* log)  {
    if (!cfg || !cfg->enable_random) return;
    for (int c = 0; c < cpu->ncores; ++c) {
        Thread* t = cpu->core[c];
        if (!t) continue;
        int r = rand() % 100;
        if (r < cfg->pct_io) {
            int dur = rnd(cfg->io_min, cfg->io_max);
            int unblock = SIM_TIME + dur;
            block_to_waiting(cpu, c, waiting, unblock);
            log_io_event(log, SIM_TIME, c, t->tid, dur, unblock);
        }
    }
}

/* tiny input helpers */
static int read_int(const char *p){
    int x; for(;;){
        printf("%s", p);
        if (scanf("%d",&x)==1) return x;
        int c; while((c=getchar())!='\n' && c!=EOF){}; puts("Invalid input. Try again.");
    }
}
static int read_01(const char *p){ int v; do{ v=read_int(p); }while(v!=0 && v!=1); return v; }

int main(void){
    puts("\n--- Simulator + Job Generator ---");
    // jobgen params
    JobGenParams jp = {0};
    jp.n           = read_int("Number of jobs: ");
    jp.min_arrival = read_int("Min arrival time: ");
    jp.max_arrival = read_int("Max arrival time: ");
    jp.min_burst   = read_int("Min burst length: ");
    jp.max_burst   = read_int("Max burst length: ");

    puts("Pattern: 0=Random, 1=Staggered, 2=Bursty");
    int pat        = read_int("Pattern: ");
    jp.pattern     = (pat==1)?JG_STAGGERED:(pat==2)?JG_BURSTY:JG_RANDOM;

    if (jp.pattern==JG_STAGGERED) jp.gap = read_int("Stagger gap (ticks): ");
    if (jp.pattern==JG_BURSTY)    jp.group_size = read_int("Bursty group size: ");
    jp.seed = (unsigned)read_int("RNG seed (e.g., 42): ");

    // sim params
    int ncores = read_int("Number of cores: ");
    // NOTE: simulator supports FIFO today
    puts("Scheduler: FIFO (only)"); // future: extend dispatch to SJF/RR/Priority

    InterruptConfig intr = {0};
    intr.enable_random = read_01("Enable random I/O interrupts? (0/1): ");
    if (intr.enable_random) {
        intr.pct_io = read_int("  IO probability (%): ");
        intr.io_min = read_int("  IO min duration: ");
        intr.io_max = read_int("  IO max duration: ");
    }
    srand( (unsigned)jp.seed );

    // build workload
    Queue workload; workload_init(&workload);
    jobgen_fill(&workload, &jp);

    // queues
    Queue ready, waiting, finished;
    q_init(&ready); q_init(&waiting); q_init(&finished);

    // cpu
    CPU cpu; cpu_init(&cpu, ncores);
    int **run_trace = (int**)malloc(sizeof(int*) * ncores);
    for (int c = 0; c < ncores; ++c) {
        run_trace[c] = (int*)malloc(sizeof(int) * MAX_TICKS);
        for (int t = 0; t < MAX_TICKS; ++t) run_trace[c][t] = -1;
    }
    cpu.run_trace = run_trace;
    cpu.trace_len = MAX_TICKS;

    // logging
    Log log;
    if (log_open(&log, "sim_log.txt") != 0) {
        fprintf(stderr, "cannot open sim_log.txt\n");
        return 1;
    }
    log_set_multiline(&log, 1);
    log_workload(&log, "Workload before simulation", &workload);
    log_interrupts_config(&log, intr.enable_random, intr.pct_io, intr.io_min, intr.io_max);

    // start
    SIM_TIME = 0;
    workload_admit_tick(&workload, &ready, SIM_TIME);

    DispatchFn schedule = dispatch_get(DISP_FIFO);

    // main loop
    for (;;) {
        workload_admit_tick(&workload, &ready, SIM_TIME);
        waiting_io_resolve(&waiting, &ready, SIM_TIME);
        random_interrupts(&intr, &cpu, &waiting, &log);
        schedule(&cpu, &ready);
        bump_queue_wait(&ready);
        log_snapshot(&log, SIM_TIME, &ready, &waiting, &cpu, &finished);
        cpu_step(&cpu);
        collect_completions(&cpu, &finished);
        if (all_done(&ready, &waiting, &cpu)) break;
    }

    // tail
    log_snapshot(&log, SIM_TIME, &ready, &waiting, &cpu, &finished);
    log_final_averages(&log, &finished);
    log_close(&log);

    if (write_core_trace_default(&cpu) == 0)
        puts("Wrote per-core trace to core_trace.txt");
    else
        puts("Failed to write per-core trace");

    while (!q_empty(&finished)) free(q_pop(&finished));
    for (int c = 0; c < ncores; ++c) free(run_trace[c]);
    free(run_trace);
    free(cpu.core);
    return 0;
}