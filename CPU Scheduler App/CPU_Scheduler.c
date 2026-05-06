// CPU_Scheduler.c
// Schedulers: FCFS(FIFO), SJF (non-preemptive, shortest next CPU burst),
// Priority (non-preemptive, smaller number = higher priority), Round Robin (preemptive).
// NEW: Per-task CPU/I-O loops (e.g., 10 loops, 2ms CPU, 8ms I/O).
// I/O between loops blocks the task (not the CPU). Optional trailing I/O after final loop
// is included only in the reported "End (incl. IO)" and turnaround, not in CPU blocking.
//
// Build (MSVC): cl /TC CPU_Scheduler.c /Fe:cpu_sched.exe
// Build (MinGW): gcc -std=c11 -O2 -mconsole CPU_Scheduler.c -o cpu_sched.exe

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int id;                 // 1-based ID
    int arrival;            // initial arrival time
    int priority;           // smaller = higher priority (used in Priority scheduler)

    // Workload model
    // If loops == 0 => single-burst mode: CPU = cpu, post-CPU I/O = io (optional)
    // If loops > 0  => loops of [CPU 'cpu' then I/O 'io'] with I/O between loops;
    //                  optional trailing I/O after last CPU doesn't block CPU, just affects reported end.
    int loops;              // number of (CPU, I/O) loops; 0 => use single-burst mode
    int cpu;                // CPU time per loop (or single-burst time when loops==0)
    int io;                 // I/O time per loop (or single post-CPU I/O when loops==0)
    int include_last_io;    // 0/1: add io after last loop to reported End/Turnaround (does not block CPU)

    // Runtime state
    int cur_loop;           // which loop we’re on (0-based)
    int remaining_cpu;      // remaining CPU in current loop’s CPU burst
    int next_ready_time;    // when the task will be ready for CPU (arrival or post-I/O)
    int first_start;        // first time it ran on CPU
    int last_cpu_finish;    // time when the last CPU burst finished
    int done;               // 1 if all CPU work completed
} Task;

typedef struct { int task_id, start, end; } Slice;

typedef struct {
    Slice *data; int size, cap;
} Timeline;

static void tl_init(Timeline *t){ t->data=NULL; t->size=0; t->cap=0; }
static void tl_push(Timeline *t,int id,int s,int e){
    if(e<=s) return;
    if(t->size==t->cap){ t->cap = t->cap ? t->cap*2 : 16; t->data = (Slice*)realloc(t->data, t->cap*sizeof(Slice)); }
    t->data[t->size++] = (Slice){id,s,e};
}
static void tl_free(Timeline *t){ free(t->data); t->data=NULL; t->size=t->cap=0; }

static int all_done(Task *a, int n){ for(int i=0;i<n;i++) if(!a[i].done) return 0; return 1; }

static int min_ready_time(Task *a,int n){
    int r = -1;
    for(int i=0;i<n;i++) if(!a[i].done){
        if(r<0 || a[i].next_ready_time<r) r = a[i].next_ready_time;
    }
    return r<0?0:r;
}

static void init_runtime(Task *a,int n){
    for(int i=0;i<n;i++){
        if(a[i].loops < 0) a[i].loops = 0;
        if(a[i].loops == 0){
            // normalize to single CPU burst as one "loop" with non-blocking trailing I/O if any
            a[i].loops = 1;
            // a[i].io is the "post-CPU" I/O; we’ll treat it as *trailing only*
            // so there is no blocking I/O between loops (since loops=1 => none)
            // include_last_io is already set automatically by input for single-burst mode.
        }
        a[i].cur_loop = 0;
        a[i].remaining_cpu = a[i].cpu;
        a[i].next_ready_time = a[i].arrival;
        a[i].first_start = -1;
        a[i].last_cpu_finish = -1;
        a[i].done = 0;
    }
}

static void print_timeline(const Timeline *tl){
    printf("\nCPU Timeline:\n");
    for(int i=0;i<tl->size;i++){
        printf("[T%d %d->%d] ", tl->data[i].task_id, tl->data[i].start, tl->data[i].end);
    }
    printf("\n");
}

