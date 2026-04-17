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
#include <stdio.h>
#include <sys/wait.h>
#include <time.h>
#include "pmu_event.h"
#include "common.h"

int main(int argc, char *argv[])
{
    printf("Vanilla ZCU102 no trace demo.\n");
    printf("Build: on %s at %s\n\n", __DATE__, __TIME__);

    pid_t target_pid;

    // Disabling all cpuidle. Access the ETM of an idled core will cause a hang.
    linux_disable_cpuidle();

    // Pin to the 4-th core, because we will use 1st core to run the target application.
    pin_to_core(3);

    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    // fork a child to execute the target application
    target_pid = fork();
    if (target_pid == 0)
    {
        pin_to_core(0);

        // execute target application
        execl("/usr/bin/cp", "cp", "-r", "/home/petalinux/glibc", "/home/petalinux/glibc_copy", NULL);
        perror("execl failed. Target application failed to start.");
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

    // Calculate and print the execution time
    double execution_time = (end_time.tv_sec - start_time.tv_sec) + (end_time.tv_nsec - start_time.tv_nsec) / 1e9;
    printf("Execution time: %f seconds\n", execution_time);

    return 0;
}
