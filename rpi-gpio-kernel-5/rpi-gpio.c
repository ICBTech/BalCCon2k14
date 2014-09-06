#include <linux/module.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <mach/platform.h>
#include <mach/gpio.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/hrtimer.h>
#include <linux/delay.h>


#define CLASS_NAME "balccon2k14"
#define DEVICE_NAME "rpi_gpio"


static int input_pins[] = {2, 3}; // left (inner), right (outer)
//static int outputs = {4, 7, 8, 9, 10, 11, 14, 15, 17, 18, 22, 23, 24, 25, 27};
static int led_pins[] = {9, 8, 7}; // green, yellow, red LEDs
static int display_pins[] = {4, 17, 27, 22, 18, 23, 24, 25}; // top left to right, bottom left to right
// 10, 11, 14, 15, 18 are unused

enum display_bits {
	middle      = 0x01,
	upper_left  = 0x02,
	top         = 0x04,
	upper_right = 0x08,
	lower_left  = 0x10,
	bottom      = 0x20,
	lower_right = 0x40,
	dot         = 0x80,
};


static char display_chars[] = {
	top | upper_right | lower_right | bottom | lower_left | upper_left, // 0 = 0x7E
	0x48,                                                               // 1 = 0x48
	top | upper_right | middle | lower_left | bottom,                   // 2 = 0x3D
	top | upper_right | middle | lower_right | bottom,                  // 3 = 0x6D
	upper_left | middle | upper_right | lower_right,                    // 4 = 0x4B
	top | upper_left | middle | lower_right | bottom,                   // 5 = 0x67
	top | upper_left | lower_left | bottom | lower_right | middle,      // 6 = 0x77
	top | upper_right | lower_right,                                    // 7 = 0x4C
	0xFF & ~dot,                                                        // 8 = 0x7F
	top | upper_right | middle | upper_left | lower_right | bottom,     // 9 = 0x6F
	0,                                                                  //   = 0x00 (blank)
};


#define INPUT_PINS_NUMBER (ARRAY_SIZE(input_pins))
#define LED_PINS_NUMBER (ARRAY_SIZE(led_pins))
#define DISPLAY_PINS_NUMBER (ARRAY_SIZE(display_pins))


static struct hrtimer blink_timer;
static enum hrtimer_restart blink_timer_timeout(struct hrtimer* timer) {
	static int x = 1;
	x = !x;

	int i;

	for (i = 0; i < LED_PINS_NUMBER; i++) {
		gpio_direction_output(led_pins[i], (i % 2) ^ x);
	}

	for (i = 0; i < DISPLAY_PINS_NUMBER; i++) {
		gpio_direction_output(display_pins[i], (i % 2) ^ x);
	}

	for (i = 0; i < INPUT_PINS_NUMBER; i++) {
		printk(KERN_INFO "input %d (gpio %d): %d\n", i, input_pins[i], gpio_get_value(input_pins[i]));
	}

	// run again in 1s
	hrtimer_forward_now(timer, ktime_set(1, 0));
	return HRTIMER_RESTART;
}


static irqreturn_t rpi_gpio_irq(int irq, void *dev_id) {
	int gpio = irq_to_gpio(irq);
	int value = gpio_get_value(gpio);

	printk(KERN_INFO "irq: %d, gpio: %d, value: %d\n", irq, gpio, value);

	return IRQ_HANDLED;
}


// lock for exclusive access to the character device
static DEFINE_MUTEX(rpi_gpio_mutex);


// write handler for the blink sysfs attribute
static ssize_t rpi_gpio_blink_write(struct device* dev, struct device_attribute* attr, const char* buf, size_t count) {
	int b;
	if (sscanf(buf, "%d", &b) != 1)
		return -EINVAL;

	if (b) {
		if (!hrtimer_active(&blink_timer)) {
			printk(KERN_INFO "timer not active, starting\n");
			hrtimer_start(&blink_timer, ktime_set(1, 0), HRTIMER_MODE_REL);
		} else {
			printk(KERN_INFO "timer active, not starting\n");
		}
	} else {
		if (hrtimer_active(&blink_timer)) {
			printk(KERN_INFO "timer active, stopping\n");
			hrtimer_cancel(&blink_timer);
		} else {
			printk(KERN_INFO "timer not active, not stopping\n");
		}
	}

	return count;
}


// read handler for the blink sysfs attribute
static ssize_t rpi_gpio_blink_read(struct device* dev, struct device_attribute* attr, char* buf) {
	return sprintf(buf, "%d", hrtimer_active(&blink_timer));
}


static DEVICE_ATTR(blink, (S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR | S_IWGRP | S_IWOTH),
	rpi_gpio_blink_read, rpi_gpio_blink_write);


static struct attribute *rpi_gpio_attributes[] = {
	&dev_attr_blink.attr,
	NULL
};


static struct attribute_group rpi_gpio_attribute_group = {
	.attrs = rpi_gpio_attributes
};


static struct class *rpi_gpio_class;
static struct device *rpi_gpio_device;
static int rpi_gpio_major;


// character device open() handler
static int rpi_gpio_open(struct inode* inode, struct file* filp) {
	// ensure that only one process can open the device at any time
	if (!mutex_trylock(&rpi_gpio_mutex)) {
		printk(KERN_WARNING "another process is accessing the device\n");
		return -EBUSY;
	}

	return 0;
}