static void print_summary(Task *a,int n){
    printf("\nPer-task summary:\n");
    printf("ID | Arrival | CPU/loop | IO/loop | Loops | Start | End (incl. IO) | Turnaround | Waiting\n");
    printf("---+---------+----------+---------+-------+-------+-----------------+------------+--------\n");
    for(int i=0;i<n;i++){
        int total_cpu = a[i].loops * a[i].cpu;
        int io_between = (a[i].loops>1? (a[i].loops-1)*a[i].io : 0);
        int trailing_io = a[i].include_last_io ? a[i].io : 0;
        int end_with_io = a[i].last_cpu_finish + trailing_io;
        int turnaround = end_with_io - a[i].arrival;
        int waiting = turnaround - total_cpu - io_between - trailing_io;
        if(waiting < 0) waiting = 0;
        printf("%2d | %7d | %8d | %7d | %5d | %5d | %13d | %10d | %7d\n",
            a[i].id, a[i].arrival, a[i].cpu, a[i].io, a[i].loops,
            a[i].first_start, end_with_io, turnaround, waiting);
    }
    printf("\n");
}

/* -------------------- Ready-Set Helpers -------------------- */

static void collect_ready_fcfs(int time, Task *a, int n, int *q, int *qh, int *qt, int *inQ){
    for(int i=0;i<n;i++){
        if(!a[i].done && !inQ[i] && a[i].remaining_cpu>0 && a[i].next_ready_time <= time){
            q[(*qt)++] = i; inQ[i]=1;
        }
    }
}

static int pick_sjf(int time, Task *a, int n){
    int best=-1;
    for(int i=0;i<n;i++){
        if(!a[i].done && a[i].remaining_cpu>0 && a[i].next_ready_time<=time){
            if(best<0 || a[i].remaining_cpu < a[best].remaining_cpu ||
              (a[i].remaining_cpu == a[best].remaining_cpu &&
               (a[i].arrival < a[best].arrival || (a[i].arrival==a[best].arrival && a[i].id < a[best].id)))){
                best=i;
            }
        }
    }
    return best;
}

static int pick_priority(int time, Task *a, int n){
    int best=-1;
    for(int i=0;i<n;i++){
        if(!a[i].done && a[i].remaining_cpu>0 && a[i].next_ready_time<=time){
            if(best<0 || a[i].priority < a[best].priority ||
              (a[i].priority == a[best].priority &&
               (a[i].arrival < a[best].arrival || (a[i].arrival==a[best].arrival && a[i].id < a[best].id)))){
                best=i;
            }
        }
    }
    return best;
}

static void after_full_cpu_burst(Task *t, int *time){
    // Finished the current loop's CPU burst at *time
    t->last_cpu_finish = *time;
    t->cur_loop++;
    if(t->cur_loop < t->loops){
        // block on I/O between loops
        t->next_ready_time = *time + t->io;
        t->remaining_cpu   = t->cpu;
    }else{
        // all CPU done; optional trailing I/O is *not* blocking
        t->done = 1;
    }
}

/* -------------------- FCFS -------------------- */
static void schedule_fcfs(Task *orig, int n){
    Task *a = (Task*)malloc(n*sizeof(Task));
    memcpy(a, orig, n*sizeof(Task));
    init_runtime(a,n);

    Timeline tl; tl_init(&tl);

    int *queue = (int*)malloc(sizeof(int)* (n*1024 + 16));
    int *inQ = (int*)calloc(n, sizeof(int));
    int qh=0, qt=0;

    int time = min_ready_time(a,n);
    collect_ready_fcfs(time, a, n, queue, &qh, &qt, inQ);

    while(!all_done(a,n)){
        if(qh==qt){ // no ready task: jump to next event
            time = min_ready_time(a,n);
            collect_ready_fcfs(time, a, n, queue, &qh, &qt, inQ);
            continue;
        }
        int i = queue[qh++]; inQ[i]=0;

        if(a[i].first_start < 0) a[i].first_start = time;

        int start=time;
        int run = a[i].remaining_cpu; // non-preemptive
        time += run;
        a[i].remaining_cpu = 0;
        tl_push(&tl, a[i].id, start, time);

        after_full_cpu_burst(&a[i], &time);

        // collect any tasks that became ready by 'time'
        collect_ready_fcfs(time, a, n, queue, &qh, &qt, inQ);
    }

    print_timeline(&tl);
    print_summary(a,n);

    free(queue); free(inQ);
    tl_free(&tl);
    free(a);
}

