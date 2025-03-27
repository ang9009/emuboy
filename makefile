CFLAGS=-std=c99 -Wall -Wextra -Werror

all:
	gcc ./src/main.c ./src/cpu.c -o ./out/main -g $(CFLAGS)

clean:
	rm ./out/main

run:
	./out/main $(ARGS)