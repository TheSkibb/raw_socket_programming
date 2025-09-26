CC = gcc
CFLAGS  = -g -Wall
libs = ./lib/utils.c ./lib/interfaces.c ./lib/arp_table.c ./lib/sockets.c ./lib/mip.c 
MIPD = $(CC) $(CFLAGS) $(libs) mip_daemon.c  -o mip_daemon
CLIENT = $(CC) $(CFLAGS) $(libs) client.c -o ping_client
SERVER = $(CC) $(CFLAGS) $(libs) server.c -o ping_server

all:
	$(CLIENT) 
	$(SERVER)
	$(MIPD)

mipd:
	$(MIPD)

server:
	$(SERVER)

client: 
	$(CLIENT)

clean:
	rm -f ping_client ping_server mip_daemon
