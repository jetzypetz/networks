#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <time.h>

#include "file_share.h"

typedef struct {
    int sockfd;
    struct sockaddr_in address;
    pthread_t thread;
} client_data_t;
    
char src[60] = {0};
int src_size = 0;

int parse_message(char * http_request, char * filename,
        int filename_size, struct sockaddr_in * udp_address, long int * start_byte, long int * nbytes);
void * client_handler(void * client);
int cleanup(int sock1, int sock2, void * client);

int main(int argc, char ** argv) {
    // tcp/http

    int http_port = 0;
    int listen_sockfd = 0;
    int client_sockfd = 0;

    struct sockaddr_in http_client = {0};
    socklen_t http_client_len = sizeof(http_client);
    
    struct sockaddr_in http_address = {0};

    // args

    if (argc < 2) {
        fprintf(stderr, "usage: %s <port> [src]\n", argv[0]);
        return -1;
    }

    http_port = ascii_to_int(argv[1], strlen(argv[1]));
    if (http_port <= 0 || http_port > 65535) {
        fprintf(stderr, "invalid server port (%d)\n", http_port);
        return -1;
    }

    if (argc > 2) {
        src_size = strlen(argv[2]);
        strcpy(src, argv[2]);
        if (src[src_size - 1] != '/') {
            src[src_size] = '/';
            src_size++;
        }
    } else {
        strcpy(src, "files/");
        src_size = 6;
    }


    // tcp/http setup

    http_address.sin_family = AF_INET;
    http_address.sin_port = htons(http_port);
    http_address.sin_addr.s_addr = INADDR_ANY;

    if ((listen_sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("failed to connect tcp socket");
        return -1;
    }

    if (bind(listen_sockfd, (struct sockaddr *) &http_address, sizeof(http_address)) == -1) {
        perror("failed to bind tcp socket");
        close(listen_sockfd);
        return -1;
    }

    if (listen(listen_sockfd, 10) == -1) {
        perror("failed to listen");
        close(listen_sockfd);
        return -1;
    }

    // receiving loop
        
    while (1) {
        printf("accepting...\n");
        client_sockfd = accept(listen_sockfd, (struct sockaddr *) &http_client, &http_client_len);

        client_data_t * client = malloc(sizeof(client_data_t));
        if (!client) {
            perror("malloc failed");
            close(listen_sockfd);
            close(client_sockfd);
            return -1;
        }

        client->sockfd = client_sockfd;
        client->address = http_client;

        pthread_create(&client->thread, NULL, client_handler, (void *) client);
        pthread_detach(client->thread);
    }
}

void * client_handler(void * client_data) {
    client_data_t * client = (client_data_t *) client_data;
    int udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in udp_address = {0};

    char http_request[SIZE_HTTP_REQUEST] = {0};
    char http_response[SIZE_HTTP_RESPONSE] = {0};
    char http_error[SIZE_HTTP_RESPONSE] = {0};

    char * filename = malloc(MAX_FILENAME_SIZE);
    strncpy(filename, src, src_size);


    long file_size = 0;
    long parsed = 0;

    long start_byte = 0;
    long chunk_size = 0;
    uint8_t sendbuff[SIZE_SENDBUFF] = {0};

    // receive http message
    int http_status = 0;
    int http_recvlen = recv(client->sockfd, http_request, SIZE_HTTP_REQUEST, 0);
    if (http_recvlen == -1) {
        perror("failed to receive");
        cleanup(udp_sockfd, client->sockfd, client);
        pthread_exit(NULL);
    }

    // parse message
    http_status = parse_message(http_request, filename, MAX_FILENAME_SIZE - src_size,
            &udp_address, &start_byte, &chunk_size);

    if (http_status < 0) {
        fprintf(stderr, "bad http request\n");
        fprintf(stderr, "message:\n%s\n", http_request);
        fprintf(stderr, "sending error response\n");

        snprintf(http_error, SIZE_HTTP_RESPONSE, "HTTP/1.1 400 ERROR\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 40\r\n\r\n"
                "ERROR badly formatted request\r\n");
        write(client->sockfd, http_error, strlen(http_error));

        cleanup(udp_sockfd, client->sockfd, client);
        pthread_exit(NULL);
    }
    
    FILE * file = fopen(filename, "r");
    if (!file) {
        perror("failed to open file");

        snprintf(http_error, SIZE_HTTP_RESPONSE, "HTTP/1.1 400 ERROR\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 30\r\n\r\n"
                "ERROR File not found\r\n");
        write(client->sockfd, http_error, strlen(http_error));

        cleanup(udp_sockfd, client->sockfd, client);
        pthread_exit(NULL);
    }

    fseek(file, 0, SEEK_END);
    file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size >= SIZE_FILEBUFF) {
        perror("file too big");

        snprintf(http_error, SIZE_HTTP_RESPONSE, "HTTP/1.1 400 ERROR\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 30\r\n\r\n"
                "ERROR File too big\r\n");
        write(client->sockfd, http_error, strlen(http_error));

        fclose(file);
        cleanup(udp_sockfd, client->sockfd, client);
        pthread_exit(NULL);
    }

    snprintf(http_response, SIZE_HTTP_RESPONSE, "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 20\r\n\r\n"
            "OK %ld\r\n", file_size);

    write(client->sockfd, http_response, strlen(http_response));

    uint8_t filebuff[file_size];
    fread(filebuff, 1, file_size, file);
        

    if (http_status == 1) {

        // send whole file

        while (parsed < file_size) {
            if (parsed + SIZE_CHUNK > file_size) {
                chunk_size = file_size - parsed;
            } else {
                chunk_size = SIZE_CHUNK;
            }

            ((uint16_t *) sendbuff)[0] = htons((uint16_t) parsed);
            ((uint16_t *) sendbuff)[1] = htons((uint16_t) chunk_size);
            memcpy(sendbuff + 4, filebuff + parsed, chunk_size);

            if (sendto(udp_sockfd, sendbuff, chunk_size + 4, 0,
                        (struct sockaddr *) &udp_address, sizeof(udp_address)) == -1) {
                perror("failed to send udp package");

                snprintf(http_error, SIZE_HTTP_RESPONSE, "HTTP/1.1 400 ERROR\r\n"
                        "Content-Type: text/plain\r\n"
                        "Content-Length: 40\r\n\r\n"
                        "ERROR failed to send package\r\n");
                write(client->sockfd, http_error, strlen(http_error));

                fclose(file);
                cleanup(udp_sockfd, client->sockfd, client);
                pthread_exit(NULL);
            }

            parsed += chunk_size;
        }
        close(client->sockfd);

    } else if (http_status == 2) {

        // send requested packet

        if (start_byte + chunk_size > file_size) {
            fprintf(stderr, "bytes requested cross bounds of file\n");

            snprintf(http_error, SIZE_HTTP_RESPONSE, "HTTP/1.1 400 ERROR\r\n"
                    "Content-Type: text/plain\r\n"
                    "Content-Length: 40\r\n\r\n"
                    "ERROR bytes requested are out of bounds\r\n");
            write(client->sockfd, http_error, strlen(http_error));

            fclose(file);
            cleanup(udp_sockfd, client->sockfd, client);
            pthread_exit(NULL);
        }

        ((uint16_t *) sendbuff)[0] = htons((uint16_t) start_byte);
        ((uint16_t *) sendbuff)[1] = htons((uint16_t) chunk_size);
        memcpy(sendbuff + 4, filebuff + start_byte, chunk_size);

        if (sendto(udp_sockfd, sendbuff, chunk_size + 4, 0,
                    (struct sockaddr *) &udp_address, sizeof(udp_address)) == -1) {
            perror("failed to send udp package");

            snprintf(http_error, SIZE_HTTP_RESPONSE, "HTTP/1.1 400 ERROR\r\n"
                    "Content-Type: text/plain\r\n"
                    "Content-Length: 40\r\n\r\n"
                    "ERROR failed to send package\r\n");
            write(client->sockfd, http_error, strlen(http_error));

            fclose(file);
            cleanup(udp_sockfd, client->sockfd, client);
            pthread_exit(NULL);
        }
    }

    fclose(file);
    cleanup(udp_sockfd, client->sockfd, client);
    pthread_exit(NULL);
}

