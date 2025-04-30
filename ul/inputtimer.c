#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

/*---------------------------------------------------
    Prepared by Ben Burnham and Mateen Sharif
    EC523 Intro to Embedded Systems
    Final Project

    Usage:
     ./inputtimer read
        -> prints out the kernel driver status by reading /dev/myco2

     ./inputtimer co2time <val>
     ./inputtimer timeropen <val>
     ./inputtimer timerclose <val>
        -> writes that command to /dev/myco2
---------------------------------------------------*/

#define DEVICE_PATH "/dev/myco2"

int main(int argc, char *argv[])
{
    int fd;
    char buffer[256];
    ssize_t n;

    if (argc < 2) {
        fprintf(stderr, "Usage:\n");
        fprintf(stderr, "  %s read\n", argv[0]);
        fprintf(stderr, "  %s co2time <value>\n", argv[0]);
        fprintf(stderr, "  %s timeropen <value>\n", argv[0]);
        fprintf(stderr, "  %s timerclose <value>\n", argv[0]);
        return 1;
    }

    // If user typed "read", we just open /dev/myco2 (read-only) and print status
    if (strcmp(argv[1], "read") == 0) {
        fd = open(DEVICE_PATH, O_RDONLY);
        if (fd < 0) {
            perror("open for read");
            return 1;
        }
        n = read(fd, buffer, sizeof(buffer) - 1);
        if (n < 0) {
            perror("read");
            close(fd);
            return 1;
        }
        buffer[n] = '\0';
        printf("%s", buffer);
        close(fd);
        return 0;
    }

    // Otherwise, write one of the commands:
    // co2time <val>, timeropen <val>, timerclose <val>
    if (argc < 3) {
        fprintf(stderr, "Not enough arguments\n");
        return 1;
    }

    // Construct string
    snprintf(buffer, sizeof(buffer), "%s %s", argv[1], argv[2]);

    fd = open(DEVICE_PATH, O_WRONLY);
    if (fd < 0) {
        perror("open for write");
        return 1;
    }
    // Write the command
    n = write(fd, buffer, strlen(buffer));
    if (n < 0) {
        perror("write");
        close(fd);
        return 1;
    }
    close(fd);

    return 0;
}
