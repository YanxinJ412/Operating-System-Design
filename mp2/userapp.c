#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include "userapp.h"

#define BUFFER_SIZE 2048

// get the current time in microsecond
unsigned long get_time_in_μs() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1e6 + tv.tv_usec;
}

// register the task by writing "R,pid,period,runtime" to /proc/mp2/status
int register_task(int pid, unsigned long period, unsigned long runtime) {
    char buffer[BUFFER_SIZE];
    int size;
    // open file
    int fd_reg = open("/proc/mp2/status", O_WRONLY);
    if (fd_reg == -1) {
        printf("Failed to open /proc/mp2/status for registration");
        return -1;
    }
    size = snprintf(buffer, BUFFER_SIZE, "R,%d,%lu,%lu\n", pid, period, runtime);
    if(write(fd_reg, buffer, size) <= 0) {
        printf("Failed to write registration to /proc/mp2/status");
        close(fd_reg);
        return -1;
    }
    close(fd_reg);
    return 0;
}
// verify the registration by reading /proc/mp2/status and see if the 
// correct pid is written in status
int verify_registration(int pid) {
    int fd_verify = open("/proc/mp2/status", O_RDONLY);
    if (fd_verify == -1) {
        printf("Failed to open /proc/mp2/status for verification");
        return -1;
    }
    char buffer[BUFFER_SIZE];
    int fd_pid;
    ssize_t byte_read;
    unsigned long period, runtime;
    // read status file line by line and find the related line with pid
    if(read(fd_verify, buffer, sizeof(buffer)) > 0) {
        char *line = strtok(buffer, "\n");
        while (line != NULL) {
            if (sscanf(line, "%d: %lu, %lu", &fd_pid, &period, &runtime) == 3) {
                // match the pid in file with the real pid.
                if (fd_pid == pid) {
                    close(fd_verify);
                    return 0;
                }
            }
            line = strtok(NULL, "\n");
        }
    }
    close(fd_verify);
    printf("Cann't verify the registration");
    return -1;
}

// yield the task with pid by writing "Y,pid" to /proc/mp2/status
int yield_task(int pid) {
    char buffer[BUFFER_SIZE];
    int size;
    // open file
    int fd_yield = open("/proc/mp2/status", O_WRONLY);
    if (fd_yield == -1) {
        printf("Failed to open /proc/mp2/status for yielding\n");
        return -1;
    }
    // write yield command into status file
    size = snprintf(buffer, BUFFER_SIZE, "Y,%d\n", pid);
    if(write(fd_yield, buffer, size) <= 0) {
        printf("Failed to write yield to /proc/mp2/status\n");
        close(fd_yield);
        return -1;
    }
    close(fd_yield);
    return 0;
}
// performn factorials
void do_job() {
    unsigned long long factorial = 1;
    for (int i = 1; i <= 10; i++) {
        factorial *= i;
    }
}

// deregister the task with pid by writing "D,pid" to /proc/mp2/status
int deregister_task(int pid) {
    char buffer[BUFFER_SIZE];
    int size;
    int fd_dereg = open("/proc/mp2/status", O_WRONLY);
    // open file
    if (fd_dereg == -1) {
        printf("Failed to open /proc/mp2/status for deregistration");
        return -1;
    }
    // write deregister command into status file
    size = snprintf(buffer, BUFFER_SIZE, "D,%d\n", pid);
    if(write(fd_dereg, buffer, size) <= 0) {
        printf("Failed to write deregistration to /proc/mp2/status");
        close(fd_dereg);
        return -1;
    }
    close(fd_dereg);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Usage: %s <period_ms> <runtime_ms> <tasks>\n", argv[0]);
        return -1;
    }
    // extract period, runtime, and number of jobs for each pid
    int pid = getpid();
    unsigned long period_ms = (unsigned long)atoi(argv[1]);
    unsigned long runtime_ms = (unsigned long)atoi(argv[2]);
    int tasks = atoi(argv[3]);
    // register
    if (register_task(pid, period_ms, runtime_ms) < 0) {
        return -1;
    }
    // verify_registration
    if (verify_registration(pid) < 0) {
        return -1;
    }
    // printf("Finish verify_registration for PID: %d\n", pid);
    unsigned long t0 = get_time_in_μs();
    if (yield_task(pid) < 0) {
        printf("Failed to yield\n");
        return -1;
    }
    for (int i = 0; i < tasks; i++) {
        // calculate processing time.
        unsigned long wakeup_time = get_time_in_μs() - t0;
        do_job();  
        unsigned long process_time = get_time_in_μs() - wakeup_time;
        // yield tasks
        if (yield_task(pid) < 0) {
            printf("Failed to yield\n");
            return -1;
        }
        printf("PID: %d, Period: %lu ms, Processing Time: %lu µs\n", pid, period_ms, process_time);
    }
    // deregister task
    if (deregister_task(pid) < 0) {
        printf("Failed to deregister task\n");
        return -1;
    }
    printf("finish deregister for PID: %d\n", pid);
    return 0;
}
