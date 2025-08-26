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
    int send_status;
    int receive_status;
    socklen_t len = sizeof(struct sockaddr);
    char message_string[MESSAGE_SIZE + 1];

    if (argc < 3) {
        fprintf(stderr, "usage: ./client <ip address> <port number>\n");
        return -1;
    }
    int socketfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socketfd == -1) {
        perror("failed to create socket");
        return -1;
    }

    struct sockaddr_in server_address = {0};
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(atoi(argv[2]));
    inet_pton(AF_INET, argv[1], &server_address.sin_addr.s_addr);

    while (1) {
        printf("send: ");
        if (fgets(message_string, MESSAGE_SIZE + 1, stdin) == NULL) {
            printf("client stopped\n");
            close(socketfd);
            return 0;
        }

        send_status = sendto(socketfd, message_string, strlen(message_string),
                0, (struct sockaddr *) &server_address, len);
        if (send_status == 0) {
            fprintf(stderr, "failed to send\n");
            close(socketfd);
            return -1;
        }

        receive_status = recvfrom(socketfd, message_string, MESSAGE_SIZE + 1,
                0, (struct sockaddr *) &server_address, &len);
        if (receive_status <= 0) {
            fprintf(stderr, "failed to receive\n");
            close(socketfd);
            return -1;
        }
        message_string[receive_status] = '\0';
        printf("received: %s", message_string);
    }
}
