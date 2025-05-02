#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include "chess.h"
#include <locale.h>


#define PORT 5000
#define BUF_SIZE 256

int client_sock[2];     /* client sockets for WHITE=0, BLACK=1 */
GameState game;         /* Shared game state */

pthread_mutex_t game_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t turn_cond = PTHREAD_COND_INITIALIZER;

/* Thread data */
typedef struct {
    int color;         /* 0 for White, 1 for Black */
} ThreadData;

/* Send a message to a client */
void send_msg(int sock, const char *msg) {
    send(sock, msg, strlen(msg), 0);
}
static void broadcast_line(const char *line) {
    for (int i = 0; i < 2; i++) {
        send(client_sock[i], line, strlen(line), 0);
    }
}

void broadcast_board() {
    setlocale(LC_ALL, "");

    // 파일 헤더 (열 이름) — 공백 3칸
    broadcast_line("      a   b   c   d   e   f   g   h\n");
    broadcast_line("   ╔═══╦═══╦═══╦═══╦═══╦═══╦═══╦═══╗\n");

    for (int r = 0; r < BOARD_SIZE; r++) {
        char line[BUF_SIZE] = {0};
        int off = snprintf(line, sizeof(line), " %d ║", BOARD_SIZE - r);

        for (int c = 0; c < BOARD_SIZE; c++) {
            char pc = game.board[r][c];
            const char *sym = " ";
            switch (pc) {
                case 'K': sym = "♔"; break;
                case 'Q': sym = "♕"; break;
                case 'R': sym = "♖"; break;
                case 'B': sym = "♗"; break;
                case 'N': sym = "♘"; break;
                case 'P': sym = "♙"; break;
                case 'k': sym = "♚"; break;
                case 'q': sym = "♛"; break;
                case 'r': sym = "♜"; break;
                case 'b': sym = "♝"; break;
                case 'n': sym = "♞"; break;
                case 'p': sym = "♟"; break;
                default: sym = " "; break;
            }
            off += snprintf(line + off, sizeof(line) - off, " %s ║", sym);
        }

        off += snprintf(line + off, sizeof(line) - off, " %d\n", BOARD_SIZE - r);
        broadcast_line(line);

        if (r < BOARD_SIZE - 1)
            broadcast_line("   ╠═══╬═══╬═══╬═══╬═══╬═══╬═══╬═══╣\n");
        else
            broadcast_line("   ╚═══╩═══╩═══╩═══╩═══╩═══╩═══╩═══╝\n");
    }

    // 파일 푸터 (열 이름) — 공백 3칸
    broadcast_line("      a   b   c   d   e   f   g   h\n");
}

/* Handle a client (White or Black) */
void *client_thread(void *arg) {
    ThreadData *td = (ThreadData*)arg;
    int me = td->color;
    int other = 1 - me;
    char buf[BUF_SIZE];

    /* Assign color */
    if (me == WHITE) {
        send_msg(client_sock[me], "You are WHITE. Waiting for Black...\n");
    } else {
        send_msg(client_sock[me], "You are BLACK. Starting game...\n");
    }

    /* Wait for both clients ready */
    if (me == BLACK) {
        /* When Black connects, broadcast initial board */
        broadcast_board();
    }

    /* Game loop */
    while (1) {
        pthread_mutex_lock(&game_mutex);
        /* Wait for our turn */
        while (game.turn != me) {
            if (game.turn == -1) {
                pthread_mutex_unlock(&game_mutex);
                goto game_end;
            }
            pthread_cond_wait(&turn_cond, &game_mutex);
        }

        /* Check for checkmate or stalemate */
        if (is_in_check(&game, me)) {
            if (!has_valid_moves(&game, me)) {
                broadcast_board();
                if (me == WHITE) {
                    send_msg(client_sock[me], "Checkmate! BLACK wins.\n");
                    send_msg(client_sock[other], "Checkmate! BLACK wins.\n");
                } else {
                    send_msg(client_sock[me], "Checkmate! WHITE wins.\n");
                    send_msg(client_sock[other], "Checkmate! WHITE wins.\n");
                }
                game.turn = -1;
                pthread_cond_signal(&turn_cond);
                pthread_mutex_unlock(&game_mutex);
                break;
            }
        } else {
            if (!has_valid_moves(&game, me)) {
                broadcast_board();
                send_msg(client_sock[me], "Stalemate! Game is a draw.\n");
                send_msg(client_sock[other], "Stalemate! Game is a draw.\n");
                game.turn = -1;
                pthread_cond_signal(&turn_cond);
                pthread_mutex_unlock(&game_mutex);
                break;
            }
        }

        /* Prompt for move */
        send_msg(client_sock[me], "Your move: \n");
        pthread_mutex_unlock(&game_mutex);

        /* Read move from client */
        ssize_t bytes_read = recv(client_sock[me], buf, sizeof(buf) - 1, 0);
        if (bytes_read <= 0) {
            perror("recv");
            exit(1);
        }
        buf[bytes_read] = '\0';

        /* Remove newline */
        buf[strcspn(buf, "\r\n")] = '\0';

        pthread_mutex_lock(&game_mutex);
        int sr, sc, dr, dc;
        if (!parse_move(buf, &sr, &sc, &dr, &dc)) {
            send_msg(client_sock[me], "Invalid input format. Use e2e4, etc.\n");
        } else {
            if (!make_move(&game, sr, sc, dr, dc)) {
                send_msg(client_sock[me], "Invalid move. Try again.\n");
            } else {
                /* Move applied, switch turn */
                broadcast_board();
                game.turn = other;
                pthread_cond_signal(&turn_cond);
            }
        }
        pthread_mutex_unlock(&game_mutex);
    }

game_end:
    return NULL;
}

/* Function to run serveo or localhost.run */
void start_reverse_tunnel() {
    printf("Starting reverse tunnel via serveo.net...\n");
    // 비대화형, 백그라운드 실행
    const char *cmd =
      "ssh -o StrictHostKeyChecking=no "
      "-o UserKnownHostsFile=/dev/null "
      "-N "
      "-R 0:localhost:5000 "
      "serveo.net &";
    int rc = system(cmd);
    if (rc != 0) {
        fprintf(stderr, "Failed to establish reverse tunnel (rc=%d).\n", rc);
        exit(1);
    }
    sleep(2);  // 터널 안정화 잠깐 대기
    printf("Reverse tunnel launched in background.\n");
}


int main() {
    int server_sock;
    struct sockaddr_in serv_addr;
    printf("Starting Chess server on port %d...\n", PORT);

    /* Initialize game */
    init_board(&game);

    /* Run reverse SSH tunnel for public access */
    start_reverse_tunnel();

    /* Setup TCP socket */
    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(server_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind");
        exit(1);
    }
    listen(server_sock, 2);
    printf("Waiting for two players to connect...\n");

    /* Accept two clients */
    for (int i = 0; i < 2; i++) {
        client_sock[i] = accept(server_sock, NULL, NULL);
        if (client_sock[i] < 0) {
            perror("accept");
            exit(1);
        }
        printf("Client %d connected.\n", i);
    }

    /* Launch client threads */
    pthread_t th[2];
    ThreadData td[2];
    for (int i = 0; i < 2; i++) {
        td[i].color = i;
        if (pthread_create(&th[i], NULL, client_thread, &td[i]) != 0) {
            perror("pthread_create");
            exit(1);
        }
    }

    /* Wait for threads to finish */
    for (int i = 0; i < 2; i++) {
        pthread_join(th[i], NULL);
    }
    printf("Game over. Server shutting down.\n");
    close(server_sock);
    return 0;
}
