/*
 * led.c - Trivial led on/off using an LKM.
 */
#include <linux/atomic.h>
#include <linux/cdev.h>
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
	int ret = 0;

	pr_info("In the beginning...\n");

	if (led_dev.major) {
		led_dev.dev_code = MKDEV(led_dev.major, led_dev.minor);
		ret = register_chrdev_region(led_dev.dev_code, NUM_DEVICES,
					     DEVICE_NAME);
	} else {
		ret = alloc_chrdev_region(&led_dev.dev_code, 0, NUM_DEVICES,
					  DEVICE_NAME);
	}

	if (ret < 0) {
		pr_alert(
			"Failed to get major/minor character allocation, error: %d\n",
			ret);
		return ret;
	}

	pr_info("Major = %d, Minor = %d\n", MAJOR(led_dev.dev_code),
		MINOR(led_dev.dev_code));

	// Create one character device using the supplied major/minor
	// allocations
	cdev_init(&led_dev.cdev, &counter_fops);
	ret = cdev_add(&led_dev.cdev, led_dev.dev_code, 1);
	if (ret) {
		pr_err("Failed to add device\n");
		goto fail1;
	}

	// Creates a struct class pointer (to be filled in device_create)
	led_dev.cls = class_create(DEVICE_NAME);
	if (IS_ERR(led_dev.cls)) {
		pr_err("Failed to create class for device\n");
		ret = PTR_ERR(led_dev.cls);
		goto fail2;
	}

	led_dev.dev = device_create(led_dev.cls, NULL, led_dev.dev_code, NULL,
				    DEVICE_NAME);
	if (IS_ERR(led_dev.dev)) {
		pr_err("Failed to create the device file\n");
		ret = PTR_ERR(led_dev.dev);
		goto fail3;
	}

	pr_info("Device created on /dev/%s\n", DEVICE_NAME);

	ret = gpio_request(led_gpio.gpio, led_gpio.label);

	if (ret) {
		pr_err("GPIO Request failed with %d.\n", ret);
		goto fail4;
	}

	ret = gpio_direction_output(led_gpio.gpio, led_gpio.flags);

	if (ret) {
		pr_err("GPIO flags configuration failed with %d.\n", ret);
		goto fail5;
	}

	return 0;

fail5:
	gpio_free(led_gpio.gpio);

fail4:
	device_destroy(led_dev.cls, led_dev.dev_code);

fail3:
	class_destroy(led_dev.cls);

fail2:
	cdev_del(&led_dev.cdev);

fail1:
	unregister_chrdev_region(led_dev.dev_code, NUM_DEVICES);

	return ret;
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
