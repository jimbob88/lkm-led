/*
 * led.c - Trivial led on/off using an LKM.
 */
#include <linux/atomic.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/minmax.h>
#include <linux/module.h>
#include <linux/printk.h>

#include "led.h"

static struct led_device led_dev = {
	.device_status = ATOMIC_INIT(DEVICE_FREE),
	.counter       = ATOMIC_INIT(0),
};

static struct gpio led_gpio = {
	.gpio  = 535,
	.flags = GPIOF_OUT_INIT_LOW,
	.label = "GREEN LED",
};

static struct file_operations counter_fops = {
	.owner	 = THIS_MODULE,
	.read	 = on_read,
	.write	 = on_write,
	.open	 = on_open,
	.release = on_release,
};

static int __init counter_init(void)
{
	pr_info("In the beginning...\n");

	led_dev.major = register_chrdev(0, DEVICE_NAME, &counter_fops);

	if (led_dev.major < 0) {
		pr_alert("Registering device failed with %d\n", led_dev.major);
		return led_dev.major;
	}

	pr_info("Counter assigned major number %d.\n", led_dev.major);

	led_dev.cls	 = class_create(DEVICE_NAME);
	led_dev.dev_code = MKDEV(led_dev.major, led_dev.minor);
	led_dev.dev = device_create(led_dev.cls, NULL, led_dev.dev_code, NULL,
				    DEVICE_NAME);

	pr_info("Device created on /dev/%s\n", DEVICE_NAME);

	int gpio_request_ret = gpio_request(led_gpio.gpio, led_gpio.label);

	if (gpio_request_ret) {
		pr_err("GPIO Request failed with %d.\n", gpio_request_ret);
		return gpio_request_ret;
	}

	int gpio_direction_ret =
		gpio_direction_output(led_gpio.gpio, led_gpio.flags);

	if (gpio_direction_ret) {
		pr_err("GPIO flags configuration failed with %d.\n",
		       gpio_direction_ret);
		return gpio_direction_ret;
	}

	return 0;
}

static void __exit counter_exit(void)
{
	pr_info("Amen.\n");

	gpio_set_value(led_gpio.gpio, 0);
	gpio_free(led_gpio.gpio);

	device_destroy(led_dev.cls, led_dev.dev_code);
	class_destroy(led_dev.cls);
	unregister_chrdev(led_dev.major, DEVICE_NAME);
}

static ssize_t on_read(struct file *file, char __user *buffer, size_t length,
		       loff_t *offset)
{
	size_t length_of_msg = strlen(led_dev.msg);

	if (*offset >= length_of_msg) {
		return 0;
	}

	if (copy_to_user(buffer, led_dev.msg, length_of_msg)) {
		// Failed to copy the entire message to the buffer.
		return -EFAULT;
	}

	*offset += length_of_msg;

	return length_of_msg;
}

static ssize_t on_write(struct file *file, const char __user *buffer,
			size_t length, loff_t *offset)
{
	size_t length_to_write = min_t(size_t, length, CMD_LEN);

	if (copy_from_user(led_dev.cmd_buffer, buffer, length_to_write)) {
		return -EFAULT;
	}

	switch (led_dev.cmd_buffer[0]) {
	case '0':
		gpio_set_value(led_gpio.gpio, 0);
		pr_info("Switched off LED\n");
		break;
	case '1':
		gpio_set_value(led_gpio.gpio, 1);
		pr_info("Switched on LED\n");
		break;
	default:
		pr_warn("Unknown CMD %d\n", led_dev.cmd_buffer[0]);
		break;
	}

	*offset += length_to_write;

	return length_to_write;
}

static int on_open(struct inode *inode, struct file *file)
{
	if (atomic_cmpxchg(&led_dev.device_status, DEVICE_FREE, DEVICE_BOUND)) {
		pr_debug("Device already bound, busy, failed to open!\n");
		return -EBUSY;
	}

	atomic_inc(&led_dev.counter);
	sprintf(led_dev.msg, "I have been read %d times.\n",
		atomic_read(&led_dev.counter));
	pr_info("%s", led_dev.msg);

	return 0;
}

static int on_release(struct inode *inode, struct file *file)
{
	atomic_set(&led_dev.device_status, DEVICE_FREE);
	return 0;
}

module_init(counter_init);
module_exit(counter_exit);

MODULE_LICENSE("GPL");
