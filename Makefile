CC      = gcc
CFLAGS  = -Wall -Wextra -g -std=gnu11 -O2 -flto
LDFLAGS = -flto
LDLIBS  = -lm

# планировщик: drrd (по умолчанию), rr, wrr
SCHED   ?= drrd

COMMON  = src/varithreads.o \
          sched/src/$(SCHED).o \
          fiber/src/fiber.o \
          mode/src/cooperative.o

TESTS   = proportional latency starvation dynamic block
TBINS   = $(addprefix test_,$(TESTS))

STRESS  = stress_long stress_max_groups stress_extreme_prio \
          stress_create_destroy stress_block_churn stress_join
SBINS   = $(addprefix test_,$(STRESS))

DEMOS   = lamps
DBINS   = $(addprefix demo_,$(DEMOS))

ALL_BINS = test bench $(TBINS) $(SBINS) $(DBINS)

all: test bench

test: main.o $(COMMON)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

bench: tests/bench_switch.o $(COMMON)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

tests: $(TBINS)
tests-stress: $(SBINS)
tests-all: tests tests-stress
demos: $(DBINS)

test_%: tests/test_%.o $(COMMON)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

demo_%: examples/demo_%.o $(COMMON)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# ── санитайзеры ──

SAN_FLAGS  = -fsanitize=address,undefined -fno-omit-frame-pointer -g
SAN_DIR    = build/san
SAN_COMMON = $(SAN_DIR)/src/varithreads.o \
             $(SAN_DIR)/sched/src/$(SCHED).o \
             $(SAN_DIR)/fiber/src/fiber.o \
             $(SAN_DIR)/mode/src/cooperative.o

san: $(addprefix $(SAN_DIR)/,$(TBINS) $(SBINS))

$(SAN_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(SAN_FLAGS) -c -o $@ $<

$(SAN_DIR)/test_%: tests/test_%.o $(SAN_COMMON)
	$(CC) $(CFLAGS) $(SAN_FLAGS) -o $@ $^ $(LDLIBS)

$(SAN_DIR)/test: main.o $(SAN_COMMON)
	$(CC) $(CFLAGS) $(SAN_FLAGS) -o $@ $^ $(LDLIBS)

# ── valgrind ──

VALGRIND = valgrind --leak-check=full --error-exitcode=1 --track-origins=yes

valgrind: tests
	@for t in $(TBINS); do \
		echo "──── $$t ────"; \
		$(VALGRIND) ./$$t > /dev/null || exit 1; \
	done

# ── прогон ──

run: tests tests-stress
	@for t in $(TBINS) $(SBINS); do \
		echo; echo "════════ $$t ════════"; \
		./$$t || exit 1; \
	done

clean:
	rm -f $(COMMON) main.o tests/*.o examples/*.o $(ALL_BINS)
	rm -rf build

.PHONY: all test bench tests tests-stress tests-all demos san valgrind run clean