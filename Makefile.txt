# Makefile for Chess Server and Client

CC = gcc
CFLAGS = -Wall

all: server client

server: server.c chess.c chess.h
	$(CC) $(CFLAGS) server.c chess.c -o server -lpthread

client: client.c
	$(CC) $(CFLAGS) client.c -o client

clean:
	rm -f server client
