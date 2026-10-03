CC      = gcc
CFLAGS  = -Wall -Wextra -g -std=c11

SRC = src/drrd.c src/stat.c src/print.c src/main.c
OBJ = $(SRC:.c=.o)
BIN = sim

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) $(BIN)

.PHONY: clean