CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -Werror -std=c11

.PHONY: all clean check fetch-kernel initramfs kernel vm test-vm iso test-iso-bios test-iso-uefi
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

iso: kernel initramfs
	bash tools/build-iso.sh

test-iso-bios: iso
	QAUNTUM_ISO="$(CURDIR)/build/QauntumOS-Version-1-x86_64.iso" python3 tests/vm-smoke.py

test-iso-uefi: iso
	@test -n "$(QAUNTUM_OVMF_CODE)" || { echo 'Set QAUNTUM_OVMF_CODE to OVMF_CODE.4m.fd' >&2; exit 1; }
	QAUNTUM_ISO="$(CURDIR)/build/QauntumOS-Version-1-x86_64.iso" python3 tests/vm-smoke.py

clean:
	rm -rf build