/* -------------------- SJF (non-preemptive, shortest next CPU burst) -------------------- */
static void schedule_sjf(Task *orig, int n){
    Task *a = (Task*)malloc(n*sizeof(Task));
    memcpy(a, orig, n*sizeof(Task));
    init_runtime(a,n);

    Timeline tl; tl_init(&tl);
    int time = min_ready_time(a,n);

    while(!all_done(a,n)){
        int i = pick_sjf(time, a, n);
        if(i<0){ time = min_ready_time(a,n); continue; }

        if(a[i].first_start < 0) a[i].first_start = time;

        int start=time;
        int run = a[i].remaining_cpu;
        time += run;
        a[i].remaining_cpu = 0;
        tl_push(&tl, a[i].id, start, time);

        after_full_cpu_burst(&a[i], &time);
    }

    print_timeline(&tl);
    print_summary(a,n);

    tl_free(&tl);
    free(a);
}

/* -------------------- Priority (non-preemptive) -------------------- */
static void schedule_priority(Task *orig, int n){
    Task *a = (Task*)malloc(n*sizeof(Task));
    memcpy(a, orig, n*sizeof(Task));
    init_runtime(a,n);

    Timeline tl; tl_init(&tl);
    int time = min_ready_time(a,n);

    while(!all_done(a,n)){
        int i = pick_priority(time, a, n);
        if(i<0){ time = min_ready_time(a,n); continue; }

        if(a[i].first_start < 0) a[i].first_start = time;

        int start=time;
        int run = a[i].remaining_cpu;
        time += run;
        a[i].remaining_cpu = 0;
        tl_push(&tl, a[i].id, start, time);

        after_full_cpu_burst(&a[i], &time);
    }

    print_timeline(&tl);
    print_summary(a,n);

    tl_free(&tl);
    free(a);
}

/* -------------------- Round Robin (preemptive) -------------------- */
typedef struct { int *q; int h,t,cap; } Q;
static void q_init(Q *q,int cap){ q->cap=cap?cap:16; q->q=(int*)malloc(q->cap*sizeof(int)); q->h=q->t=0; }
static int  q_empty(Q *q){ return q->h==q->t; }
static void q_push(Q *q,int v){
    q->q[q->t++] = v; if(q->t==q->cap) q->t=0;
    if(q->t==q->h){ // grow
        int ncap=q->cap*2; int *nq=(int*)malloc(ncap*sizeof(int)); int i=0;
        while(!q_empty(q)){ nq[i++]=q->q[q->h++]; if(q->h==q->cap) q->h=0; }
        free(q->q); q->q=nq; q->cap=ncap; q->h=0; q->t=i;
    }
}
static int  q_pop(Q *q){ int v=q->q[q->h++]; if(q->h==q->cap) q->h=0; return v; }
static void q_free(Q *q){ free(q->q); q->q=NULL; q->h=q->t=q->cap=0; }

static void collect_ready_rr(int time, Task *a, int n, Q *qq, int *inQ){
    for(int i=0;i<n;i++){
        if(!a[i].done && a[i].remaining_cpu>0 && a[i].next_ready_time<=time && !inQ[i]){
            q_push(qq, i); inQ[i]=1;
        }
    }
}

