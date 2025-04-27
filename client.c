/* client.c: Chess client using socket send/recv */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUF_SIZE 256

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server-ip> <port>\n", argv[0]);
        exit(1);
    }
    char *server_ip = argv[1];
    int port = atoi(argv[2]);

    int sockfd;
    struct sockaddr_in servaddr;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    if (inet_pton(AF_INET, server_ip, &servaddr.sin_addr) <= 0) {
        perror("inet_pton");
        exit(1);
    }
    if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        perror("connect");
        exit(1);
    }
    printf("Connected to chess server %s:%d\n", server_ip, port);

    char buf[BUF_SIZE];
    while (1) {
        ssize_t n = recv(sockfd, buf, sizeof(buf) - 1, 0);
        if (n <= 0) {
            printf("Connection closed by server.\n");
            break;
        }
        buf[n] = '\0';
        printf("%s", buf);

        /* If server prompts "Your move:", get user input */
        if (strstr(buf, "Your move:") != NULL) {
            char move[10];
            if (fgets(move, sizeof(move), stdin) == NULL) {
                break;
            }
            /* Remove newline */
            move[strcspn(move, "\r\n")] = '\0';
            send(sockfd, move, strlen(move), 0);
        }
    }
    close(sockfd);
    return 0;
}