int parse_message(char * http_request, char * filename,
        int filename_size, struct sockaddr_in * udp_address, long int * start_byte, long int * nbytes) {

    char name[MAX_FILENAME_SIZE] = {0};
    char ip[15] = {0};
    int port;

    if (sscanf(http_request, "GET /sendfile/%256[^/]/%15[^/]/%d HTTP/1.1", name, ip, &port) == 3) {

        if (strlen(filename) >= MAX_FILENAME_SIZE - src_size) {
            return -1;
        }
        strcat(filename, name);

        if (inet_pton(AF_INET, ip, &udp_address->sin_addr) != 1) {
            return -3;
        }

        if (port <=0 || port > 65535) {
            return -4;
        }
        udp_address->sin_port = htons(port);

        return 1;

    } else if (sscanf(http_request, "GET /retransmit/%256[^/]/%15[^/]/%d/%ld/%ld HTTP/1.1",
                name, ip, &port, start_byte, nbytes) == 5) {

        if (strlen(filename) >=MAX_FILENAME_SIZE - src_size) {
            return -1;
        }

        strcat(filename, name);

        if (inet_pton(AF_INET, ip, &udp_address->sin_addr) != 1) {
            return -3;
        }

        if (port <=0 || port > 65535) {
            return -4;
        }
        udp_address->sin_port = htons(port);

        return 2;

    }

    return -1;

}

int cleanup(int sock1, int sock2, void * client) {
    if (sock1) {
        if (close(sock1)) {
            perror("failed to close udp socket");
        }
    }
    if (sock2) {
        if (close(sock2)) {
            perror("failed to close udp socket");
        }
    }
    if (client) {
        free(client);
    }
    return -1;
}
