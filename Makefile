CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -Werror -std=c11

.PHONY: all clean check fetch-kernel
all: build/qauntum-init

build/qauntum-init: src/init/main.c
	mkdir -p build
	$(CC) $(CFLAGS) -static -o $@ $<

check: all
	./build/qauntum-init --check

fetch-kernel:
	bash tools/fetch-kernel.sh

clean:
	rm -rf build
