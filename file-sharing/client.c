#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <errno.h>
#include <time.h>

#include "file_share.h"

// helper functions

int parse_http_response(char * http_response, int http_recvlen);
int missing_data(int * data_info, int data_npairs, int file_size,
        int * missing_data_start, int * missing_data_nbytes);
void display_data_info(int * data_info, int data_npairs);
int add_data(int * data_info, int data_npairs, int start, int nbytes);
int cleanup(int udp_sockfd, int http_sockfd);

int main(int argc, char ** argv) {
    // udp

    int udp_port = 0;
    int udp_sockfd = 0;
    int udp_recvlen = 0;

    struct sockaddr_in udp_address = {0}; // personal receiving address
    in_addr_t my_ip_address = {0};
    char my_ip_address_string[20] = {0};

    char recvbuff[SIZE_RECVBUFF] = {0};
    int recv_start_byte = 0;
    int recv_nbytes = 0;

    int file_size = 0;
    char filebuff[SIZE_FILEBUFF] = {0};

    int data_info[(2 * SIZE_FILEBUFF/SIZE_RECVBUFF) + 2] = {0};
    int data_npairs = 0;
    int missing_data_start = 0;
    int missing_data_nbytes = 0;
    time_t locked = time(NULL);
    int consecutive_failed_requests = 0;

    // tcp/http

    int http_port = 0;
    int http_sockfd = 0;
    int http_recvlen = 0;

    struct sockaddr_in http_server_address = {0}; // server forwarding address
    in_addr_t http_server_ip_address = {0};

    char http_file_request[SIZE_HTTP_REQUEST] = {0};
    char http_retransmit_request[SIZE_HTTP_REQUEST] = {0};
    char http_response[SIZE_HTTP_RESPONSE] = {0};

    // args

    if (argc < 4) {
        fprintf(stderr, "usage: %s <server address> <server port> <filename> [receive port]\n", argv[0]);
        return -1;
    }

    if (inet_pton(AF_INET, argv[1], &http_server_ip_address) != 1) {
        fprintf(stderr, "invalid server address\n");
        return -1;
    }

    http_port = ascii_to_int(argv[2], strlen(argv[2]));
    if (http_port <= 0 || http_port > 65535) {
        fprintf(stderr, "invalid server port (%d)\n", http_port);
        return -1;
    }

    if (argc > 4) {
        udp_port = ascii_to_int(argv[4], strlen(argv[4]));
        if (udp_port <= 0 || udp_port > 65535) {
            fprintf(stderr, "bad receive port (%d), using default\n", udp_port);
            udp_port = 0;
        }
    }

    // udp setup

    udp_address.sin_family = AF_INET;
    udp_address.sin_port = htons(udp_port);
    udp_address.sin_addr.s_addr = INADDR_ANY;

    if ((udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("failed to connect udp socket");
        return -1;
    }
    
    if (set_nonblock(udp_sockfd) == -1) {
        return cleanup(udp_sockfd, http_sockfd);
    }

    if (udp_port != 0) {
        if (bind(udp_sockfd, (struct sockaddr *) &udp_address, sizeof(udp_address)) == -1) {
            if (errno == EADDRINUSE) {
                fprintf(stderr, "chosen receive port (%d) busy, using default\n", udp_port);
                udp_port = 0;
            } else {
                perror("failed to bind udp socket");
                return cleanup(udp_sockfd, http_sockfd);
            }
        }
    }

    if (udp_port == 0) {
        for (udp_port = 5000; udp_port < 5011; udp_port++) {
            udp_address.sin_port = htons(udp_port);
            if (bind(udp_sockfd, (struct sockaddr *) &udp_address, sizeof(udp_address)) == -1) {
                if (udp_port == 5010) {
                    perror("failed to bind all default receive ports (5000-5010)");
                    return cleanup(udp_sockfd, http_sockfd);
                } else if (errno == EADDRINUSE) {
                    fprintf(stderr, "default receive port (%d) busy, trying next one\n", udp_port);
                } else {
                    perror("failed to bind default receive port");
                    return cleanup(udp_sockfd, http_sockfd);
                }
            } else break;
        }
    }

    // tcp setup

    http_server_address.sin_family = AF_INET;
    http_server_address.sin_port = htons(http_port);
    http_server_address.sin_addr.s_addr = http_server_ip_address;

    if ((http_sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("failed to connect tcp socket");
        return cleanup(udp_sockfd, http_sockfd);
    }
                
    if (connect(http_sockfd, (struct sockaddr *) &http_server_address, sizeof(http_server_address)) == -1) {
        perror("failed to connect socket");
        return cleanup(udp_sockfd, http_sockfd);
    }

    // send http file request

    my_ip_address = INADDR_ANY;

    inet_ntop(AF_INET, &my_ip_address, my_ip_address_string, 20);

    snprintf(http_file_request, SIZE_HTTP_REQUEST,
            "GET /sendfile/%s/%s/%d HTTP/1.1\r\n\r\n", // \r\nHost: %s:%d
            argv[3], my_ip_address_string, udp_port); // , argv[1], http_port
    

    write(http_sockfd, http_file_request, strlen(http_file_request));

    if ((http_recvlen = recv(http_sockfd, http_response, SIZE_HTTP_RESPONSE, 0)) == -1) {
        perror("failed to receive");
        return cleanup(udp_sockfd, http_sockfd);
    }

    if (http_recvlen == 0) {
        fprintf(stderr, "the server has closed the connection\n");
        return cleanup(udp_sockfd, http_sockfd);
    }
        
    
    // check http response

    if ((file_size = parse_http_response(http_response, http_recvlen)) == -1) {
        return cleanup(udp_sockfd, http_sockfd);
    }

    if (file_size == 0) {
        fprintf(stderr, "file size appears to be zero\n");
        return cleanup(udp_sockfd, http_sockfd);
    }

    // receive and analyze udp packets

    while (missing_data(data_info, data_npairs, file_size, &missing_data_start, &missing_data_nbytes)) {
        while ((udp_recvlen = recv(udp_sockfd, recvbuff, SIZE_RECVBUFF, 0)) <= 0) { // socket not blocking
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                fprintf(stderr, "file receive failed\n");
                return cleanup(udp_sockfd, http_sockfd);
            }

            if (difftime(time(NULL), locked) >= 4) {
                fprintf(stderr, "sending retransmission request...\n");
                if (consecutive_failed_requests++ == 5) {
                    fprintf(stderr, "sent 5 consecutive requests with no reply\n");
                    return cleanup(udp_sockfd, http_sockfd);
                }
                
                snprintf(http_retransmit_request, SIZE_HTTP_REQUEST,
                        "GET /retransmit/%s/%s/%d/%d/%d HTTP/1.1\r\n\r\n", // \r\nHost: %s:%d
                        argv[3], my_ip_address_string, udp_port, missing_data_start, missing_data_nbytes); // , argv[1], http_port
    
                fprintf(stderr, "requesting missing data [%d|%d]\n", missing_data_start, missing_data_start + missing_data_nbytes);

                write(http_sockfd, http_retransmit_request, strlen(http_retransmit_request));

                if ((http_recvlen = recv(http_sockfd, http_response, SIZE_HTTP_RESPONSE, 0)) == -1) {
                    perror("failed to receive");
                    return cleanup(udp_sockfd, http_sockfd);
                }
                if (http_recvlen == 0) {
                    fprintf(stderr, "the server has closed the connection\n");
                    return cleanup(udp_sockfd, http_sockfd);
                }

                fprintf(stderr, "got message:\n%s", http_response);
                
                if ((file_size = parse_http_response(http_response, http_recvlen)) == -1) {
                    return cleanup(udp_sockfd, http_sockfd);
                }

                locked = time(NULL);
            }
        }
        consecutive_failed_requests = 0;

        recv_start_byte = ntohs(((uint16_t *) recvbuff)[0]);
        recv_nbytes = ntohs(((uint16_t *) recvbuff)[1]);

        fprintf(stderr, "got packet: start=%d, nbytes=%d, end=%d\n", recv_start_byte, recv_nbytes, recv_start_byte + recv_nbytes);

        if ((data_npairs = add_data(data_info, data_npairs, recv_start_byte, recv_nbytes)) == -1) {
            fprintf(stderr, "attempting to rewrite data\n");
            return cleanup(udp_sockfd, http_sockfd);
        }

        display_data_info(data_info, data_npairs);

        memcpy(filebuff + recv_start_byte, recvbuff + 4, recv_nbytes);
    }

    printf("writing %d buffered bytes\n", file_size);

    FILE * file = fopen(argv[3], "w");

    if (file == NULL) {
        fprintf(stderr, "failed to open file (%s)\n", argv[3]);
        return cleanup(udp_sockfd, http_sockfd);
    }

    fwrite(filebuff, 1, file_size, file);

    cleanup(udp_sockfd, http_sockfd);
    fclose(file);

    return 0;
}

int parse_http_response(char * http_response, int http_recvlen) {
    char * http_message = NULL;
    char * file_size_string = NULL;

    if (strncmp(http_response, "HTTP/1.1 200 OK", 15)) {
        fprintf(stderr, "received bad http response (of length %d)\n", http_recvlen);
        fprintf(stderr, "message: \n%s\n", http_response);
        return -1;
    }
    if ((http_message = strstr(http_response, "\r\n\r\n")) == NULL) {
        fprintf(stderr, "received garbled http response (of length %d). no message\n", http_recvlen);
        fprintf(stderr, "response: \n%s\n", http_response);
        return -1;
    }
    http_message += 4;


    if ((file_size_string = strstr(http_message, "OK ")) == NULL) {
        if (strstr(http_message, "ERROR") == NULL) {
            fprintf(stderr, "received garbled http message (of length %d)\n", http_recvlen);
            fprintf(stderr, "response: \n%s\n", http_response);
            return -1;
        }

        // ERROR
        if (strstr(http_message, "No such file") != NULL) {
            fprintf(stderr, "SERVER: error, no such file\n");
            fprintf(stderr, "message: \n%s\n",  http_message);
            return -1;
        } else if (strstr(http_message, "Internal error") != NULL) {
            fprintf(stderr, "SERVER: error, internal error\n");
            fprintf(stderr, "message: \n%s\n", http_message);
            return -1;
        } else {
            fprintf(stderr, "SERVER: error, unknown error\n");
            fprintf(stderr, "response: \n%s\n", http_response);
            return -1;
        }
    }
    file_size_string += 3;
    return ascii_to_int(file_size_string, strnlen(file_size_string, http_response + http_recvlen - file_size_string));
}

void display_data_info(int * data_info, int data_npairs) {
    printf("data info pairs (%d): ", data_npairs);
    for (int i = 0; i < data_npairs; i++) {
        printf("[%d|%d] ", data_info[2 * i], data_info[2 * i + 1]);
    }
    printf("\n");
}

int missing_data(int * data_info, int data_npairs, int file_size, int * missing_data_start, int * missing_data_nbytes) {
    if (data_npairs == 0) {
        *missing_data_start = 0;
        *missing_data_nbytes = file_size;
        return 1;
    }

    if (data_info[0] != 0) {
        *missing_data_start = 0;
        *missing_data_nbytes = data_info[0];
        return 1;
    }

    if (data_info[1] != file_size) {
        *missing_data_start = data_info[1];
        *missing_data_nbytes = (data_npairs > 1) ? data_info[2] : file_size;
        return 1;
    }

    return 0;
}

int add_data(int * data_info, int data_npairs, int start, int nbytes) {
    int end = start + nbytes;

    if (data_npairs == 0) {
        data_info[0] = start;
        data_info[1] = end;
        return 1;
    }

    int left_bound = 2 * data_npairs - 1;
    int right_bound = 0;

    while ((left_bound != -1) && (data_info[left_bound] > start)) {left_bound -= 2;}
    while ((right_bound != 2 * data_npairs) && (data_info[right_bound] < end)) {right_bound += 2;}

    if (right_bound != left_bound + 1) {
        fprintf(stderr, "error: overlapping packages\n");
        return -1;
    }

    if ((left_bound == -1) || (data_info[left_bound] < start)) {
        if ((right_bound == 2 * data_npairs) || (data_info[right_bound] > end)) {
            for (int worker = 2 * data_npairs - 1; worker >= right_bound; worker--) {
                data_info[worker + 2] = data_info[worker];
            }
            data_info[right_bound] = start;
            data_info[right_bound + 1] = end;
            return data_npairs + 1;
        } else {
            data_info[right_bound] = start;
            return data_npairs;
        }
    } else {
        if ((right_bound == 2 * data_npairs) || (data_info[right_bound] > end)) {
            data_info[left_bound] = end;
            return data_npairs;
        } else {
            for (int worker = right_bound + 1; worker < 2 * data_npairs; worker++) {
                data_info[worker - 2] = data_info[worker];
            }
            return data_npairs - 1;
        }
    }
}
            
int cleanup(int udp_sockfd, int http_sockfd) {
    if (udp_sockfd) {
        if (close(udp_sockfd)) {
            perror("failed to close udp socket");
        }
    }
    if (http_sockfd) {
        if (close(http_sockfd)) {
            perror("failed to close http socket");
        }
    }
    return -1;
}
