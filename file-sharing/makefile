.PHONY = all clean
FLAGS = -Wall -Werror

BINARIES = client server

all: $(BINARIES)

server: server.c
	gcc $(FLAGS) -o $@ $^

client: client.c
	gcc $(FLAGS) -o $@ $^

clean:
	rm -rf $(BINARIES)
