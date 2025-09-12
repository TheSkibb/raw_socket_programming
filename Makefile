CC = gcc
CFLAGS  = -g -Wall
MIPD = $(CC) $(CFLAGS) mip-daemon.c common.c -o mipd
CLIENT = $(CC) $(CFLAGS) client.c common.c -o client
SERVER = $(CC) $(CFLAGS) server.c common.c -o server

all:
	$(CLIENT) 
	$(SERVER)
	$(MIPD)

mipd:
	$(MIPD)

clean:
	rm -f client server mipd
