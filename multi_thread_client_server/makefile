.PHONY: all clean
FLAGS = -Wall # -Werror

BINARIES = server client

DEPENDENCIES = server.c  client.c
all: $(BINARIES)


server: server.c
	gcc $(FLAGS) -o $@ $^

client: client.c
	gcc $(FLAGS) -o $@ $^ -lpthread

clean:
	rm -rf $(BINARIES)
