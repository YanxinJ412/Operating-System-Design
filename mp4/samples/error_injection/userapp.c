#include <sys/syscall.h> 
#include <stdio.h>    
#include <unistd.h> 

// call some system calls and invoke perror to print the error msg associated with the injected errno.
int main(void)
{
	/* TODO: Implement this */
    // call system call getpid
	long pid = syscall(SYS_getpid);  
    if (pid < 0) {
        // print perror
        perror("Error during syscall getpid");
    } else {
        printf("PID: %ld\n", pid);
    }
    // call system call dup
    // int fd = dup(-1);  
    // if (fd < 0) {
    //     print perror
    //     perror("Error during syscall dup");
    // } else {
    //     printf("dup successful: %d\n", fd);
    // }

	return 0;
}
