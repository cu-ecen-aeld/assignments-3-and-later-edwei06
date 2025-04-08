#include "systemcalls.h"
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed successfully using
 *   the system() call, false if an error occurred or if a non-zero return
 *   value was returned.
 */
bool do_system(const char *cmd)
{
    int ret = system(cmd);
    if (ret == -1) {
        return false;
    }
    return (ret == 0);
}

/**
 * @param count - The number of variables passed to the function. The variables are
 *   the command to execute followed by arguments for the command.
 * @param ... - A list of 1 or more arguments after the @param count argument.
 *   The first is always the full path to the command to execute with execv()
 *   The remaining arguments are a list of arguments to pass to execv().
 * @return true if the command and its arguments were executed successfully using execv(),
 *   false if an error occurred or if a non-zero return value was returned.
 */
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

    pid_t pid = fork();
    if (pid < 0) {
        return false;
    }
    if (pid == 0) {
        // Child process: Execute the command.
        if (execv(command[0], command) == -1) {
            exit(EXIT_FAILURE);
        }
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
    return false;
}

/**
 * @param outputfile - The full path to the file to write with command output.
 *   This file will be closed at the end of the function call.
 * @param count - The number of variables passed to the function, similar to do_exec().
 * @param ... - A list of 1 or more arguments after @param count.
 * @return true if the command was executed successfully with its output redirected,
 *   false otherwise.
 */
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

    pid_t pid = fork();
    if (pid < 0) {
        return false;
    }
    if (pid == 0) {
        int fd = open(outputfile, O_WRONLY | O_TRUNC | O_CREAT, 0644);
        if (fd < 0) {
            exit(EXIT_FAILURE);
        }
        if (dup2(fd, STDOUT_FILENO) < 0) {
            close(fd);
            exit(EXIT_FAILURE);
        }
        close(fd);
        if (execv(command[0], command) == -1) {
            exit(EXIT_FAILURE);
        }
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
    return false;
}

