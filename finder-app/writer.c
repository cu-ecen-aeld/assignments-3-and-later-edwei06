/*
 * finder-app/writer.c
 *
 * This application writes a provided string to a specified file.
 * It uses standard file I/O to write the contents, and employs syslog
 * (with the LOG_USER facility) to log messages. A debug message is logged
 * before writing, and any unexpected errors are logged at the error level.
 *
 * Usage:
 *   writer <string> <file>
 *
 * Requirements:
 * - Log the message "Writing <string> to <file>" at LOG_DEBUG level.
 * - Log any file open/write/close errors at LOG_ERR level.
 * - Assume that the target directory already exists.
 */

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <string> <file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *text = argv[1];
    const char *filepath = argv[2];

    /* Initialize syslog using LOG_USER facility */
    openlog("writer", LOG_PID | LOG_CONS, LOG_USER);

    /* Log the intent to write */
    syslog(LOG_DEBUG, "Writing %s to %s", text, filepath);

    /* Open the file for writing.
     * Use "w" mode which truncates an existing file or creates a new one.
     */
    FILE *file = fopen(filepath, "w");
    if (file == NULL) {
        syslog(LOG_ERR, "Error opening file %s for writing", filepath);
        perror("fopen");
        closelog();
        return EXIT_FAILURE;
    }

    /* Write the input text to the file */
    if (fprintf(file, "%s", text) < 0) {
        syslog(LOG_ERR, "Error writing to file %s", filepath);
        fclose(file);
        closelog();
        return EXIT_FAILURE;
    }

    /* Close the file, checking for potential errors */
    if (fclose(file) != 0) {
        syslog(LOG_ERR, "Error closing file %s", filepath);
        closelog();
        return EXIT_FAILURE;
    }

    /* Close the syslog connection */
    closelog();

    return EXIT_SUCCESS;
}

