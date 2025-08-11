.PHONY: all clean
FLAGS = -Wall -Werror

BINARIES = server client test

DEPENDENCIES = server.c  client.c  concurrent_test.c

all: $(BINARIES)


server: server.c
	gcc $(FLAGS) -o $@ $^

client: client.c
	gcc $(FLAGS) -o $@ $^

test: concurrent_test.c
	gcc $(FLAGS) -o $@ $^ -lpthread

clean:
	rm -rf $(BINARIES)
