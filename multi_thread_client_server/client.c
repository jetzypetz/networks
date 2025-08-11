#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <netdb.h>

#define MESSAGE_SIZE 5000
#define NUMBER_OF_MESSAGES 500

int socketfd;
int thread_interrupt = 0;
int main_interrupt = 0;

struct sockaddr_in client_address = {0};
struct sockaddr_in server_address = {0};


void * receive_function();

int close_shop(int return_value, pthread_t * thread);

int main(int argc, char ** argv) {
    int port;
    pthread_t receive_thread;
    char message_out[MESSAGE_SIZE + 1] = {0};

    // arg error handling
    if (argc < 3) {
        fprintf(stderr, "usage: ./client <ip address> <port number>\n");
        return -1;
    }

    if ((port = atoi(argv[2])) == 0) {
        fprintf(stderr, "invalid port\n");
        return -1;
    }

    // addresses
    client_address.sin_family = AF_INET;
    client_address.sin_port = htons(0);
    client_address.sin_addr.s_addr = htonl(INADDR_ANY);

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    if (inet_pton(AF_INET, argv[1], &server_address.sin_addr.s_addr) <= 0) {
        perror("invalid address");
        close(socketfd);
        return -1;
    }

    // socket
    socketfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socketfd == -1) {
        perror("failed to create socket");
        return -1;
    }

    if (bind(socketfd, (struct sockaddr *) &client_address,
                sizeof(client_address)) == -1) {
        perror("failed to bind");
        close(socketfd);
        return -1;
    }

    // start listening
    pthread_create(&receive_thread, NULL, receive_function, NULL);

    // start accepting messages
    while (!main_interrupt) {
        printf("send: ");
        if (fgets(message_out, MESSAGE_SIZE + 1, stdin) == NULL) {
            printf("failed to read stdin\n");
            return close_shop(-1, &receive_thread);
        }

        if (strlen(message_out) == 0) {
            printf("client stopped\n");
            return close_shop(0, &receive_thread);
        }

        if ((sendto(socketfd, message_out, strlen(message_out), 0,
                (struct sockaddr *) &server_address, sizeof(server_address)))
                <= 0) {
            fprintf(stderr, "failed to send\n");
            return close_shop(-1, &receive_thread);
        }
    }
    
    return close_shop(-1, &receive_thread);
}

void * receive_function() {
    int receive_status;
    char message_in[MESSAGE_SIZE + 1] = {0};

    while (!thread_interrupt) {
        receive_status = recvfrom(socketfd, message_in,
                MESSAGE_SIZE + 1, 0, NULL, NULL);
        
        if (receive_status <= 0) {
            fprintf(stderr, "failed to receive\n");
            main_interrupt = 1;
            pthread_exit(NULL);
        }

        message_in[receive_status] = '\0';
        printf("received: %s", message_in);
    }

    pthread_exit(NULL);
}

int close_shop(int return_value, pthread_t * thread) {
    thread_interrupt = 1;
    pthread_join(*thread, NULL);
    close(socketfd);
    return return_value;
}
