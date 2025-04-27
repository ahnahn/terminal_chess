/* server.c: Chess server handling two clients over TCP */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include "chess.h"

#define PORT 5000       /* Server port */
#define BUF_SIZE 256

int client_sock[2];     /* client sockets for WHITE=0, BLACK=1 */
GameState game;         /* Shared game state */

pthread_mutex_t game_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t turn_cond = PTHREAD_COND_INITIALIZER;

/* Thread data */
typedef struct {
    int color;         /* 0 for White, 1 for Black */
} ThreadData;

/* Broadcast the current board to both clients */
void broadcast_board() {
    char buf[BUF_SIZE];
    /* Generate board ASCII into buffer */
    /* For simplicity, we print to stdout on server and send same */
    printf("Current board:\n");
    print_board(&game);
    /* Send board as text to clients */
    for(int i=0; i<2; i++) {
        FILE *fp = fdopen(client_sock[i], "w");
        if(!fp) continue;
        fprintf(fp, "\n");
        fprintf(fp, "  a b c d e f g h\n");
        for(int r = 0; r < BOARD_SIZE; r++) {
            fprintf(fp, "%d ", BOARD_SIZE - r);
            for(int c = 0; c < BOARD_SIZE; c++) {
                fprintf(fp, "%c ", game.board[r][c]);
            }
            fprintf(fp, "%d\n", BOARD_SIZE - r);
        }
        fprintf(fp, "  a b c d e f g h\n");
        fflush(fp);
    }
}

/* Handle a client (White or Black) */
void *client_thread(void *arg) {
    ThreadData *td = (ThreadData*)arg;
    int me = td->color;
    int other = 1 - me;
    char buf[BUF_SIZE];

    /* Assign color */
    FILE *client_fp = fdopen(client_sock[me], "r+");
    if(me == WHITE) {
        fprintf(client_fp, "You are WHITE. Waiting for Black...\n");
    } else {
        fprintf(client_fp, "You are BLACK. Starting game...\n");
    }
    fflush(client_fp);

    /* Wait for both clients ready */
    if(me == BLACK) {
        /* When Black connects, broadcast initial board */
        broadcast_board();
    }

    /* Game loop */
    while (1) {
        pthread_mutex_lock(&game_mutex);
        /* Wait for our turn */
        while (game.turn != me) {
            /* Check for end of game */
            if (game.turn == -1) {
                pthread_mutex_unlock(&game_mutex);
                goto game_end;
            }
            pthread_cond_wait(&turn_cond, &game_mutex);
        }
        /* It's our turn now */
        /* Check for checkmate or stalemate */
        if (is_in_check(&game, me)) {
            if (!has_valid_moves(&game, me)) {
                /* Checkmate */
                broadcast_board();
                if(me == WHITE) {
                    fprintf(client_fp, "Checkmate! BLACK wins.\n");
                } else {
                    fprintf(client_fp, "Checkmate! WHITE wins.\n");
                }
                fflush(client_fp);
                game.turn = -1;
                pthread_cond_signal(&turn_cond);
                pthread_mutex_unlock(&game_mutex);
                break;
            }
        } else {
            if (!has_valid_moves(&game, me)) {
                /* Stalemate */
                broadcast_board();
                fprintf(client_fp, "Stalemate! Game is a draw.\n");
                fflush(client_fp);
                game.turn = -1;
                pthread_cond_signal(&turn_cond);
                pthread_mutex_unlock(&game_mutex);
                break;
            }
        }
        /* Prompt for move */
        fprintf(client_fp, "Your move: ");
        fflush(client_fp);
        pthread_mutex_unlock(&game_mutex);

        /* Read move from client */
        if (fgets(buf, sizeof(buf), client_fp) == NULL) {
            perror("Client disconnected");
            exit(1);
        }
        /* Remove newline */
        buf[strcspn(buf, "\r\n")] = '\0';

        pthread_mutex_lock(&game_mutex);
        int sr, sc, dr, dc;
        if (!parse_move(buf, &sr, &sc, &dr, &dc)) {
            fprintf(client_fp, "Invalid input format. Use e2e4, etc.\n");
        } else {
            if (!make_move(&game, sr, sc, dr, dc)) {
                fprintf(client_fp, "Invalid move. Try again.\n");
            } else {
                /* Move applied, switch turn */
                broadcast_board();
                game.turn = other;
                pthread_cond_signal(&turn_cond);
            }
        }
        fflush(client_fp);
        pthread_mutex_unlock(&game_mutex);
    }

game_end:
    return NULL;
}

int main() {
    int server_sock;
    struct sockaddr_in serv_addr;
    printf("Starting Chess server on port %d...\n", PORT);

    /* Initialize game */
    init_board(&game);

    /* Setup TCP socket */
    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket");
        exit(1);
    }
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(server_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Bind");
        exit(1);
    }
    listen(server_sock, 2);
    printf("Waiting for two players to connect...\n");

    /* Accept two clients */
    for(int i=0; i<2; i++) {
        client_sock[i] = accept(server_sock, NULL, NULL);
        if (client_sock[i] < 0) {
            perror("Accept");
            exit(1);
        }
        printf("Client %d connected.\n", i);
    }

    /* Launch client threads */
    pthread_t th[2];
    ThreadData td[2];
    for(int i=0; i<2; i++) {
        td[i].color = i;  /* White=0, Black=1 */
        if(pthread_create(&th[i], NULL, client_thread, &td[i]) != 0) {
            perror("pthread_create");
            exit(1);
        }
    }

    /* Wait for threads to finish */
    for(int i=0; i<2; i++) {
        pthread_join(th[i], NULL);
    }
    printf("Game over. Server shutting down.\n");
    close(server_sock);
    return 0;
}
