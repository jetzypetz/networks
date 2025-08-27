#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>

#define SIZE_SENDBUFF 10240
#define SIZE_RECVBUFF 10240

int main(int argc, char ** argv) {
    int sockfd = 0;
    int recvlen = 0;
    struct hostent * hostinfo = {0};
    struct sockaddr_in target = {0};
    char recvbuff[SIZE_RECVBUFF];
    char sendbuff[SIZE_SENDBUFF];
    snprintf(sendbuff, SIZE_SENDBUFF,
            "GET / HTTP/1.1\r\n"
            "Host: %s\r\n"
            "\r\n",
            argv[1]);
    
    if (argc < 2) {
        fprintf(stderr, "usage: %s [hostname]\n", argv[0]);
        return -1;
    }

    if ((hostinfo = gethostbyname(argv[1])) == NULL) {
        perror("failed to get hostname");
        return -1;
    }

    target.sin_family = AF_INET;
    target.sin_port = htons(80);
    memcpy(&target.sin_addr.s_addr, hostinfo->h_addr, hostinfo->h_length);
    
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("failed to create socket");
        return -1;
    }
    
    if (connect(sockfd, (struct sockaddr *) &target, sizeof(struct sockaddr)) == -1) {
        perror("failed to connect socket");
        close(sockfd);
        return -1;
    }

    write(sockfd, sendbuff, strlen(sendbuff));

    if ((recvlen = recv(sockfd, recvbuff, SIZE_RECVBUFF, 0)) == -1) {
        perror("failed to receive");
        close(sockfd);
        return -1;
    }

    printf("%s\n", recvbuff);

    close(sockfd);
    return 0;
}
