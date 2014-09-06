#include <linux/module.h>
#include <mach/gpio.h>
#include <linux/gpio.h>

static int led_pins[] = {9, 8, 7}; // green, yellow, red LEDs

#define LED_PINS_NUMBER (ARRAY_SIZE(led_pins))

static int __init rpi_gpio_module_init(void) {
	printk(KERN_INFO "Hello!");

	int i;
	for (i = 0; i < LED_PINS_NUMBER; i++) {
		gpio_request_one(led_pins[i], GPIOF_DIR_OUT | GPIOF_INIT_LOW, "rpi_gpio_pin");
		gpio_direction_output(led_pins[i], 1);
	}

	return 0;
}


static void __exit rpi_gpio_module_exit(void) {
	printk(KERN_INFO "Bye!");

	int i;
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