// character device close() handler (aka release())
static int rpi_gpio_close(struct inode* inode, struct file* filp) {
	mutex_unlock(&rpi_gpio_mutex);
	return 0;
}


// character device write() handler
static ssize_t rpi_gpio_write(struct file *filp, const char *buff, size_t len, loff_t * off) {
	int i;
	for (i = 0; i < len; i++) {
		int b;
		for (b = 0; b < 8; b++) {
			gpio_direction_output(display_pins[b], (buff[i] >> b) & 0x00000001);
			msleep(5);
		}
		msleep(100);
	}
	return len;
}


/*
// character device read() handler
static ssize_t rpi_gpio_read(struct file *filp, char *buff, size_t len, loff_t * off) {
	printk(KERN_ERR "Sorry, this operation isn't supported.\n");
	return -EINVAL;
}
*/


// character device file operations
static struct file_operations fops = {
	.owner = THIS_MODULE,
	.write = rpi_gpio_write,
//	.read = rpi_gpio_read,
	.open = rpi_gpio_open,
	.release = rpi_gpio_close
};


static int __init rpi_gpio_module_init(void) {
	int retval;

	mutex_init(&rpi_gpio_mutex);

	rpi_gpio_class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(rpi_gpio_class)) {
		printk(KERN_ERR "failed to register device class %s, error: %ld\n", CLASS_NAME, PTR_ERR(rpi_gpio_class));
		return PTR_ERR (rpi_gpio_class);
	}

	rpi_gpio_major = register_chrdev(0, DEVICE_NAME, &fops);
	if (rpi_gpio_major < 0) {
		printk(KERN_ERR "failed to register device %s, error %d\n", DEVICE_NAME, rpi_gpio_major);
		return PTR_ERR(rpi_gpio_device);
	}

	rpi_gpio_device = device_create(rpi_gpio_class, NULL, MKDEV(rpi_gpio_major, 0), NULL, DEVICE_NAME);
	if (IS_ERR(rpi_gpio_device)) {
		printk(KERN_ERR "failed to create device %s/%s\n", CLASS_NAME, DEVICE_NAME);
		return PTR_ERR(rpi_gpio_device);
	}

	retval = sysfs_create_group(&rpi_gpio_device->kobj, &rpi_gpio_attribute_group);
	if (retval) {
		printk(KERN_ERR "failed to create sysfs attribute group, error %d\n", retval);
		return retval;
	}

	int i;
	// inputs
	for (i = 0; i < INPUT_PINS_NUMBER; i++) {
		gpio_request_one(i, GPIOF_DIR_IN, "rpi_gpio_pin");
		if (request_irq(gpio_to_irq(input_pins[i]), rpi_gpio_irq, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "rpi_gpio_irq", NULL)) {
			printk(KERN_ERR "unable to request irq for input %d (gpio %d)\n", i, input_pins[i]);
		}
	}

	// outputs
	for (i = 0; i < LED_PINS_NUMBER; i++) {
		gpio_request_one(led_pins[i], GPIOF_DIR_OUT | GPIOF_INIT_LOW, "rpi_gpio_pin");
	}
	for (i = 0; i < DISPLAY_PINS_NUMBER; i++) {
		gpio_request_one(display_pins[i], GPIOF_DIR_OUT | GPIOF_INIT_LOW, "rpi_gpio_pin");
	}

	hrtimer_init(&blink_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	blink_timer.function = &blink_timer_timeout;

//	hrtimer_start(&blink_timer, ktime_set(1, 0), HRTIMER_MODE_REL);

	for (i = 0; i < ARRAY_SIZE(display_chars); i++) {
		printk(KERN_INFO "showing %d on the display, bitmask 0x%02X\n", i, display_chars[i]);
		int b;
		for (b = 0; b < 8; b++) {
			gpio_direction_output(display_pins[b], (display_chars[i] >> b) & 0x00000001);
			msleep(5);
		}
		msleep(100);
	}

	return 0;
}


static void __exit rpi_gpio_module_exit(void) {
	hrtimer_cancel(&blink_timer);

	int i;
	for (i = 0; i < INPUT_PINS_NUMBER; i++) {
		free_irq(gpio_to_irq(input_pins[i]), NULL);
		gpio_free(input_pins[i]);
	}

	for (i = 0; i < LED_PINS_NUMBER; i++) {
		gpio_direction_output(led_pins[i], 0);
		gpio_free(led_pins[i]);
	}

	for (i = 0; i < DISPLAY_PINS_NUMBER; i++) {
		gpio_direction_output(display_pins[i], 0);
		gpio_free(display_pins[i]);
	}

	sysfs_remove_group(&rpi_gpio_device->kobj, &rpi_gpio_attribute_group);

	device_destroy(rpi_gpio_class, MKDEV(rpi_gpio_major, 0));
	unregister_chrdev(rpi_gpio_major, DEVICE_NAME);

	class_unregister(rpi_gpio_class);
	class_destroy(rpi_gpio_class);
}


module_init(rpi_gpio_module_init);
module_exit(rpi_gpio_module_exit);

MODULE_AUTHOR("ICBTech <opensource@icbtech.rs>");
MODULE_DESCRIPTION("Raspberry Pi GPIO Example for BalCCon 2k14");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL");

