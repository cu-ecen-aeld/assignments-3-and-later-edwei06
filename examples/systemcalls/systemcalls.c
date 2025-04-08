
#include "systemcalls.h"
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
bool do_system(const char *cmd) 
{
    int ret = system(cmd);
    if(ret == -1) {
        return false;
    }
    return (ret == 0);
}

bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);
    char *command[count+1];
    for (int i = 0; i < count; i++) {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    va_end(args);

    // Ensure the command is provided with an absolute path.
    if (command[0][0] != '/') {
        return false;
    }

    pid_t pid = fork();
    if (pid < 0) {
        return false;
    }
    if (pid == 0) {
        // In the child process: flush stdout to avoid duplicate prints.
        fflush(stdout);
        if (execv(command[0], command) == -1) {
            exit(EXIT_FAILURE);
        }
        // Should never reach here.
        exit(EXIT_FAILURE);
    } else {
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            return false;
        }
        if (WIFEXITED(status)) {
            return (WEXITSTATUS(status) == 0);
        } else {
            return false;
        }
    }
    // Extra return to satisfy compiler, though unreachable.
    return false;
} 


bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char *command[count+1];
    for (int i = 0; i < count; i++) {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    va_end(args);

    // Ensure the command is an absolute path.
    if (command[0][0] != '/') {
        return false;
    }

    pid_t pid = fork();
    if (pid < 0) {
        return false;
    }
    if (pid == 0) {
        int fd = open(outputfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            exit(EXIT_FAILURE);
        }
        if (dup2(fd, STDOUT_FILENO) < 0) {
            close(fd);
            exit(EXIT_FAILURE);
        }
        close(fd);
        fflush(stdout);
        if (execv(command[0], command) == -1) {
            exit(EXIT_FAILURE);
        }
        // Should never reach here.
        exit(EXIT_FAILURE);
    } else {
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            return false;
        }
        if (WIFEXITED(status)) {
            return (WEXITSTATUS(status) == 0);
        } else {
            return false;
        }
    }
    // Extra return to satisfy compiler, though unreachable.
    return false;
}
