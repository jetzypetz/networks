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
        fprintf(stderr, "usage: ./server <PORT>\n");
        return -1;
    }

    http_port = ascii_to_int(argv[1], strlen(argv[1]));
    if (http_port <= 0 || http_port > 65535) {
        fprintf(stderr, "invalid server port (%d)\n", http_port);
        return -1;
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
    int udp_port = 0;
    struct sockaddr_in udp_address = {0};

    char http_request[SIZE_HTTP_REQUEST] = {0};
    char http_response[SIZE_HTTP_RESPONSE] = {0};
    char http_error[SIZE_HTTP_RESPONSE] = {0};
    char * filename = NULL;
    char * ip_address_string = NULL;
    char * udp_port_string = NULL;
    char * start_byte_string = NULL;
    char * nbytes_string = NULL;

    long file_size = 0;
    long parsed = 0;

    long chunk_size = 0;
    uint8_t sendbuff[SIZE_SENDBUFF] = {0};

    // receive http message
    int http_recvlen = recv(client->sockfd, http_request, SIZE_HTTP_REQUEST, 0);
    if (http_recvlen == -1) {
        perror("failed to receive");
        cleanup(udp_sockfd, client->sockfd, client);
        pthread_exit(NULL);
    }

    // parse message
    if (strncmp(http_request, "GET /sendfile/", 14)) {
        fprintf(stderr, "bad http request\n");
        fprintf(stderr, "message:\n%s\n", http_request);
        fprintf(stderr, "sending error response\n");

        snprintf(http_error, SIZE_HTTP_RESPONSE, "HTTP/1.1 400 ERROR\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 40\r\n\r\n"
                "ERROR badly formatted request (problem: http command)\r\n");
        write(client->sockfd, http_error, strlen(http_error));

        cleanup(udp_sockfd, client->sockfd, client);
        pthread_exit(NULL);
    }
    filename = http_request + 8;
    memcpy(http_request + 8, "files/", 6);

    if ((ip_address_string = strstr(filename + 6, "/")) == NULL) {
        fprintf(stderr, "bad http request (ip)\n");
        fprintf(stderr, "message:\n%s", http_request);

        snprintf(http_error, SIZE_HTTP_RESPONSE, "HTTP/1.1 400 ERROR\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 40\r\n\r\n"
                "ERROR badly formatted request (problem: ip address)\r\n");
        write(client->sockfd, http_error, strlen(http_error));

        cleanup(udp_sockfd, client->sockfd, client);
        pthread_exit(NULL);
    }
    
    *ip_address_string = '\0';
    ip_address_string += 1;
    
    if ((udp_port_string = strstr(ip_address_string, "/")) == NULL) {
        fprintf(stderr, "bad http request (port)\n");
        fprintf(stderr, "message:\n%s", http_request);

        snprintf(http_error, SIZE_HTTP_RESPONSE, "HTTP/1.1 400 ERROR\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 40\r\n\r\n"
                "ERROR badly formatted request (problem: port)\r\n");
        write(client->sockfd, http_error, strlen(http_error));

        cleanup(udp_sockfd, client->sockfd, client);
        pthread_exit(NULL);
    }
    *udp_port_string = '\0';
    udp_port_string += 1;
    
    udp_port = ascii_to_int(udp_port_string, 5);

    printf("opening file: %s\n", filename);

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
    printf("file size: %ld\n", file_size);

    snprintf(http_response, SIZE_HTTP_RESPONSE, "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 20\r\n\r\n"
            "OK %ld\r\n", file_size);

    write(client->sockfd, http_response, strlen(http_response));

    uint8_t filebuff[file_size];
    fread(filebuff, 1, file_size, file);

    udp_address.sin_family = AF_INET;
    udp_address.sin_port = htons(udp_port);
    udp_address.sin_addr.s_addr = client->address.sin_addr.s_addr;
        
    // send file

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


    // listen to retransmission requests for 5 seconds

    set_nonblock(client->sockfd);

    time_t waiting = time(NULL);
    while (difftime(time(NULL), waiting) <= 5.0) {
        http_recvlen = recv(client->sockfd, http_request, SIZE_HTTP_REQUEST, 0);

        if (http_recvlen > 0) {
            // parse message
            if (strncmp(http_request, "GET /retransmit/", 16)) {
                fprintf(stderr, "bad http request\n");
                fprintf(stderr, "message:\n%s", http_request);

                snprintf(http_error, SIZE_HTTP_RESPONSE, "HTTP/1.1 400 ERROR\r\n"
                        "Content-Type: text/plain\r\n"
                        "Content-Length: 40\r\n\r\n"
                        "ERROR badly formatted request (problem: http command)\r\n");
                write(client->sockfd, http_error, strlen(http_error));

                fclose(file);
                cleanup(udp_sockfd, client->sockfd, client);
                pthread_exit(NULL);
            }
            filename = http_request + 16;

            if ((ip_address_string = strstr(filename, "/")) == NULL) {
                fprintf(stderr, "bad http request (ip)\n");
                fprintf(stderr, "message:\n%s", http_request);

                snprintf(http_error, SIZE_HTTP_RESPONSE, "HTTP/1.1 400 ERROR\r\n"
                        "Content-Type: text/plain\r\n"
                        "Content-Length: 40\r\n\r\n"
                        "ERROR badly formatted request (problem: ip)\r\n");
                write(client->sockfd, http_error, strlen(http_error));

                fclose(file);
                cleanup(udp_sockfd, client->sockfd, client);
                pthread_exit(NULL);
            }
            
            *ip_address_string = '\0';
            ip_address_string += 1;
            
            if ((udp_port_string = strstr(ip_address_string, "/")) == NULL) {
                fprintf(stderr, "bad http request (port)\n");
                fprintf(stderr, "message:\n%s", http_request);

                snprintf(http_error, SIZE_HTTP_RESPONSE, "HTTP/1.1 400 ERROR\r\n"
                        "Content-Type: text/plain\r\n"
                        "Content-Length: 40\r\n\r\n"
                        "ERROR badly formatted request (problem: port)\r\n");
                write(client->sockfd, http_error, strlen(http_error));

                fclose(file);
                cleanup(udp_sockfd, client->sockfd, client);
                pthread_exit(NULL);
            }
            *udp_port_string = '\0';
            udp_port_string += 1;
            
            if ((start_byte_string = strstr(udp_port_string, "/")) == NULL) {
                fprintf(stderr, "bad http request (start byte)\n");
                fprintf(stderr, "message:\n%s", http_request);

                snprintf(http_error, SIZE_HTTP_RESPONSE, "HTTP/1.1 400 ERROR\r\n"
                        "Content-Type: text/plain\r\n"
                        "Content-Length: 40\r\n\r\n"
                        "ERROR badly formatted request (problem: start byte)\r\n");
                write(client->sockfd, http_error, strlen(http_error));

                fclose(file);
                cleanup(udp_sockfd, client->sockfd, client);
                pthread_exit(NULL);
            }
            *start_byte_string = '\0';
            start_byte_string  += 1;

            if ((nbytes_string = strstr(start_byte_string, "/")) == NULL) {
                fprintf(stderr, "bad http request (byte size)\n");
                fprintf(stderr, "message:\n%s", http_request);
                fclose(file);
                cleanup(udp_sockfd, client->sockfd, client);
                pthread_exit(NULL);
            }
            *nbytes_string = '\0';
            nbytes_string  += 1;

            parsed = ascii_to_int(start_byte_string, strlen(start_byte_string));
            chunk_size = ascii_to_int(nbytes_string, strlen(nbytes_string));
            
            if (parsed + chunk_size > file_size) {
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
        }
    }
    fclose(file);
    cleanup(udp_sockfd, client->sockfd, client);
    pthread_exit(NULL);
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
