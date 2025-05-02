/* server.c: Chess server handling two clients over TCP */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include "chess.h"

#define PORT 5000
#define BUF_SIZE 256

int client_sock[2];     /* client sockets for WHITE=0, BLACK=1 */
GameState game;         /* Shared game state */

pthread_mutex_t game_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t turn_cond = PTHREAD_COND_INITIALIZER;

####################################################################################################
#include <sys/stat.h>
#include <curl/curl.h>

struct MemoryStruct {
    char *memory;
    size_t size;
};

// ngrok API 응답 수신용 콜백
static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (!ptr) return 0;
    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
    return realsize;
}

void install_and_setup_ngrok() {
    struct stat st;
    if (stat("ngrok", &st) != 0) {
        printf("ngrok not found. Downloading...\n");
        system("wget https://bin.equinox.io/c/bNyj1mQVY4c/ngrok-stable-linux-amd64.zip -O ngrok.zip");
        system("unzip ngrok.zip");
        system("rm ngrok.zip");
    } else {
        printf("ngrok is already installed.\n");
    }

    char token[256];
    printf("Enter your ngrok authtoken: ");
    fflush(stdout);
    if (fgets(token, sizeof(token), stdin) == NULL) {
        fprintf(stderr, "Failed to read authtoken.\n");
        exit(1);
    }
    token[strcspn(token, "\r\n")] = '\0';

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "./ngrok config add-authtoken %s", token);
    printf("Configuring ngrok...\n");
    system(cmd);

    printf("Starting ngrok TCP tunnel on port 5000...\n");
    system("nohup ./ngrok tcp 5000 > ngrok.log 2>&1 &");

    // ngrok API 주소에서 외부 접속 주소 가져오기
    printf("Waiting for ngrok tunnel to be ready...\n");
    sleep(3); // ngrok이 포트 열 시간

    CURL *curl;
    CURLcode res;
    struct MemoryStruct chunk = { malloc(1), 0 };

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, "http://127.0.0.1:4040/api/tunnels");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
        res = curl_easy_perform(curl);
        if (res == CURLE_OK) {
            char *p = strstr(chunk.memory, "tcp://");
            if (p) {
                char address[128] = {0};
                sscanf(p, "tcp://%127[^\"]", address);
                printf("Your public TCP address: tcp://%s\n", address);
            } else {
                printf("Could not extract ngrok TCP address.\n");
            }
        } else {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        }
        curl_easy_cleanup(curl);
    }
    curl_global_cleanup();
    free(chunk.memory);
}
#####################################################################################################


/* Thread data */
typedef struct {
    int color;         /* 0 for White, 1 for Black */
} ThreadData;

/* Send a message to a client */
void send_msg(int sock, const char *msg) {
    send(sock, msg, strlen(msg), 0);
}

/* Broadcast the current board to both clients */
void broadcast_board() {
    char buf[BUF_SIZE];
    snprintf(buf, sizeof(buf), "\n  a b c d e f g h\n");
    for (int i = 0; i < 2; i++) {
        send(client_sock[i], buf, strlen(buf), 0);
    }

    for (int r = 0; r < BOARD_SIZE; r++) {
        snprintf(buf, sizeof(buf), "%d ", BOARD_SIZE - r);
        for (int c = 0; c < BOARD_SIZE; c++) {
            char piece[4];
            snprintf(piece, sizeof(piece), "%c ", game.board[r][c]);
            strncat(buf, piece, sizeof(buf) - strlen(buf) - 1);
        }
        char endline[16];
        snprintf(endline, sizeof(endline), "%d\n", BOARD_SIZE - r);
        strncat(buf, endline, sizeof(buf) - strlen(buf) - 1);

        for (int i = 0; i < 2; i++) {
            send(client_sock[i], buf, strlen(buf), 0);
        }
    }

    snprintf(buf, sizeof(buf), "  a b c d e f g h\n");
    for (int i = 0; i < 2; i++) {
        send(client_sock[i], buf, strlen(buf), 0);
    }
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
        send_msg(client_sock[me], "Your move: ");
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

int main() {
    int server_sock;
    struct sockaddr_in serv_addr;
    printf("Starting Chess server on port %d...\n", PORT);

    /* Initialize game */
    init_board(&game);

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
