#include <stdio.h>
#include <stdlib.h>
#include <errno.h> 
#include <signal.h>
#include <sys/wait.h>

// runs at the end of every child process
void sigchld_handler(int s)
{
    (void)s;
    int saved_errno = errno;
    while(waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno;
}

int setup_sigchld()
{
    struct sigaction sa;
    sa.sa_handler = sigchld_handler; 
    sa.sa_flags = SA_RESTART;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        return -1;
    }

    return 0;
}