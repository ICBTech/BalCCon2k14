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
static int led_pins[] = {9, 8, 7}; // green, yellow, red LEDs


#define INPUT_PINS_NUMBER (ARRAY_SIZE(input_pins))
#define LED_PINS_NUMBER (ARRAY_SIZE(led_pins))


static struct hrtimer blink_timer;
static enum hrtimer_restart blink_timer_timeout(struct hrtimer* timer) {
	static int x = 1;
	x = !x;

	int i;

	for (i = 0; i < LED_PINS_NUMBER; i++) {
		gpio_direction_output(led_pins[i], (i % 2) ^ x);
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


static int __init rpi_gpio_module_init(void) {
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

	hrtimer_init(&blink_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	blink_timer.function = &blink_timer_timeout;

	hrtimer_start(&blink_timer, ktime_set(1, 0), HRTIMER_MODE_REL);

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
}


module_init(rpi_gpio_module_init);
module_exit(rpi_gpio_module_exit);

MODULE_AUTHOR("ICBTech <opensource@icbtech.rs>");
MODULE_DESCRIPTION("Raspberry Pi GPIO Example for BalCCon 2k14");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL");

