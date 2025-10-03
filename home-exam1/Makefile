CC = gcc
CFLAGS  = -g -Wall
libs = ./lib/utils.c ./lib/interfaces.c ./lib/arp_table.c ./lib/sockets.c ./lib/mip.c 
MIPD = $(CC) $(CFLAGS) $(libs) mip_daemon.c  -o $(DIR)mip_daemon
CLIENT = $(CC) $(CFLAGS) $(libs) client.c -o $(DIR)ping_client
SERVER = $(CC) $(CFLAGS) $(libs) server.c -o $(DIR)ping_server
ROUTER = $(CC) $(CFLAGS) $(libs) routing_daemon.c -o $(DIR)routing_daemon
DIR = ./bin/

all:
	$(CLIENT) 
	$(SERVER)
	$(MIPD)
	$(ROUTER)

mipd:
	$(MIPD)

server:
	$(SERVER)

client: 
	$(CLIENT)

router:
	$(ROUTER)

clean:
	find $(DIR) -type f -executable -delete 
