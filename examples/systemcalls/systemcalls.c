#include "systemcalls.h"
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed successfully using system(),
 *         false if an error occurred or if a non-zero return value was returned.
 */
bool do_system(const char *cmd)
{
    int ret = system(cmd);
    if(ret == -1) {
        return false;
    }
    return (ret == 0);
}

/**
 * @param count - The number of variables passed to the function. The variables are:
 *   the command to execute followed by arguments to pass to execv().
 * @param ... - A list of 1 or more arguments after @param count.
 *   The first is always the full path to the command to execute.
 * @return true if the command was executed successfully using execv() with fork() and wait,
 *         false if an error occurred or a non-zero exit status is returned.
 */
bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);
    char *command[count + 1];
    for (int i = 0; i < count; i++) {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    va_end(args);

    /* Check that the command is provided with an absolute path. */
    if (command[0][0] != '/') {
        return false;
    }

    pid_t pid = fork();
    if (pid < 0) {
        return false;
    }
    if (pid == 0) {
        /* In the child process: flush stdout to avoid duplicated prints,
           then execute command with execv().  If execv() fails, exit with failure. */
        fflush(stdout);
        if (execv(command[0], command) == -1) {
            exit(EXIT_FAILURE);
        }
        exit(EXIT_FAILURE); // This line should never be reached.
    } else {
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            return false;
        }
        if (WIFEXITED(status)) {
            return (WEXITSTATUS(status) == 0);
        }
        return false;
    }
    return false; /* Extra return to ensure control does not reach end without a value */
}

/**
 * @param outputfile - The full path to the file where command output will be written.
 * @param count - The number of variables passed following outputfile.
 * @param ... - A list of 1 or more arguments: the first is the full path to the command,
 *   and the remaining arguments are passed to execv().
 * @return true if the command and its redirection performed successfully; false otherwise.
 */
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char *command[count + 1];
    for (int i = 0; i < count; i++) {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    va_end(args);

    /* Check that the command is provided with an absolute path. */
    if (command[0][0] != '/') {
        return false;
    }

    pid_t pid = fork();
    if (pid < 0) {
        return false;
    }
    if (pid == 0) {
        /* In the child: Open (or create) the output file and redirect stdout. */
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
        exit(EXIT_FAILURE);  // This should not be reached.
    } else {
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            return false;
        }
        if (WIFEXITED(status)) {
            return (WEXITSTATUS(status) == 0);
        }
        return false;
    }
    return false; /* Extra return to satisfy compiler. */
}

