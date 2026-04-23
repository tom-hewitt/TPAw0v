/*
    Brief: This is a simple demo to show how to use ETM to trace a target application.

    This demo should run on ZCU102/Kria board as long as the APU has linux running.
    Contrary to the original paper, this demo does not need RPU.

    The purpose of this demo is to provide a template for researchers who want to use the CoreSight debug infrastructure.

    This demo illustrates how to use TMC2 to store trace data to SRAM.

    Author: Weifan Chen
    Date: 2024-08-17
*/

#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <sys/wait.h>
#include <time.h>
#include "common.h"
#include "pmu_event.h"
#include "cs_etm.h"
#include "cs_config.h"
#include "cs_soc.h"

extern volatile ETM_interface *etms[4];
extern volatile TMC_interface *tmc2;

int sram_is_empty() {
    return (tmc2->status & (0x1 << 4)) >> 4;
}

int tmc2_is_ready() {
    return (tmc2->status & (0x1 << 2)) >> 2;
}

int all_read() {
    return tmc2->ram_read_pt == tmc2->ram_write_pt;
}

void read_trace_data_from_SRAM()
{
    printf("\nDumping trace data from SRAM\n");
    tmc_disable(tmc2);
    while(!tmc2_is_ready());
    while(!all_read()) {
        uint32_t data = tmc2->ram_read_data;
        if (data != 0xffffffff) {
            printf("0x%08x\n", data);
        }
    }
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <command> [args...]\n", argv[0]);
        return 1;
    }

    printf("Vanilla ZCU102 self-host trace demo.\n");
    printf("Build: on %s at %s\n\n", __DATE__, __TIME__);

    pid_t target_pid;

    // Disabling all cpuidle. Access the ETM of an idled core will cause a hang.
    linux_disable_cpuidle();

    // Pin to the 4-th core, because we will use cores 1-3 to run the target application.
    pin_to_core(3);

    cs_config_SRAM();

    // initialize ETM
    config_etm_n(etms[0], 0, 1);

    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    // fork a child to execute the target application
    target_pid = fork();
    if (target_pid == 0)
    {
        cpu_set_t set;
        CPU_ZERO(&set);
        CPU_SET(0, &set);
        CPU_SET(1, &set);
        CPU_SET(2, &set);
        sched_setaffinity(0, sizeof(cpu_set_t), &set);
        sched_yield();
        uint64_t child_pid = (uint64_t) getpid();

        // further configure ETM. So that it will only trace the process with pid == child_pid/target_pid
        etm_set_contextid_cmp(etms[0], child_pid);

        // Enable ETM, start trace session
        etm_enable(etms[0]);
        etm_enable(etms[1]);
        etm_enable(etms[2]);

        // execute target application
        execvp(argv[1], &argv[1]);
        perror("execvp failed. Target application failed to start.");
        exit(1);
    }
    else if (target_pid < 0)
    {
        perror("fork");
        return 1;
    }

    // wait for target application to finish
    int status;
    waitpid(target_pid, &status, 0);

    clock_gettime(CLOCK_MONOTONIC, &end_time);

    // Disable ETM, our trace session is done. Poller will print trace data.
    etm_disable(etms[0]);
    etm_disable(etms[1]);
    etm_disable(etms[2]);

    read_trace_data_from_SRAM();

    // Calculate and print the execution time
    double execution_time = (end_time.tv_sec - start_time.tv_sec) +
                            (end_time.tv_nsec - start_time.tv_nsec) / 1e9;
    printf("Execution time: %f seconds\n", execution_time);

    return 0;
}
