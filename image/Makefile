QEMU=/usr/local/Cellar/qemu/2.10.0/bin/qemu-system-i386
CPUS ?= 1
QEMUOPTS = -drive file=./kernel.img,index=0,media=disk,format=raw -serial mon:stdio -vga std -m 256m
QEMUOPTS += -smp $(CPUS)
QEMUOPTS += -drive file=./fs.img,index=1,media=disk,format=raw

run:
	$(QEMU) $(QEMUOPTS)