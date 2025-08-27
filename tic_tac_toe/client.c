#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <netdb.h>
#include "message.h"

// shared data
char pos[2] = {0};
char grid[9] = {0};

int socketfd = 0;
struct sockaddr_in server_address = {0};

// helper functions
int wait_move(char * message);
int display_end(char * message);

int main(int argc, char ** argv) {
    // setup
    int port = 0;
    int interrupt = 0;
    int receive_status = 0;

    char hello_string[8] = "_Hello!";
    hello_string[0] = TXT;

    char message[MESSAGE_SIZE + 1] = {0};

    if (argc < 3) {
        fprintf(stderr, "usage: ./client <ip address> <port number>\n");
        return -1;
    }

    if ((port = atoi(argv[2])) == 0) {
        fprintf(stderr, "invalid port\n");
        return -1;
    }
    
    if ((socketfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("failed to create socket");
        return -1;
    }

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    inet_pton(AF_INET, argv[1], &server_address.sin_addr.s_addr);

    // connect
    if (sendto(socketfd, hello_string, 8, 0,
                (struct sockaddr *) &server_address, sizeof(server_address)) == -1) {
        fprintf(stderr, "failed to send\n");
        close(socketfd);
        return -1;
    }

    // listen loop
    while (!interrupt) {
        receive_status = recvfrom(socketfd, message, MESSAGE_SIZE + 1, 0, NULL, NULL);
        if (receive_status == -1) {
            fprintf(stderr, "failed to receive\n");
            close(socketfd);
            return -1;
        }
        message[receive_status] = '\0';

        switch (message[0]) {
            case TXT:
                printf("[r] [TXT] (%d bytes)\n", receive_status);
                printf("%s\n", message + 1);
                break;
            case FYI:
                printf("[r] [FYI] (%d bytes)\n", receive_status);
                interrupt = as_grid(message, grid);
                interrupt += display_grid(grid);
                break;
            case MYM:
                printf("[r] [MYM] (%d bytes)\n", receive_status);
                interrupt = wait_move(message);
                break;
            case END:
                printf("[r] [END] (%d bytes)\n", receive_status);
                interrupt = display_end(message + 1);
                break;
            case NO_SPACE:
                printf("[r] [NO_SPACE] (%d bytes)\n", receive_status);
                fprintf(stderr, "no space for new players.\n");
                interrupt = -1;
            default:
                printf("[r] [ERR] (%d bytes)\n", receive_status);
                fprintf(stderr, "failed to read message\n");
                interrupt = -1;
        }
    }
    close(socketfd);
    return interrupt;
}

int wait_move(char * message) {
    memset(pos, 0, 2);

    printf("column: ");

    if (fgets(message, MESSAGE_SIZE + 1, stdin) == NULL) {
        perror("failed to read stdin");
        return -1;
    }

    switch (strlen(message)) {
        case 1:
            printf("client stopped\n");
            return 1;
        case 2:
            break;
        default:
            printf("invalid input (0,1 or 2)\n");
            return -1;
    }

    switch (message[0]) {
        case '0':
            pos[0] = 0x00;
            break;
        case '1':
            pos[0] = 0x01;
            break;
        case '2':
            pos[0] = 0x02;
            break;
        default:
            printf("invalid input (0,1 or 2)\n");
            return -1;
    }

    printf("row: ");

    if (fgets(message, MESSAGE_SIZE + 1, stdin) == NULL) {
        perror("failed to read stdin");
        return -1;
    }

    switch (strlen(message)) {
        case 1:
            printf("client stopped\n");
            return 1;
        case 2:
            break;
        default:
            printf("invalid input (0,1 or 2)\n");
            return -1;
    }

    switch (message[0]) {
        case '0':
            pos[1] = 0x00;
            break;
        case '1':
            pos[1] = 0x01;
            break;
        case '2':
            pos[1] = 0x02;
            break;
        default:
            printf("invalid input (0,1 or 2)\n");
            return -1;
    }

    message[0] = MOV;
    message[1] = pos[0];
    message[2] = pos[1];
    message[3] = '\0';

    if (sendto(socketfd, message, 4, 0,
                (struct sockaddr *) &server_address, sizeof(server_address)) == -1) {
        perror("failed to send");
        return -1;
    }

    return 0;
}

int display_end(char * message) {
    switch (message[0]) {
        case WIN1:
            printf("Player 1 won.\n");
            break;
        case WIN2:
            printf("Player 2 won.\n");
            break;
        case DRAW:
            printf("It was a draw.\n");
            break;
        default:
            fprintf(stderr, "failed on end\n");
            return -1;
    }
    return 1;
}
