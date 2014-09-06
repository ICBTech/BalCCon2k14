ARCH=arm
CROSS_COMPILE=$(PWD)/../../rpi-buildroot/output/host/usr/bin/arm-linux-
KDIR=$(PWD)/../../rpi-buildroot/output/build/linux-3.12.22
obj-m += rpi-gpio.o
EXTRA_CFLAGS = -Wno-declaration-after-statement

MAKEARCH=make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE)

all:
	$(MAKEARCH) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKEARCH) -C $(KDIR) M=$(PWD) clean

