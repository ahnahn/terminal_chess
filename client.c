/* client.c: Chess client using socket send/recv (with hostname resolution) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>          // getaddrinfo
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <locale.h>


#define BUF_SIZE 256

int main(int argc, char *argv[]) {

    setlocale(LC_ALL, "");
    
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server-hostname> <port>\n", argv[0]);
        exit(1);
    }
    char *server_host = argv[1];
    char *port_str    = argv[2];

    // 1) DNS 조회 / 주소 정보 얻기
    struct addrinfo hints, *res, *rp;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;      // IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;    // TCP

    int rc = getaddrinfo(server_host, port_str, &hints, &res);
    if (rc != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rc));
        exit(1);
    }

    int sockfd;
    // 2) 얻은 addrinfo 리스트를 순회하며 연결 시도
    for (rp = res; rp != NULL; rp = rp->ai_next) {
        sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sockfd == -1) continue;

        if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) != -1) {
            // 성공!
            break;
        }
        close(sockfd);
    }

    if (rp == NULL) {
        fprintf(stderr, "Could not connect to %s:%s\n", server_host, port_str);
        freeaddrinfo(res);
        exit(1);
    }
    freeaddrinfo(res);

    printf("Connected to chess server %s:%s\n", server_host, port_str);

    // 3) 게임 루프
    char buf[BUF_SIZE];
    while (1) {
        ssize_t n = recv(sockfd, buf, sizeof(buf) - 1, 0);
        if (n <= 0) {
            printf("Connection closed by server.\n");
            break;
        }
        buf[n] = '\0';
        printf("%s", buf);

        // 서버가 "Your move:"를 보냈으면 사용자 입력 받아 전송
        if (strstr(buf, "Your move:") != NULL) {
            char move[16];
            if (!fgets(move, sizeof(move), stdin)) {
                break;
            }
            move[strcspn(move, "\r\n")] = '\0';
            send(sockfd, move, strlen(move), 0);
        }
    }

    close(sockfd);
    return 0;
}
