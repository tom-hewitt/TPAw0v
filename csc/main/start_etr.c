/*
    Brief: This is a simple demo to show how to use ETM to trace a target application.

    This demo should run on ZCU102/Kria board as long as the APU has linux running.
    Contrary to the original paper, this demo does not need RPU.

    The purpose of this demo is to provide a template for researchers who want to use the CoreSight debug infrastructure.

    This demo illustrates how to use ETR to route trace data to any memory mapped address.

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
extern volatile TMC_interface *tmc3;

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

    // configure CoreSight to use ETR; The addr and size is the On-Chip memory (OCM) on chip.
    // You can change the addr and size to use any other
    // uint64_t buf_addr = 0x00FFE00000; // RPU 0 ATCM
    // uint32_t buf_size = 1024 * 64;
    uint64_t buf_addr = 0x00FFFC0000;  //OCM
    uint32_t buf_size = 1024 * 256;

    cs_config_etr_mp(buf_addr, buf_size);

    // prepare the trace data buffer
    clear_buffer(buf_addr, buf_size);

    // initialize ETM
    config_etm_n(etms[0], 0, 1);
    config_etm_n(etms[1], 0, 2);
    config_etm_n(etms[2], 0, 3);

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

    // Disable ETM, our trace session is done
    etm_disable(etms[0]);
    etm_disable(etms[1]);
    etm_disable(etms[2]);

    munmap((void *)etms[0], sizeof(ETM_interface));

    // drain the TMC3 (ETR) and write the trace data to files
    tmc_drain_data(tmc3);

    // Calculate and print the execution time
    double execution_time = (end_time.tv_sec - start_time.tv_sec) + (end_time.tv_nsec - start_time.tv_nsec) / 1e9;
    printf("Execution time: %f seconds\n", execution_time);

    return 0;
}
