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
struct sockaddr_in client[3] = {0}; // incoming messages go to 0, 1 for player 1 and 2 for player 2

char message[MESSAGE_SIZE + 1] = {0};

// helper function
int check_win(char * grid);
int is_equal(struct sockaddr_in * addr1, struct sockaddr_in * addr2);
void display_game_state(char * game_state);

int main(int argc, char ** argv) {
    // setup
    socklen_t from_len = sizeof(struct sockaddr_in);
    int n = 0;
    int turn = 1; // 1 for player 1, 2 for player 2
    int winner = 0;
    int players = 0;
    int is_player = 0;

    int port = 0;
    int receive_status = 0;

    char grid[10] = {0};

    // messages
    char game_state[30] = {0};
    game_state[0] = FYI;

    char invite_string[28] = "_Welcome, you are player _!";
    invite_string[0] = TXT;

    char end_string[3] = "__";
    end_string[0] = END;

    char move_request_string[2] = "_";
    move_request_string[0] = MYM;

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
                0, (struct sockaddr *) &client[players + 1], &from_len);
        if (receive_status <= 0) {
            perror("failed to receive");
            close(socketfd);
            return receive_status;
        }
        message[receive_status] = '\0';

        if ((message[0] != TXT) || (receive_status < 2)) {
            printf("got strange message\n");
            continue;
        }

        // player approved
        players++;

        printf("received: %s\n", message);

        invite_string[25] = players + 48;
            
        if (sendto(socketfd, invite_string, 28,
                0, (struct sockaddr *) &client[players], sizeof(client[players])) <= 0) {
            perror("failed to send");
            close(socketfd);
            return -1;
        }
    }

    // game loop
    while (!winner) {
        if (sendto(socketfd, game_state, 30, 0, (struct sockaddr *) &client[turn], sizeof(client[turn])) <= 0) {
            perror("failed to send");
            close(socketfd);
            return -1;
        }

        if (sendto(socketfd, move_request_string, 2, 0, (struct sockaddr *) &client[turn], sizeof(client[turn])) <= 0) {
            perror("failed to send");
            close(socketfd);
            return -1;
        }
        
        // resolve unknown clients and not-your-turn
        while (1) {
            receive_status = recvfrom(socketfd, message, MESSAGE_SIZE + 1,
                    0, (struct sockaddr *) &client[0], &from_len);
            if (receive_status <= 0) {
                perror("failed to receive");
                close(socketfd);
                return receive_status;
            }
            message[receive_status] = '\0';

            is_player = is_equal(&client[0], &client[1]) +
                    2 * is_equal(&client[0], &client[2]);
            if (!is_player) {
                end_string[1] = 255;
                if (sendto(socketfd, end_string, 3, 0,
                            (struct sockaddr *) &client[0], sizeof(client[0])) <= 0) {
                    perror("failed to send");
                    close(socketfd);
                    return -1; }
            } else if (is_player == turn) {
                break;
            }
        }

        // move validity
        n = game_state[1];

        if ((message[0] != MOV)
                || (message[1] < 0)
                || (message[1] > 2)
                || (message[2] < 0)
                || (message[2] > 2)
           ) {
            printf("invalid move by player %d on move %d. wrong format\n", is_player, n + 1);
            winner = is_player ^ 3;
        }

        for (int i=0;i<n;i++) {
            if ((game_state[3 * i + 3] == message[1])
                    && (game_state[3 * i + 4] == message[2])) {
                printf("invalid move by player %d on move %d. space already taken\n", is_player, n + 1);
                winner = is_player ^ 3;
            }
        }

        // move approved
        game_state[3 * n + 2] = is_player;
        game_state[3 * n + 3] = message[1];
        game_state[3 * n + 4] = message[2];
        game_state[1] = n + 1;
        turn ^= 3;
        as_grid(game_state, grid);
        display_game_state(game_state);
        printf("grid byte for byte:\n");
        for (int i=0;i<9;i++) {
            printf("%c|", grid[i] + 48);
        }
        printf("\n");
        display_grid(grid);

        // check win
        if (as_grid(game_state, grid)) {
            fprintf(stderr, "failed as_grid\n");
            return -1;
        }

        if (check_win(grid)) {
            winner = is_player;
        }
    }
    printf("found winner!\n");
    return 0;
}

void display_game_state(char * game_state) {
    printf("game_state: |");
    for (int i=0;i<30;i++) {
        printf("%c|", game_state[i] + 48);
    }
    printf("\n");
}

int check_win(char * grid) {
    int row = 0;
    int col = 0;

    // horizontal wins
    for (row=0;row<3;row++) {
        if (grid[3*row] && grid[3*row] == grid[3*row + 1] && grid[3*row] == grid[3*row + 2]) {
            return grid[3*row];
        }
    }

    // vertical wins
    for (col=0;col<3;col++) {
        if (grid[col] && grid[col] == grid[col + 3] && grid[col] == grid[col + 6]) {
            return grid[col];
        }
    }

    // diagonal wins
    if (grid[0] && grid[0] == grid[4] && grid[0] == grid[8]) {
        return grid[0];
    }
    if (grid[2] && grid[2] == grid[4] && grid[2] == grid[6]) {
        return grid[2];
    }

    return 0;
}

int is_equal(struct sockaddr_in * addr1, struct sockaddr_in * addr2) {
    return (addr1->sin_port == addr2->sin_port) && (addr1->sin_addr.s_addr == addr2->sin_addr.s_addr);
}













