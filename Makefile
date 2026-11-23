# SRMS — student record management
CC ?= gcc
CFLAGS ?= -std=c11 -D_POSIX_C_SOURCE=200809L \
	-Wall -Wextra -Wpedantic -Werror \
	-Wshadow -Wstrict-prototypes -Wmissing-prototypes -Wwrite-strings \
	-Wconversion -Wformat=2 \
	-Iinclude
LDFLAGS ?=

SRC := \
	src/status.c \
	src/ascii.c \
	src/buffer.c \
	src/crc32.c \
	src/student.c \
	src/registry.c \
	src/validate.c \
	src/storage.c \
	src/stats.c \
	src/csv_io.c \
	src/ui.c \
	src/app.c

OBJ := $(SRC:src/%.c=build/%.o)

.PHONY: all test san clean install

all: build/srms

build/srms: build/main.o $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

build/srms_tests: build/test_main.o build/harness.o $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

build/%.o: src/%.c
	@mkdir -p build
	$(CC) $(CFLAGS) -c -o $@ $<

build/main.o: src/main.c
	@mkdir -p build
	$(CC) $(CFLAGS) -c -o $@ $<

build/test_main.o: tests/test_main.c
	@mkdir -p build
	$(CC) $(CFLAGS) -Itests -c -o $@ $<

build/harness.o: tests/harness.c
	@mkdir -p build
	$(CC) $(CFLAGS) -Itests -c -o $@ $<

test: build/srms_tests build/srms
	./build/srms_tests
	./tests/cli_smoke.sh ./build/srms

san:
	$(MAKE) clean
	$(MAKE) test CC=$(CC) CFLAGS="$(CFLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined" LDFLAGS="$(LDFLAGS) -fsanitize=address,undefined"

clean:
	rm -rf build

PREFIX ?= /usr/local
install: build/srms
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 build/srms $(DESTDIR)$(PREFIX)/bin/srms
