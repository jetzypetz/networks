# file-sharing program

## description

this is a client server interface for sharing files between two network endpoints

## usage

### build

the program can be built simply with ```make```

### server

```server <port>```

* port: port for receiving tcp/http requests

### client

```client <server address> <server port> <filename> [receive port]```

* server address: ipv4 address of server supplying file

* server port: tcp forwarding address for server

* file: name of requested file

* receive port: *optional* preferred port for receiving data

## code details

### client

```c
// retransmission request loop
place each packet in the corresponding block and keep track of what is missing
while missing data
    while recvfrom() == no data
        if not locked
            failed_requests++
            request
            lock
    failed_requests = 0
    receive packet into receive buffer
    identify pos and bytes
    update missing data
    place in filebuffer
```

data info organised in start end pairs delimiting starts and ends of data filled sections in file buffer
two intervals will collapse to one if the missing middle interval is added
