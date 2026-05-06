#include <stdio.h>
#include <stdlib.h>

static int read_int(const char *p){
    int x; for(;;){
        printf("%s", p);
        if (scanf("%d",&x)==1) return x;
        int c; while((c=getchar())!='\n' && c!=EOF){}; puts("Invalid input. Try again.");
    }
}

int main(void){
    for(;;){
        puts("\n=== Scheduler UI ===");
        puts("1) Simulator + Job Generator (writes sim_log.txt & core_trace.txt)");
        puts("2) Calculator (FCFS/SJF/Priority/RR with CPU/I/O loops)");
        puts("0) Quit");
        int choice = read_int("Enter choice: ");

        if (choice == 0) break;

#ifdef _WIN32
        const char *sim_cmd = ".\\sim_gen.exe";
        const char *calc_cmd = ".\\cpu_calc.exe";
#else
        const char *sim_cmd = "./sim_gen";
        const char *calc_cmd = "./cpu_calc";
#endif
        int rc = -1;
        if (choice == 1) {
            rc = system(sim_cmd);
        } else if (choice == 2) {
            rc = system(calc_cmd);
        } else {
            puts("Unknown option.");
            continue;
        }
        if (rc != 0) puts("Command returned a non-zero status.");
    }
    return 0;
}