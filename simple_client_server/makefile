.PHONY = all clean
FLAGS = -Wall -Werror

BINARIES = s c

all: $(BINARIES)

# targ: source.c code.c
# 	  gcc $(FLAGS) -o $@ $^

s: server.c
	  gcc $(FLAGS) -o $@ $^

c: client.c
	  gcc $(FLAGS) -o $@ $^

clean:
	rm -rf $(BINARIES)
