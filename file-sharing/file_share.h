#ifndef FILE_SHARE_H
#define FILE_SHARE_H

#define MAX_FILENAME_SIZE 200
#define LOCK_TIMEOUT 2
#define SIZE_CHUNK 1400
#define SIZE_RECVBUFF 1500
#define SIZE_SENDBUFF 1500
#define SIZE_FILEBUFF 655350
#define SIZE_HTTP_REQUEST 100
#define SIZE_HTTP_RESPONSE 1000

#include <fcntl.h>
#include <stdio.h>

int ascii_to_int(const char *str, int len) { // this function is in part courtesy of ai
    if (len <= 0) {
        fprintf(stderr, "failed atoi\n");
        return -1;
    }
    int result = 0;
    while (*str >= '0' && *str <= '9' && len-- != 0) {
        result = result * 10 + (*str - '0');
        str++;
    }
    return result;
}

int set_nonblock(int socket_fd) { // this function is courtesy of ai
    int flags = fcntl(socket_fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl(F_GETFL) failed");
        return -1;
    }

    if (fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl(F_SETFL) failed");
        return -1;
    }

    return 0;
}

#endif
