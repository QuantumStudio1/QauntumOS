CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -Werror -std=c11

.PHONY: all clean check
all: build/qauntum-init

build/qauntum-init: src/init/main.c
	mkdir -p build
	$(CC) $(CFLAGS) -static -o $@ $<

check: all
	./build/qauntum-init --check

clean:
	rm -rf build
