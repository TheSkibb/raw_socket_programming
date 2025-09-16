CC = gcc
CFLAGS  = -g -Wall
libs = ./lib/interfaces.c ./lib/arp_table.c ./lib/raw_sockets.c ./lib/mip.c 
MIPD = $(CC) $(CFLAGS) $(libs) mip-daemon.c  -o mipd
CLIENT = $(CC) $(CFLAGS) $(libs) client.c common.c -o client
SERVER = $(CC) $(CFLAGS) $(libs) server.c common.c -o server

all:
	$(CLIENT) 
	$(SERVER)
	$(MIPD)

mipd:
	$(MIPD)

clean:
	rm -f client server mipd
