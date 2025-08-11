.PHONY: all clean
FLAGS = -Wall -Werror

BINARIES = client server

DEPENDENCIES = client.c  server.c

all: $(BINARIES)


client: client.c
	gcc $(FLAGS) -o $@ $^

server: server.c
	gcc $(FLAGS) -o $@ $^

clean:
	rm -rf $(BINARIES)
