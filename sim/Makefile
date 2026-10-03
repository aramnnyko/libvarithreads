CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -g -std=c11 -Iinclude

SRC = src/drrd.c src/stat.c src/print.c src/fuzz.c src/trace.c src/main.c
OBJ = $(SRC:.c=.o)
BIN = sim

.PHONY: all clean run

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) $(BIN)

run: $(BIN)
	./$(BIN) 1 1000