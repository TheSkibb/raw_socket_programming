# Socket programming in C

## Structure

The project consists of three programs:

### mipd

the mip deamon takes MIP messages from a unix socket, sends/resolves MIP-ARP messages and sends MIP messages on raw sockets.

### client

Sends a MIP *"ping"* message

### server

Answers MIP *"ping"* messages with *"pong"*

## Build the project

build all of the components with:

~~~bash
make all
~~~

build a single component with:

~~~
make <client/server/mipd>
~~~