static void schedule_rr(Task *orig, int n, int quantum){
    if(quantum<=0){ printf("Quantum must be > 0.\n"); return; }

    Task *a = (Task*)malloc(n*sizeof(Task));
    memcpy(a, orig, n*sizeof(Task));
    init_runtime(a,n);

    Timeline tl; tl_init(&tl);
    Q qq; q_init(&qq, n*2+16);
    int *inQ = (int*)calloc(n, sizeof(int));

    int time = min_ready_time(a,n);
    collect_ready_rr(time, a, n, &qq, inQ);

    while(!all_done(a,n)){
        if(q_empty(&qq)){
            time = min_ready_time(a,n);
            collect_ready_rr(time, a, n, &qq, inQ);
            continue;
        }
        int i = q_pop(&qq); inQ[i]=0;

        if(a[i].first_start < 0) a[i].first_start = time;

        int run = a[i].remaining_cpu < quantum ? a[i].remaining_cpu : quantum;
        int start=time; int end=time+run;
        tl_push(&tl, a[i].id, start, end);

        a[i].remaining_cpu -= run;
        time = end;

        // Add any tasks that became ready during this slice
        collect_ready_rr(time, a, n, &qq, inQ);

        if(a[i].remaining_cpu > 0){
            // still in same CPU burst, round-robin back to tail
            q_push(&qq, i); inQ[i]=1;
        }else{
            // finished this CPU burst
            a[i].last_cpu_finish = time;
            a[i].cur_loop++;
            if(a[i].cur_loop < a[i].loops){
                a[i].remaining_cpu = a[i].cpu;
                a[i].next_ready_time = time + a[i].io; // block on I/O
                // will be added to queue when ready by collect_ready_rr
            }else{
                a[i].done = 1;
            }
        }
    }

    print_timeline(&tl);
    print_summary(a,n);

    free(inQ); q_free(&qq);
    tl_free(&tl);
    free(a);
}

/* -------------------- Input -------------------- */
static int read_int(const char *p){
    int x; for(;;){
        printf("%s", p);
        if(scanf("%d",&x)==1) return x;
        int c; while((c=getchar())!='\n' && c!=EOF){}; printf("Invalid input. Try again.\n");
    }
}

static void read_tasks(Task *a,int n, int ask_priority){
    for(int i=0;i<n;i++){
        a[i].id = i+1;
        a[i].arrival = read_int("\nTask arrival time: ");
        int loops = read_int("Number of loops (0 for single burst): ");
        a[i].loops = loops;

        if(loops > 0){
            a[i].cpu = read_int("CPU per loop: ");
            a[i].io  = read_int("I/O per loop: ");
            a[i].include_last_io = read_int("Include I/O after the LAST loop in turnaround? (0/1): ");
        }else{
            a[i].cpu = read_int("Single CPU burst time: ");
            a[i].io  = read_int("Post-CPU I/O (0 if none): ");
            a[i].include_last_io = (a[i].io > 0) ? 1 : 0;
        }

        if(ask_priority){
            a[i].priority = read_int("Priority (smaller = higher): ");
        }else{
            a[i].priority = 0;
        }
    }
}

/* -------------------- Main -------------------- */
int main(void){
    printf("CPU Scheduler Calculator (with CPU/I-O loops)\n");
    printf("--------------------------------------------\n");
    printf("1) FCFS (FIFO)\n");
    printf("2) SJF (non-preemptive, shortest NEXT CPU burst)\n");
    printf("3) Priority (non-preemptive; smaller value = higher priority)\n");
    printf("4) Round Robin (preemptive)\n");

    int choice = read_int("Enter choice: ");
    int n = read_int("How many tasks? ");
    if(n<=0){ printf("Nothing to schedule.\n"); return 0; }

    Task *tasks = (Task*)calloc(n, sizeof(Task));

    switch(choice){
        case 1:
            printf("\n-- Enter tasks for FCFS/FIFO --\n");
            read_tasks(tasks, n, 0);
            schedule_fcfs(tasks, n);
            break;
        case 2:
            printf("\n-- Enter tasks for SJF (non-preemptive) --\n");
            read_tasks(tasks, n, 0);
            schedule_sjf(tasks, n);
            break;
        case 3:
            printf("\n-- Enter tasks for Priority (non-preemptive) --\n");
            read_tasks(tasks, n, 1);
            schedule_priority(tasks, n);
            break;
        case 4: {
            printf("\n-- Enter tasks for Round Robin (preemptive) --\n");
            read_tasks(tasks, n, 0);
            int q = read_int("Time quantum: ");
            schedule_rr(tasks, n, q);
            break;
        }
        default:
            printf("Unknown choice.\n");
    }

    free(tasks);
    return 0;
}
