CC      = gcc
CFLAGS  = -Wall -Werror -Wextra -O2 -g -std=c11 -Iinclude

SRC = src/drrd.c src/stat.c src/print.c src/fuzz.c src/trace.c src/main.c

OBJ_STAT   = $(SRC:.c=.o.stat)
OBJ_NOSTAT = $(SRC:.c=.o.nostat)

BIN_STAT   = sim
BIN_NOSTAT = sim-nostat

.PHONY: all clean run run-nostat stat nostat

all: $(BIN_STAT) $(BIN_NOSTAT)

# --- со статистикой ---
$(BIN_STAT): $(OBJ_STAT)
	$(CC) $(CFLAGS) -o $@ $^

%.o.stat: %.c
	$(CC) $(CFLAGS) -DENABLE_STAT -c -o $@ $<

# --- без статистики ---
$(BIN_NOSTAT): $(OBJ_NOSTAT)
	$(CC) $(CFLAGS) -o $@ $^

%.o.nostat: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f src/*.o.stat src/*.o.nostat $(BIN_STAT) $(BIN_NOSTAT)

run: $(BIN_STAT)
	./$(BIN_STAT) 1 1000

run-nostat: $(BIN_NOSTAT)
	./$(BIN_NOSTAT) 1 1000