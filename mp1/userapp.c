#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>

#define PID_SIZE 64
#define BUFFER_SIZE 4096
// This is simple user space application that registers itself
// Open and write Proc Filesystem entry using fopen and fputs
// Open and read this entry using open and read
int main(void)
{   
    int pid = (int)getpid();
    char *buffer;
    FILE *fd;
    buffer = malloc(PID_SIZE);
    // open /proc/mp1/status to write
    fd = fopen("/proc/mp1/status", "w+");
    if(fd == NULL){
        printf("Fail to open /proc/mp1/status\n");
        return -1;
    }
    // store pid information into buffer
    snprintf(buffer, 64, "%d\n", pid);
    if(fputs(buffer, fd) < 0){
        printf("Fail to write pid in /proc/mp1/status\n");
        return -1;
    }
    fclose(fd);

    // Please tweak the iteration counts to make this calculation run long enough
    volatile long long unsigned int sum = 0;
    for (int i = 0; i < 100000000; i++) {
        volatile long long unsigned int fac = 1;
        for (int j = 1; j <= 50; j++) {
            fac *= j;
        }
        sum += fac;
    }
    printf("Finish Calculation\n");

    char *read_buffer;
    read_buffer = malloc(BUFFER_SIZE);
    memset(read_buffer, 0, BUFFER_SIZE);
    int file;
    // open /proc/mp1/status to read
    file = open("/proc/mp1/status", O_RDONLY, 0644);
    if(file == -1){
        printf("Fail to open /proc/mp1/status\n");
        return -1;
    }
    // read /proc/mp1/status
    if (read(file, read_buffer, strlen(read_buffer)) < 0) {
        perror("Fail to read from /proc/mp1/status with pid\n");
        return -1;
    }
    if (close(file) == -1) {
        perror("Error closing file");
        return 1;
    }
    // free allocated memory
    free(buffer);
    free(read_buffer);
    printf("Finish Userapp\n");

    return 0;
}
