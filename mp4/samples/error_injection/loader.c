#define USAGE "./loader <syscall> <errno>"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/perf_event.h>
#include <linux/unistd.h>
#include <librex.h>
#include <libbpf.h>
#include <bpf.h>
#include <sys/wait.h>


#define EXE "./target/x86_64-unknown-linux-gnu/release/error_injection"
static int num_pid = 0;


/*
 * This function is responsible for loading, attaching, and updating the maps 
 * for our Rex program. It should take in the target system call name as a string
 * followed by the errno as a positive number (as errnos are defined to be all 
 * positive numbers)
 * Input: argc: number of arguments
 *        argv: array of arguments : syscall_name, errno
 * Output: sucess: 0; failure: -1
 */
int main(int argc, char *argv[])
{
	/* TODO: Implement this */
	const char *syscall_name, *section;
	int errno_, map_errno_fd, map_pid_fd, key_;
	struct bpf_object *obj;
	struct bpf_program *prog;
	struct bpf_link *link;
	pid_t pid_, pid_child;
	// check if the number of arguments is correct
	if (argc != 3) {
        fprintf(stderr, "USAGE: %s\n", USAGE);
        return EXIT_FAILURE;
    }
	// get the syscall name and errno
	syscall_name = argv[1];
    errno_ = atoi(argv[2]);
	link = NULL;

	rex_set_debug(1); // enable debug info
	// load the object file of Rex program
	obj = rex_obj_get_bpf(rex_obj_load(EXE));
	if (!obj) {
		fprintf(stderr, "Object could not be opened\n");
		
		return -1;
	}
	// Find bpf program by name
	prog = bpf_object__find_program_by_name(obj, "err_injector");
	if (!prog) {
		fprintf(stderr, "_start not found\n");
		
		return 1;
	}
	// attach BPF program to the target system call
	link = bpf_program__attach_ksyscall(prog, syscall_name, NULL);
	if (libbpf_get_error(link)) {
		fprintf(stderr, "ERROR: bpf_program__attach_ksyscall failed\n");
		link = NULL;
		return -1;
	}
	// find map_errno_fd by name
	map_errno_fd = bpf_object__find_map_fd_by_name(obj, "MAP_ERRNO");
	if (map_errno_fd < 0) {
		fprintf(stderr, "ERROR: finding map_errno_fd failed\n");
		bpf_link__destroy(link);
		return -1;
	}
	// fork a child process to run the userapp
	pid_child = fork();
    if (pid_child == 0) {
		int key_ = 0;
		// get child pid
		pid_child = getpid();
		// find map_pid_fd by name
        int map_pid_fd = bpf_object__find_map_fd_by_name(obj, "MAP_PID");
        if (map_pid_fd < 0) {
            fprintf(stderr, "ERROR: finding map_pid_fd failed\n");
			bpf_link__destroy(link);
            return -1;
        }
		// update map_pid_fd with (0, target_pid)
		if (bpf_map_update_elem(map_pid_fd, &key_, &pid_child, BPF_ANY) < 0) {
			fprintf(stderr, "ERROR: bpf_map_update_elem failed\n");
			bpf_link__destroy(link);
			return -1;
		}
		// update map_errno_fd with (0, errno)
		if (bpf_map_update_elem(map_errno_fd, &key_, &errno_, BPF_ANY) < 0) {
			fprintf(stderr, "ERROR: bpf_map_update_elem failed\n");
			bpf_link__destroy(link);
			return -1;
		}
		// execute the userapp
		execl("./userapp", "userapp", NULL);
        perror("execl");
        return -1;
    } else if (pid_child > 0) {
		// parent pid should wait for the child to finish.
        wait(NULL);
    } else {
		// fork failed
        perror("fork");
        return -1;
    }

	// clean up link
	bpf_link__destroy(link);
	return 0;
}
