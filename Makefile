CC = gcc
CFLAGS = -O0 -Wall -Werror -Werror=vla -std=gnu11 -fsanitize=address
.PHONY: clean
all: server

dict.o: dict.h dict.c
	$(CC) $(CFLAGS) -c $^

server: dict.o server.c
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm *.o
	rm *.h.gch
	rm server