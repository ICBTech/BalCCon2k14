BalCCon2k14
===========

Presentation held at BalCCon2k14


Building everything
-------------------

First, build the rpi-buildroot.

```sh
git clone --depth 1 git://github.com/ICBTech/rpi-buildroot.git
cd rpi-buildroot
make raspberrypi_defconfig
make
```

The full instructions can be found at https://github.com/ICBTech/rpi-buildroot

Depending on your computer, this can take from 10 minutes to over an hour.

Next, clone this repository and enter the directory

```sh
git clone git://github.com/ICBTech/BalCCon2k14
cd BalCCon2k14
```

In here, you will find directories, named rpi-gpio-kernel-1 to rpi-gpio-kernel-5.

For example, to build the final version, do
```sh
cd rpi-gpio-kernel-5
make
```

If there are no errors, you will have a file named rpi-gpio.ko. This is the kernel module that should be copied to the Raspberry Pi and loaded there.

To load the module, do
```sh
insmod rpi-gpio.ko
```

