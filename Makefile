# Makefile for Chess Server and Client

CC = gcc
CFLAGS = -Wall

all: server client

server: server.c chess.c chess.h
	$(CC) $(CFLAGS) server.c chess.c -o server -lpthread -lcurl

client: client.c
	$(CC) $(CFLAGS) client.c -o client

clean:
	rm -f server client



#gcc server.c -o server -lcurl -lpthread
#sudo apt-get install libcurl4-openssl-dev
