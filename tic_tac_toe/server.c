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

// shared variables
int socketfd = 0;
struct sockaddr_in server = {0};
struct sockaddr_in incoming = {0};
struct sockaddr_in client[2] = {0};

char message[MESSAGE_SIZE + 1] = {0};

// helper function
int send_move_request(int turn);

int main(int argc, char ** argv) {
    // setup
    int n = 0;
    int turn = 1; // 1 for player 1, 2 for player 2
    int winner = 0;
    int players = 0;
    int got_move = 0;
    int is_player = 0;

    int port = 0;
    int receive_status = 0;

    char game_state[30] = {0};
    game_state[0] = FYI;

    char invite_string[28] = "_Welcome, you are player _!";
    invite_string[0] = TXT;

    char end_string[3] = "__";
    end_string[0] = END;

    if (argc < 2) {
        fprintf(stderr, "usage: ./server <PORT>\n");
        return -1;
    }

    if ((port = atoi(argv[1])) == 0) {
        fprintf(stderr, "invalid port\n");
        return -1;
    }

    // connect
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = INADDR_ANY;

    if ((socketfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("failed to create socket");
        return -1;
    }

    if (bind(socketfd, (struct sockaddr *) &server, sizeof(server))) {
        perror("failed to bind");
        close(socketfd);
        return -1;
    }

    // connect players
    while (players != 2) {
        receive_status = recvfrom(socketfd, message, MESSAGE_SIZE + 1,
                0, (struct sockaddr *) &client[players], NULL);
        if (receive_status <= 0) {
            perror("failed to receive");
            close(socketfd);
            return receive_status;
        }
        message[receive_status] = '\0';
        
        if ((message[0] != TXT) || (strlen(message) < 2)) { // ignore weird messages
            printf("got strange message\n");
            continue;
        }

        invite_string[25] = ++players;
            
        if (sendto(socketfd, invite_string, 28,
                0, (struct sockaddr *) &client[players - 1], sizeof(client[players - 1])) <= 0) {
            perror("failed to send");
            close(socketfd);
            return -1;
        }
    }

    // game loop
    while (!winner) {
        send_move_request(turn); // TODO
        
        // resolve unknown clients, not-your-turn, and incorrect messages
        while (1) {
            receive_status = recvfrom(socketfd, message, MESSAGE_SIZE + 1,
                    0, (struct sockaddr *) &incoming, NULL);
            if (receive_status <= 0) {
                perror("failed to receive");
                        close(socketfd);
                return receive_status;
            }
            message[receive_status] = '\0';

            is_player = is_equal(&incoming, &client[0]) + // TODO
                    2 * is_equal(&incoming, &client[1]);
            if (!is_player) {
                end_string[1] = 255;
                if (sendto(socketfd, end_string, 3, 0,
                            (struct sockaddr *) &incoming, sizeof(incoming)) <= 0) {
                    perror("failed to send");
                    close(socketfd);
                    return -1;
                }
            } else if (is_player == turn) {
                break;
            }
        }

        // move validity
        if ((message[0] != MOV)
                || (message[1] < 0)
                || (message[1] > 2)
                || (message[2] < 0)
                || (message[2] > 2)
           ) {
            printf("invalid move by player %d\n", is_player);
            winner = is_player ^ 3;
        }

        n = game_state[1];
        for (int i=0;i<n;i++) {
            if ((game_state[3 * i + 3] == message[1])
                    || (game_state[3 * i + 4] == message[2])) {
                printf("invalid move by player %d\n", is_player);
                winner = is_player ^ 3;
            }
        }

        // add move
        game_state[3 * n + 2] = is_player;
        game_state[3 * n + 3] = message[1];
        game_state[3 * n + 4] = message[2];
        game_state[1] = n + 1;
        turn ^= 3;

        // check win
        

    }
    return 0;
}
