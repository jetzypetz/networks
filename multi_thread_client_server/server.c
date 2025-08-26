#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <netdb.h>

#define MESSAGE_SIZE 5000

int main(int argc, char ** argv) {
    int receive_status;
    int send_status;
    int bind_status;
    if (argc < 2) {
        fprintf(stderr, "usage: ./server <PORT>\n");
        return -1;
    }

    char message_string[MESSAGE_SIZE + 1];

    struct sockaddr_in client_address = {0};
    struct sockaddr_in server_address = {0};
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(atoi(argv[1]));
    server_address.sin_addr.s_addr = INADDR_ANY;

    int socketfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socketfd == -1) {
        perror("failed to create socket");
        return -1;
    }

    // bind
    bind_status = bind(socketfd, (struct sockaddr *) &server_address, sizeof(struct sockaddr));
    if (bind_status) {
        fprintf(stderr, "failed to bind\n");
        close(socketfd);
        return -1;
    }

    while (1) {
        // receive
        socklen_t len = sizeof(struct sockaddr);
        receive_status = recvfrom(socketfd, message_string, MESSAGE_SIZE + 1,
                0 , (struct sockaddr *) &client_address, &len);
        if (receive_status <= 0) {
            fprintf(stderr, "failed to receive\n");
            close(socketfd);
            return -1;
        }
        message_string[receive_status] = '\0';

        // send back
        send_status = sendto(socketfd, message_string, strlen(message_string),
                0, (struct sockaddr *) &client_address, sizeof(struct sockaddr));
        if (send_status <= 0) {
            fprintf(stderr, "failed to send\n");
            close(socketfd);
            return -1;
        }
        printf("sent: %s", message_string);
    }
    return 0;
}
