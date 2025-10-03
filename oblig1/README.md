# Socket programming in C

The MIP network stack

**All functions are documented in the header files.**

## Build the project

build all of the components with:

~~~bash
make all
~~~

This will create three files: mip_daemon, ping_client, ping_server

## build a single

~~~
make <client/server/mipd>
~~~

## usage

The daemon always needs to be started first
~~~
usage mipd [-h] [-d] <socket_upper> <MIP address>
~~~

then the server on some node

~~~
ping_server [-h] <socket_lower>
~~~

then you can run the client on a node

~~~
ping_client [-h] <socket_lower> <message> <destination_host>
~~~

