#ifndef MESSAGE_H
#define MESSAGE_H

#define FYI 0x01
#define MYM 0x02
#define END 0x03
#define TXT 0x04
#define MOV 0x05
#define LFT 0x06

#define MESSAGE_SIZE 500

#define DRAW 0x00
#define WIN1 0x01
#define WIN2 0x02
#define NO_SPACE 0x0f

int as_grid(char * game_state, char * grid) {
    memset(grid, 0, 9);

    for (int parse=0; parse < game_state[1]; parse++) {
        grid[3*game_state[3*parse + 4] + game_state[3*parse + 3]] = game_state[3*parse + 2];
    }
    return 0;
}

int display_grid(char * grid) {
    // display
    for (int i=0;i<3;i++) {
        for (int j=0;j<3;j++) {
            switch (grid[i*3 + j]) {
                case 0x00:
                    printf(" ");
                    break;
                case 0x01:
                    printf("X");
                    break;
                case 0x02:
                    printf("O");
                    break;
                default:
                    printf("\n\nError\n");
                    return -1;
            }
            if (j != 2) {
                printf("|");
            } else {
                if (i != 2) {
                    printf("\n-+-+-\n");
                } else {
                    printf("\n");
                }
            }
        }
    }
    return 0;
}

#endif
