CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -Werror -std=c11

.PHONY: all clean check fetch-kernel initramfs kernel vm test-vm
all: build/qauntum-init build/qauntum-shell

build/qauntum-init: src/init/main.c
	mkdir -p build
	$(CC) $(CFLAGS) -static -o $@ $<

build/qauntum-shell: src/shell/main.c
	mkdir -p build
	$(CC) $(CFLAGS) -static -o $@ $<

check: all
	./build/qauntum-init --check
	./build/qauntum-shell --check

fetch-kernel:
	bash tools/fetch-kernel.sh

initramfs: all
	bash tools/build-initramfs.sh

kernel:
	bash tools/build-kernel.sh

vm: kernel initramfs
	bash tools/run-vm.sh

test-vm: kernel initramfs
	python3 tests/vm-smoke.py

clean:
	rm -rf build
